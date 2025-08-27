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

typedef struct  __attribute__((packed)) IndexHeader_v1 {
    char magic[13];
    char endian_mode;
    u8   format_version;
    u8   zlib_window;
    char sha_mode[8];
    u64  embedding_dim;
    char llm_embedding_model[64];
    u64  data_offset;
} IndexHeader_v1;

typedef struct Index {
    int            fd;
    u8             *data;
    u64            filesize,
                   chunks_num,
                   data_offset;
    IndexHeader_v1 header;
} Index;

//                                1234567890123
static const char *index_magic = "caustic_index";

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

static void header_print_v1(const IndexHeader_v1 *h) {
    assert(h);

    char magic[14] = {0};
    memcpy(magic, h->magic, sizeof(h->magic));

    char sha_mode[32] = {0};
    memcpy(sha_mode, h->sha_mode, sizeof(h->sha_mode));

    char llm_embedding_model[128] = {0};
    memcpy(llm_embedding_model, h->llm_embedding_model, sizeof(h->llm_embedding_model));

    printf("%-20s %s\n", "magic:", magic);
    printf("%-20s %c\n", "endian_mode:", h->endian_mode);
    printf("%-20s %u\n", "format_version:", (unsigned)h->format_version);
    printf("%-20s %u\n", "zlib_window:", (unsigned)h->zlib_window);
    printf("%-20s %s\n", "sha_mode:", sha_mode);
    printf("%-20s %" PRIu64 "\n", "embedding_dim:", (uint64_t)h->embedding_dim);
    printf("%-20s %s\n", "llm_embedding_model:", llm_embedding_model);
    printf("%-20s %" PRIu64 "\n", "data_offset:", (uint64_t)h->data_offset);
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

    if (filesize < sizeof(IndexHeader_v1)) {
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

    IndexHeader_v1 *header = (IndexHeader_v1*)map;
    index.header = *header;
    index.data_offset = header->data_offset;

    if (memcmp(index_magic, header->magic, strlen(index_magic)) != 0) {
        fprintf(stderr, "index_new: magic string is not match\n");
        index_unmap(&index);
        return NULL;
    }

    if (index.data_offset > filesize) {
        fprintf(stderr, "index_new: data_offset more than filesize\n");
        index_unmap(&index);
        return NULL;
    }

    if (index.data_offset < sizeof(IndexHeader_v1)) {
        fprintf(stderr, "index_new: data_offset less than header size\n");
        index_unmap(&index);
        return NULL;
    }

    if (header->format_version == 1)
        header_print_v1(&index.header);

    // проверить хеш blake3
    u64 offsets_num = 1024;
    u64 *offsets = calloc(offsets_num, sizeof(*offsets));
    
    u8 *cur = index.data + index.data_offset,
       *end = index.data + index.filesize;

    while (cur < end) {
    }

    free(offsets);

    Index *index_a = calloc(1, sizeof(*index_a));
    assert(index_a);
    *index_a = index;
    return index_a;
}

void index_free(Index *index) {
    assert(index);
    index_unmap(index);

    free(index);
}

u64 index_chunks_num(Index *index) {
    return 0;
}

const char *index_chunk_raw(Index *index, u64 i) {
    return NULL;
}

