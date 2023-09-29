#pragma once
#include <stdbool.h>
#include "koh_hashers.h"

struct SetIntArray {
    int             *payload_arr, *hashes_arr;
    int             cap, taken;
    HashFunction    hasher;
};

void setintarr_init(struct SetIntArray *h, int cap, HashFunction hasher);
void setintarr_shutdown(struct SetIntArray *h);
void setintarr_add(struct SetIntArray *h, int value);
bool setintarr_exists(struct SetIntArray *h, int value);


