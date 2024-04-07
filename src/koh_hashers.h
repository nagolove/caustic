#pragma once

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include "mum.h"

//typedef uint32_t Hash_t;
typedef uint64_t Hash_t;
typedef Hash_t (*HashFunction)(const void *data, size_t data_len);

struct HashFunctionDef {
    HashFunction f;
    char *fname;
};

extern Hash_t koh_seed;
extern struct HashFunctionDef koh_hashers[];

void koh_hashers_init();

static inline Hash_t koh_hasher_mum(const void *data, size_t len) {
    assert(koh_seed != 0);
    return mum_hash(data, len, koh_seed);
}

static inline Hash_t koh_hasher_fnv64(const void *data, size_t len) {
	size_t i;
	uint64_t h = 0xcbf29ce484222325ull;
	const char *c = (char*)data;

	for (i = 0; i < len; i++) {
		h = (h * 0x100000001b3ll) ^ c[i];
	}

	return h;
}

static inline Hash_t koh_hasher_djb2(const void *data, size_t len) {
    Hash_t hash = 5381;
    const char *str = data;

    for (size_t i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + str[i]; // hash * 33 + c 

    return hash;
}

/*
static inline Hash_t koh_hasher_djb2(const char *str) {
    Hash_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c 

    return hash;
}
*/
