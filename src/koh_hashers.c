// vim: set colorcolumn=85
// vim: fdm=marker
#include "koh_hashers.h"

//#include "mum.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define XXH_INLINE_ALL
#include "xxhash.h"

Hash_t koh_seed = 0;

void koh_hashers_init() {
    koh_seed = time(NULL);
}

struct HashFunctionDef koh_hashers[] = {
    { .f = koh_hasher_fnv64, .fname = "fnv64" },
    { .f = koh_hasher_djb2, .fname = "djb2" },
    //{ .f = koh_hasher_mum, .fname = "mum" },
    { .f = NULL, .fname = NULL },
};

const char *koh_hashers_name_by_funcptr(void *func_ptr) {
    for (int i = 0; koh_hashers[i].f; i++) {
        if (koh_hashers[i].f == func_ptr)
            return koh_hashers[i].fname;
    }
    return NULL;
}

/*
Hash_t koh_hasher_mum(const void *data, size_t len) {
    // {{{
    assert(koh_seed != 0);
    return mum_hash(data, len, koh_seed);
    // }}}
}
*/

Hash_t koh_hasher_fnv64(const void *data, size_t len) {
    // {{{
	size_t i;
	uint64_t h = 0xcbf29ce484222325ull;
	const char *c = (const char*)data;

	for (i = 0; i < len; i++) {
		h = (h * 0x100000001b3ll) ^ c[i];
	}

	return h;
    // }}}
}

Hash_t koh_hasher_djb2(const void *data, size_t len) {
    // {{{
    // NOTE: Обрати внимание на размер типа hash в 32 бита
    uint32_t hash = 5381;
    const char *str = data;

    for (size_t i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + str[i]; // hash * 33 + c 

    return hash;
    // }}}
}

Hash_t koh_hasher_xxhash(const void *data, size_t len) {
    printf("koh_hasher_xxhash:\n");
    return XXH3_64bits(data, len);
}
