// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

//typedef uint32_t Hash_t;
typedef uint64_t Hash_t;
typedef Hash_t (*HashFunction)(const void *data, size_t data_len);

typedef struct HashFunctionDef {
    HashFunction f;
    char *fname;
} HashFunctionDef;

extern Hash_t koh_seed;
extern struct HashFunctionDef koh_hashers[];

// Инициализация ГПСЧ для koh_hasher_mum()
void koh_hashers_init();
const char *koh_hashers_name_by_funcptr(void *func_ptr);

// XXX: mum лучше не использовать
Hash_t koh_hasher_mum(const void *data, size_t len);
Hash_t koh_hasher_fnv64(const void *data, size_t len);
// Для текстовых строк
Hash_t koh_hasher_djb2(const void *data, size_t len);
