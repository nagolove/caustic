// vim: fdm=marker
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

//#define KOH

#ifdef KOH
#include "koh_strbuf.h"
#include "koh_table.h"
#endif

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

typedef struct __attribute__((packed)) IndexHeader_v3 {
    IndexHeaderBase     base;                // magic[13], endian_mode, format_version=2
    u8                  zlib_window;         // окно zlib
    char                sha_mode[8];         // "blake3\0", "sha256\0" и т.п.
    u64                 embedding_dim;       // размерность векторов
    char                llm_embedding_model[64]; // имя модели (null-terminated, <=63 символов)
    u64                 data_offset;         // смещение начала данных
    // сюда можно добавлять новые поля в будущем
} IndexHeader_v3;

typedef struct Index {
    int            fd;
    // mmap() файла с чанками
    u8             *data;
    //u64            *offsets;
    const char     **strings;

    const char     **ids,           // 1
                   **id_hashes,     // 2
                   **files,         // 3
                   **texts,         // 4
                   **texts_zlib,    // 5
                   **embeddings;    // 6
    u64            *line_starts,    // 7
                   *line_ends;      // 8
    // Для удобного realloc() в цикле. Содержимое дублирует ids, id_hashes, ...
    void           **pointers[8];
    int            pointers_sz[8];

    u64            filesize,
                   chunks_num,
                   data_offset;
    //IndexHeader_v2 header;
    IndexHeader_v3 header;

#ifdef KOH
    HTable         *id_hash2str;
#endif
} Index;

//                                1234567890123
static const char *index_magic = "caustic_index\0";

static void index_pointers_free(Index *index) {
    assert(index);
    assert(index->pointers);
    int pointers_num = sizeof(index->pointers) / sizeof(index->pointers[0]);
    for (int i = 0; i < pointers_num; i++) {
        if (*index->pointers[i]) {
            free(*index->pointers[i]);
            *index->pointers[i] = NULL;
        }
    }
}

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

void on_remove_str(
    const void *key, int key_len, void *value, int value_len, void *userdata
) {
    assert(value);
    free(value);
}

