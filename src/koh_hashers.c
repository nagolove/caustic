#include "koh_hashers.h"

#include <time.h>

Hash_t koh_seed;

void koh_hashers_init() {
    koh_seed = time(NULL);
}

