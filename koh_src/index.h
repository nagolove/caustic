#pragma once

#include <stdint.h>

typedef uint64_t    u64;

typedef struct Index Index;
Index *index_new(const char *fname);
void index_free(Index *index);

u64 index_chunks_num(Index *index);
const char *index_chunk_raw(Index *index, u64 i);


