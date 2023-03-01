#include "koh_strset.h"

#include "koh_hashers.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

struct Bucket {
    char   *key;
    bool   taken;
    Hash_t hash;
};

typedef struct StrSet {
    struct Bucket *arr;
    int cap, taken;
} StrSet;

static uint32_t hasher_djb2(const char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

StrSet *strset_new() {
    StrSet *set = calloc(1, sizeof(StrSet));
    assert(set);
    set->cap = 11;
    set->arr = calloc(set->cap, sizeof(set->arr[0]));
    assert(set->arr);
    return set;
}

static int _strset_get(StrSet *set, const char *key) {
    assert(set);

    if (!key)
        return -1;

    int index = hasher_djb2(key) % set->cap;
    for (int i = 0; i < set->cap; i++) {
        if (!set->arr[index].taken)
            break;
        if (!strcmp(set->arr[index].key, key))
            break;
        index = (index + 1) % set->cap;
    }

    if (set->arr[index].taken && !strcmp(set->arr[index].key, key))
        return index;

    return -1;
}

bool strset_exist(StrSet *set, const char *key) {
    return _strset_get(set, key) != -1;
}

void strset_extend(StrSet *set) {
    assert(set);
    struct Bucket *old_arr = set->arr;
    int old_cap = set->cap;

    set->cap = set->cap * 2 + 1;
    set->arr = calloc(set->cap, sizeof(set->arr[0]));
    assert(set->arr);
    set->taken = 0;
    for (int j = 0; j < old_cap; j++) {
        if (old_arr[j].taken) {
            strset_add(set, old_arr[j].key);
            free(old_arr[j].key);
        }
    }

    free(old_arr);
}

void strset_add(StrSet *set, const char *key) {
    assert(set);
    if (!key) return;

    if (strset_exist(set, key))
        return;

    if (set->taken >= set->cap * 0.7)
        strset_extend(set);

    assert(set->taken < set->cap);

    Hash_t hash = hasher_djb2(key); 
    int index = hash % set->cap;

    while (set->arr[index].taken)
        index = (index + 1) % set->cap;

    set->arr[index].taken = true;
    set->arr[index].key = strdup(key);
    set->arr[index].hash = hash;
    set->taken++;
}

void strset_clear(StrSet *set) {
    for (int k = 0; k < set->cap; k++) {
        if (set->arr[k].taken) {
            set->arr[k].taken = false;
            if (set->arr[k].key)
                free(set->arr[k].key);
        }
    }
    set->taken = 0;
    assert(set);
}

void strset_free(StrSet *set) {
    assert(set);
    for (int k = 0; k < set->cap; k++)
        if (set->arr[k].taken)
            free(set->arr[k].key);
    if (set->arr)
        free(set->arr);
    free(set);
}

static void rec_shift(StrSet *set, int index, int hashi) {
    int initial_index = index;

    index = (index + 1) % set->cap;
    for (int i = 0; i < set->cap; i++) {
        if (!set->arr[index].taken) {
            break;
        }

        if (set->arr[index].hash % set->cap <= hashi) {
            set->arr[initial_index] = set->arr[index];
            set->arr[index].taken = false;
            rec_shift(set, index, hashi + 1);
        }

        index = (index + 1) % set->cap;
    }
}

void _strset_remove(StrSet *set, int remove_index) {
    assert(set);
    assert(remove_index >= 0 && remove_index < set->cap);

    int hashi = set->arr[remove_index].hash % set->cap;
    if (set->arr[remove_index].key) {
        free(set->arr[remove_index].key);
        //set->arr[remove_index].key = NULL;
        memset(&set->arr[remove_index], 0, sizeof(set->arr[0]));
    }

    rec_shift(set, remove_index, hashi);
    --set->taken;
}

void strset_remove(StrSet *set, const char *key) {
    assert(set);

    int index = _strset_get(set, key);
    if (index != -1)
        _strset_remove(set, index);

    /*
    if (!key) return;

    int index = hasher_djb2(key) % set->cap;
    for (int i = 0; i < set->cap; i++) {
        if (!set->arr[index].taken)
            break;
        if (!strcmp(set->arr[index].key, key))
            break;
        index = (index + 1) % set->cap;
    }

    if (set->arr[index].taken && !strcmp(set->arr[index].key, key)) {
        free(set->arr[index].key);
        set->arr[index].taken = false;
        set->taken--;
    }
    */
}

void strset_each(StrSet *set, StrSetEachCallback cb, void *udata) {
    assert(set);
    if (!cb) return;

    for (int i = 0; i < set->cap; i++) {
        if (set->arr[i].taken) {
            if (!cb(set->arr[i].key, udata)) {
                    free(set->arr[i].key);
                    set->arr[i].taken = false;
                    set->taken--;
            }
        }
    }
}
