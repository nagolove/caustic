#pragma once

#include <stdint.h>
#include <assert.h>

typedef uint32_t Hash_t;
typedef Hash_t (*HashFunction)(const void *key, int key_len);

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