static void header_print_v3(const IndexHeader_v3 *h) {
    assert(h);

    printf("header_print_v3:\n");

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
*/

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

/*
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

    // отображаем файл в память только на чтение
    void *map = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }

    Index index = {
        .fd = fd,
        .data = map,
        .filesize = filesize,
    };

    IndexHeader_v3 *header = (IndexHeader_v3*)map;
    index.header = *header;
    index.data_offset = header->data_offset;

    //printf("index_new: base.magic '%s'\n", header->base.magic);

    if (memcmp(index_magic, header->base.magic, strlen(index_magic)) != 0) {
        fprintf(stderr, "index_new: magic string is not match\n");
        index_unmap(&index);
        return NULL;
    }

    switch (header->base.format_version) {
        case 2: header_print_v3(&index.header); break;
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
*/

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

    if (filesize < sizeof(IndexHeader_v3)) {
        fprintf(stderr, "index_new: file '%s' too small\n", fname);
        close(fd);
        return NULL;
    }

    // отображаем файл в память только на чтение
    void *map = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }

    Index *index = calloc(1, sizeof(*index));
    assert(index);
    *index = (Index){
        .fd = fd,
        .data = map,
        .filesize = filesize,
    };

    IndexHeader_v3 *header = (IndexHeader_v3*)map;
    index->header = *header;
    index->data_offset = header->data_offset;

    //printf("index_new: base.magic '%s'\n", header->base.magic);

    if (memcmp(index_magic, header->base.magic, strlen(index_magic)) != 0) {
        fprintf(stderr, "index_new: magic string is not match\n");
        index_unmap(index);
        free(index);
        return NULL;
    }

    switch (header->base.format_version) {
        case 3: header_print_v3(&index->header); break;
        default: 
            fprintf(
                stderr, "index_new: '%d' is unsupported format version\n",
                header->base.format_version
            );
            index_unmap(index);
            free(index);
            return NULL;
    }

    if (index->data_offset > filesize) {
        fprintf(stderr, "index_new: data_offset more than filesize\n");
        index_unmap(index);
        free(index);
        return NULL;
    }

    if (index->data_offset < sizeof(IndexHeader_v3)) {
        fprintf(stderr, "index_new: data_offset less than header size\n");
        index_unmap(index);
        free(index);
        return NULL;
    }

    if (tolower(header->base.endian_mode) != 'l') {
        fprintf(stderr, "index_new: only little endian supported\n");
        index_unmap(index);
        free(index);
        return NULL;
    }

    u64 capacity = 1024;
    //const char **strings = calloc(capacity, sizeof(*strings));
    u64 num = 0;
   
    int blake3_sz = 64;

    u8 *cur = index->data + index->data_offset,
       *end = index->data + index->filesize - blake3_sz;

    // HASHING {{{
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
    // }}}

    // pointers {{{
    index->pointers[0] = (void*)&index->ids;
    index->pointers_sz[0] = sizeof(index->ids[0]);
    index->pointers[1] = (void*)&index->id_hashes;
    index->pointers_sz[1] = sizeof(index->id_hashes[0]);
    index->pointers[2] = (void*)&index->files;
    index->pointers_sz[2] = sizeof(index->files[0]);
    index->pointers[3] = (void*)&index->texts;
    index->pointers_sz[3] = sizeof(index->texts[0]);
    index->pointers[4] = (void*)&index->texts_zlib;
    index->pointers_sz[4] = sizeof(index->texts_zlib[0]);
    index->pointers[5] = (void*)&index->embeddings;
    index->pointers_sz[5] = sizeof(index->embeddings[0]);
    index->pointers[6] = (void*)&index->line_starts;
    index->pointers_sz[6] = sizeof(index->line_starts[0]);
    index->pointers[7] = (void*)&index->line_ends;
    index->pointers_sz[7] = sizeof(index->line_ends[0]);
    // }}}
    
    int pointers_num = sizeof(index->pointers) / sizeof(index->pointers[0]);

    for (int i = 0; i < pointers_num; i++) {
        void *p = calloc(capacity, index->pointers_sz[i]);
        assert(p);
        *index->pointers[i] = p;
    }

    int i = 0;
    while (cur < end) {

        if (num + 1 == capacity) {
            capacity *= 1.5;

            for (int i = 0; i < pointers_num; i++) {
                size_t sz = capacity * index->pointers_sz[i];
                printf(
                    "index_new: realloc array numbe %d for %zu bytes\n",
                    i, sz
                );

                void *p = realloc(*index->pointers[i], sz);
                if (!p) {
                    fprintf(
                        stderr,
                        "index_new: could not realloc "
                        "memory for offsets array\n"
                    );
                    index_unmap(index);
                    index_pointers_free(index);
                    return NULL;
                }

                *index->pointers[i] = p;
            }
        }
        
        // XXX: Общего размер блока отсутствует.
        //u64 size = *(u64*)cur;
        //cur += sizeof(size);

        /*
        hasher = w_str(f, c.id, hasher)
        hasher = w_str(f, c.id_hash, hasher)
        hasher = w_str(f, c.file, hasher)
        hasher = w_u64(f, c.line_start, hasher)
        hasher = w_u64(f, c.line_end, hasher)
        hasher = w_str(f, c.text, hasher)
        hasher = w_str(f, c.text_zlib, hasher)

        assert(type(c.embedding) == 'table')
        -- XXX: Как лучше записывать embedding что-бы не было 
        -- расжатия из строки и создания таблицы?
        local embedding = serpent.dump(c.embedding)
        hasher = w_str(f, embedding, hasher)
        */
       
        u64 len = 0;

        len = *(u64*)cur;
        cur += sizeof(len);
        index->ids[num] = (char*)cur;
        cur += len + 1;

        len = *(u64*)cur;
        cur += sizeof(len);
        index->id_hashes[num] = (char*)cur;
        cur += len + 1;

        len = *(u64*)cur;
        cur += sizeof(len);
        index->files[num] = (char*)cur;
        cur += len + 1;

        index->line_starts[num] = *(u64*)cur;
        cur += sizeof(u64);

        index->line_ends[num] = *(u64*)cur;
        cur += sizeof(u64);

        len = *(u64*)cur;
        cur += sizeof(len);
        index->texts[num] = (char*)cur;
        cur += len + 1;

        len = *(u64*)cur;
        cur += sizeof(len);
        index->texts_zlib[num] = (char*)cur;
        cur += len + 1;

        len = *(u64*)cur;
        cur += sizeof(len);
        index->embeddings[num] = (char*)cur;
        cur += len + 1;

        num++;
        //index.chunks_num++;

        //printf("index_new: size %lu\n", size);

        // Показываю куски первых нескольких строк данных
        if (i++ < 5) {

            /*
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
            */

        }
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
        index_unmap(index);
        index_pointers_free(index);
        return NULL;
    }

    printf("index_new: reading done\n");
    printf("index_new: chunks_num %lu\n", index->chunks_num);

    //index_a->strings = strings;
    index->chunks_num = num;

