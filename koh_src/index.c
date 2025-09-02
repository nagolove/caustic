#include "index.h"

#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "blake3.h"
#include "lua.h"
#include "lauxlib.h"

typedef int64_t     i64;
typedef uint64_t    u64;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int16_t     i16;
typedef uint16_t    u16;
typedef int8_t      i8;
typedef uint8_t     u8;
typedef float       f32;
typedef double      f64;

typedef struct __attribute__((packed)) IndexHeaderBase {
    char magic[14];
    char endian_mode;
    u8   format_version;
} IndexHeaderBase;

/*
typedef struct  __attribute__((packed)) IndexHeader_v1 {
    IndexHeaderBase     base;
    u8                  zlib_window;
    char                sha_mode[8];
    u64                 embedding_dim;
    char                llm_embedding_model[64];
    u64                 data_offset;
    // сюда можно добавлять другие поля
} IndexHeader_v1;
*/

typedef struct __attribute__((packed)) IndexHeader_v2 {
    IndexHeaderBase     base;                // magic[13], endian_mode, format_version=2
    u8                  zlib_window;         // окно zlib
    char                sha_mode[8];         // "blake3\0", "sha256\0" и т.п.
    u64                 embedding_dim;       // размерность векторов
    char                llm_embedding_model[64]; // имя модели (null-terminated, <=63 символов)
    u64                 data_offset;         // смещение начала данных
    // сюда можно добавлять новые поля в будущем
} IndexHeader_v2;

typedef struct Index {
    int            fd;
    // mmap() файла с чанками
    u8             *data;
    //u64            *offsets;
    const char     **strings;
    u64            filesize,
                   chunks_num,
                   data_offset;
    IndexHeader_v2 header;
} Index;

//                                1234567890123
static const char *index_magic = "caustic_index\0";

static void index_unmap(Index *index) {
    assert(index);
    if (index->data) {
        munmap(index->data, index->filesize);
        index->data = NULL;
    }
    if (index->fd >= 0) {
        close(index->fd);
        index->fd = -1;
    }
}

static void header_print_v2(const IndexHeader_v2 *h) {
    assert(h);

    printf("header_print_v2:\n");

    char magic[14] = {0};
    memcpy(magic, h->base.magic, sizeof(h->base.magic));

    char sha_mode[32] = {0};
    memcpy(sha_mode, h->sha_mode, sizeof(h->sha_mode));

    char llm_embedding_model[128] = {0};
    size_t sz = sizeof(h->llm_embedding_model);
    memcpy(llm_embedding_model, h->llm_embedding_model, sz);

    printf("%-20s '%s'\n", "magic:", magic);
    printf("%-20s '%c'\n", "endian_mode:", h->base.endian_mode);
    printf("%-20s %u\n", "format_version:", (unsigned)h->base.format_version);
    printf("%-20s %u\n", "zlib_window:", (unsigned)h->zlib_window);
    printf("%-20s %s\n", "sha_mode:", sha_mode);
    printf("%-20s %" PRIu64 "\n", "embedding_dim:", h->embedding_dim);
    printf("%-20s %s\n", "llm_embedding_model:", llm_embedding_model);
    printf("%-20s %" PRIu64 "\n", "data_offset:", h->data_offset);
}

/*
static void header_print_v1(const IndexHeader_v1 *h) {
    assert(h);

    char magic[14] = {0};
    memcpy(magic, h->base.magic, sizeof(h->base.magic));

    char sha_mode[32] = {0};
    memcpy(sha_mode, h->sha_mode, sizeof(h->sha_mode));

    char llm_embedding_model[128] = {0};
    size_t sz = sizeof(h->llm_embedding_model);
    memcpy(llm_embedding_model, h->llm_embedding_model, sz);

    printf("%-20s %s\n", "magic:", magic);
    printf("%-20s %c\n", "endian_mode:", h->base.endian_mode);
    printf("%-20s %u\n", "format_version:", (unsigned)h->base.format_version);
    printf("%-20s %u\n", "zlib_window:", (unsigned)h->zlib_window);
    printf("%-20s %s\n", "sha_mode:", sha_mode);
    printf("%-20s %" PRIu64 "\n", "embedding_dim:", h->embedding_dim);
    printf("%-20s %s\n", "llm_embedding_model:", llm_embedding_model);
    printf("%-20s %" PRIu64 "\n", "data_offset:", h->data_offset);
}
*/

char *str_slice_end_alloc(const char *s, int n) {
    assert(n >= 0);
    assert(s);
    int len = strlen(s);
    if (n >= len)
        return NULL;
    char *buf_2 = calloc(n + 1, sizeof(char));
    memcpy(buf_2, s + strlen(s) - n, sizeof(char) * n);
    return buf_2;
}

char *str_slice_beg_alloc(const char *s, int n) {
    assert(n >= 0);
    assert(s);
    int len = strlen(s);
    if (n >= len)
        return NULL;
    char *buf_2 = calloc(n + 1, sizeof(char));
    memcpy(buf_2, s, sizeof(char) * n);
    return buf_2;
}

char *bytes_to_hex_alloc(const uint8_t *bytes, size_t len) {
    // каждый байт = 2 символа hex + финальный '\0'
    char *hex = malloc(len * 2 + 1);
    if (!hex) return NULL;

    for (size_t i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]); // нижний регистр
    }
    hex[len * 2] = '\0';
    return hex;
}

