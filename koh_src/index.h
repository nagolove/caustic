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

typedef struct Index Index;
Index *index_new(const char *fname);
void index_free(Index *index);

u64 index_chunks_num(Index *index);
const char *index_chunk_raw(Index *index, u64 i);


