#pragma once

#include <stdint.h>

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

typedef struct  __attribute__((packed)) IndexHeader {
    char magic[13];
    char endian_mode;
    u8   format_version;
    u8   zlib_window;
    char sha_mode[8];
    u64  embedding_dim;
    char llm_embedding_model[64];
    u64  data_offset;
} IndexHeader;

typedef struct Index {
    int         fd;
    u8          *data;
    u64         filesize,
                chunks_num,
                data_offset;
    IndexHeader header;
} Index;

Index *index_new(const char *fname);
void index_free(Index *index);

u64 index_chunks_num(Index *index);
const char *index_chunk_raw(Index *index, u64 i);


