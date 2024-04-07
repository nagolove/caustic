#include "koh_hashers.h"

#include <time.h>

Hash_t koh_seed = 0;

void koh_hashers_init() {
    koh_seed = time(NULL);
}

struct HashFunctionDef koh_hashers[] = {
    { .f = koh_hasher_mum, .fname = "mum" },
    { .f = koh_hasher_fnv64, .fname = "fnv64" },
    { .f = koh_hasher_djb2, .fname = "djb2" },
    { .f = NULL, .fname = NULL },
};