Index *index_new(const char *fname) {
    assert(fname);

    int fd = open(fname, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return NULL;
    }

    // получаем размер файла
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return NULL;
    }
    size_t filesize = st.st_size;

    if (filesize < sizeof(IndexHeader_v2)) {
        fprintf(stderr, "index_new: file '%s' too small\n", fname);
        close(fd);
        return NULL;
    }

    // отображаем файл в память
    void *map = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }

    Index index = {};

    index.fd = fd;
    index.data = map;
    index.filesize = filesize;

    IndexHeader_v2 *header = (IndexHeader_v2*)map;
    index.header = *header;
    index.data_offset = header->data_offset;

    //printf("index_new: base.magic '%s'\n", header->base.magic);

    if (memcmp(index_magic, header->base.magic, strlen(index_magic)) != 0) {
        fprintf(stderr, "index_new: magic string is not match\n");
        index_unmap(&index);
        return NULL;
    }

    //header_print_v2(&index.header);

    switch (header->base.format_version) {
        case 2: header_print_v2(&index.header); break;
        default: 
                fprintf(
                    stderr, "index_new: '%d' is unsupported format version\n",
                    header->base.format_version
                );
                index_unmap(&index);
                return NULL;
    }

    if (index.data_offset > filesize) {
        fprintf(stderr, "index_new: data_offset more than filesize\n");
        index_unmap(&index);
        return NULL;
    }

    if (index.data_offset < sizeof(IndexHeader_v2)) {
        fprintf(stderr, "index_new: data_offset less than header size\n");
        index_unmap(&index);
        return NULL;
    }

    if (tolower(header->base.endian_mode) != 'l') {
        fprintf(stderr, "index_new: only little endian supported\n");
        index_unmap(&index);
        return NULL;
    }

    u64 strings_cap = 1024;
    const char **strings = calloc(strings_cap, sizeof(*strings));
    u64 strings_num = 0;
   
    int blake3_sz = 64;
    u8 *cur = index.data + index.data_offset,
       *end = index.data + index.filesize - blake3_sz;

    blake3_hasher b3h = {};
    blake3_hasher_init(&b3h);
    int sz = end - cur;
    blake3_hasher_update(&b3h, cur, sz);

    u8 hash_computed_bin[BLAKE3_OUT_LEN] = {};
    u8 hash_computed_hex[BLAKE3_OUT_LEN * 2 + 1] = {};
    u8 hash_readed_hex[BLAKE3_OUT_LEN * 2 + 1] = {};

    blake3_hasher_finalize(&b3h, hash_computed_bin, BLAKE3_OUT_LEN);
    for (int i = 0; i < BLAKE3_OUT_LEN; i++) {
        sprintf(
            (char*)hash_computed_hex + i * 2, "%02x", hash_computed_bin[i]
        ); // нижний регистр
    }
    //hex[len * 2] = '\0';

    int i = 0;
    while (cur < end) {
        if (strings_num + 1 == strings_cap) {
            strings_cap *= 1.5;
            void *p = realloc(strings, strings_cap * sizeof(*strings));
            if (!p) {
                fprintf(
                    stderr,
                    "index_new: could not realloc memory for offsets array\n"
                );
                index_unmap(&index);
                free(strings);
                return NULL;
            }
            strings = p;
        }
        
        u64 size = *(u64*)cur;
        cur += sizeof(size);
       
        const char *str = (const char*)cur;
        strings[strings_num++] = str;

        index.chunks_num++;

        //printf("index_new: size %lu\n", size);

        // Показываю куски первых нескольких строк данных
        if (i++ < 5) {
            char *slice_beg = str_slice_beg_alloc(str, 150);
            char *slice_end = str_slice_end_alloc(str, 150);
            if (slice_beg) {
                printf("index_new: beg '%s'\n", slice_beg);
                free(slice_beg);
            }
            if (slice_end) {
                printf("index_new: end '%s'\n", slice_end);
                free(slice_end);
            }
        }

        cur += size;
    }

    memcpy(hash_readed_hex, cur, BLAKE3_OUT_LEN * 2);
    printf("index_new: hash_computed_hex '%s'\n", hash_computed_hex);
    printf("index_new: hash_readed_hex   '%s'\n", hash_readed_hex);

    _Static_assert(
        sizeof(hash_computed_hex) == sizeof(hash_readed_hex),
        "different hash buffer sizes"
    );
    if (strcmp((char*)hash_computed_hex, (char*)hash_readed_hex) != 0) {
        fprintf(
            stderr, "index_new: computed hash not is different to readed\n"
        );
        index_unmap(&index);
        free(strings);
        return NULL;
    }

    printf("index_new: reading done\n");
    printf("index_new: chunks_num %lu\n", index.chunks_num);

    Index *index_a = calloc(1, sizeof(*index_a));
    assert(index_a);
    *index_a = index;
    index_a->strings = strings;
    index_a->chunks_num = strings_num;
    return index_a;
}

void index_free(Index *index) {
    assert(index);
    index_unmap(index);
    if (index->strings) {
        free(index->strings);
        index->strings = NULL;
    }
    free(index);
}

u64 index_chunks_num(Index *index) {
    assert(index);
    return index->chunks_num;
}

// Индексирование с нуля
const char *index_chunk_raw(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr, "index_chunk_raw: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->strings[i];
}