#ifdef KOH
    index->id_hash2str = htable_new(&(HTableSetup) {
        .cap = 3000,
        .f_on_remove = on_remove_str,
    });
#endif

    return index;
}

void index_free(Index *index) {
    assert(index);
    index_unmap(index);
    index_pointers_free(index);

#ifdef KOH
    htable_free(index->id_hash2str);
#endif

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

#ifdef KOH
    const char *key = index_chunk_id_hash(index, i);
    char *s = htable_get_s(index->id_hash2str, key, NULL);

    if (!s) {
        StrBuf b = strbuf_init(NULL);

        strbuf_addf(&b, "%s", index_chunk_id_hash(index, i));
        strbuf_addf(&b, "%s", index_chunk_file(index, i));
        strbuf_addf(&b, "%lu", index_chunk_line_start(index, i));
        strbuf_addf(&b, "%lu", index_chunk_line_end(index, i));
        strbuf_addf(&b, "%s", index_chunk_text(index, i));
        strbuf_addf(&b, "%s", index_chunk_text_zlib(index, i));
        strbuf_addf(&b, "%s", index_chunk_embedding(index, i));

        assert(index->id_hash2str);

        s = strbuf_concat_alloc(&b, "\n");
        htable_add_s(
            index->id_hash2str,
            index_chunk_id_hash(index, i),
            s, sizeof(s)
        );

        strbuf_shutdown(&b);
        // */
    }

    printf("index_chunk_raw: s '%s'\n", s);

    return s;
#else
    fprintf(stderr, "index_chunk_raw: this function is deprecated\n");
    return "HUI";
#endif
}

const char *index_chunk_id(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr, "index_chunk_id: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->ids[i];
}

const char *index_chunk_id_hash(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr, "index_chunk_id_hash: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->id_hashes[i];
}

const char *index_chunk_file(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr, "index_chunk_id_file: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->files[i];
}

u64 index_chunk_line_start(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr,
            "index_chunk_id_line_start: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->line_starts[i];
}

u64 index_chunk_line_end(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr,
            "index_chunk_id_line_end: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->line_ends[i];
}

const char *index_chunk_text(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr, "index_chunk_id_text: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->texts[i];
}

const char *index_chunk_text_zlib(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr,
            "index_chunk_id_text_zlib: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->texts_zlib[i];
}

const char *index_chunk_embedding(Index *index, u64 i) {
    assert(index);
    if (i >= index->chunks_num) {
        fprintf(
            stderr,
            "index_chunk_id_embedding: i value is too large (%ld/%ld)\n",
            i, index->chunks_num
        );
    }
    return index->embeddings[i];
}

