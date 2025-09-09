// vim: fdm=marker
#pragma once

#include <stdint.h>

typedef uint64_t    u64;

typedef struct Index Index;
Index *index_new(const char *fname);
void index_free(Index *index);

// API 2.0 {{{
u64 index_chunks_num(Index *index);
const char *index_chunk_raw(Index *index, u64 i);
// API 2.0 }}}

// API 3.0 {{{
const char *index_chunk_id(Index *index, u64 i);
// Возвращает строку длиной 4 символа - u32 число
const char *index_chunk_id_hash(Index *index, u64 i);
const char *index_chunk_file(Index *index, u64 i);

// Возвращает -1 при отсутствии поля
u64 index_chunk_line_start(Index *index, u64 i);
// Возвращает -1 при отсутствии поля
u64 index_chunk_line_end(Index *index, u64 i);

const char *index_chunk_text(Index *index, u64 i);
const char *index_chunk_text_zlib(Index *index, u64 i);
const char *index_chunk_embedding(Index *index, u64 i);
// API 3.0 }}}

