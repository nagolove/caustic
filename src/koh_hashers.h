#pragma once

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
//#include "mum.h"

//typedef uint32_t Hash_t;
typedef uint64_t Hash_t;
typedef Hash_t (*HashFunction)(const void *data, size_t data_len);

extern Hash_t koh_seed;

void koh_hashers_init();

static inline Hash_t koh_hasher_mum(const void *data, size_t len) {
    //return mum_hash(data, len, koh_seed);
    return 0;
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

/*
static inline Hash_t koh_hasher_fnv32(const void *data, int len) {
    assert(data);
    assert(len > 0);
    const char *bytes = (char*)data;
    Hash_t h = 0x811c9dc5;

    for (; len >= 8; len -= 8, bytes += 8) {
        h = (h ^ bytes[0]) * 0x01000193;
        h = (h ^ bytes[1]) * 0x01000193;
        h = (h ^ bytes[2]) * 0x01000193;
        h = (h ^ bytes[3]) * 0x01000193;
        h = (h ^ bytes[4]) * 0x01000193;
        h = (h ^ bytes[5]) * 0x01000193;
        h = (h ^ bytes[6]) * 0x01000193;
        h = (h ^ bytes[7]) * 0x01000193;
    }

    while (len--) {
        h = (h ^ *bytes++) * 0x01000193;
    }

    assert(h != 0);
    return h;
}
*/
