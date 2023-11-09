#include "koh_setarr.h"
#include "koh_hashers.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void setintarr_init(struct SetIntArray *h, int cap, HashFunction hasher) {
    assert(h);
    assert(cap > 0);

    h->hashes_arr = calloc(cap, sizeof(h->hashes_arr[0]));
    h->payload_arr = calloc(cap, sizeof(h->payload_arr[0]));
}

void setintarr_shutdown(struct SetIntArray *h) {
    assert(h);
    if (h->hashes_arr) {
        free(h->hashes_arr);
        h->hashes_arr = NULL;
    }
    if (h->payload_arr) {
        free(h->payload_arr);
        h->payload_arr = NULL;
    }
    memset(h, 0, sizeof(*h));
}

void setintarr_add(struct SetIntArray *h, int value) {
    assert(h);
    assert(h->taken < h->cap);

    /*
    Hash_t hash = h->hasher(&value, sizeof(value)); 
    int index = hash % h->cap;
    */

    /*
    while (h->hasher[index].taken)
        index = (index + 1) % h->cap;

    h->arr[index].taken = true;
    h->arr[index].key = malloc(key_len);
    assert(h->arr[index].key);
    memcpy(h->arr[index].key, key, key_len);
    h->arr[index].hash = hash;
    h->arr[index].key_len = key_len;
    h->taken++;

    */
}

bool setintarr_exists(struct SetIntArray *h, int value) {
    return false;
}


