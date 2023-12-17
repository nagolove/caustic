#include "koh_table.h"

#include "koh_hashers.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Поменять тип int на size_t в _htable_get()
// Но что тогда возвращать вместо -1 ??
// SIZE_MAX
// Сперва проверить на WASM

typedef struct Bucket {
    int    key_len, value_len;
    Hash_t hash;
} Bucket;

typedef struct HTable {
    Bucket          **arr;
    size_t          taken, cap;
    HTableOnRemove  on_remove;
    HashFunction    hash_func;
} HTable;

static int _htable_get(HTable *ht, const void *key, int key_len, int *value_len);
static void _htable_remove(HTable *ht, int index);
static void bucket_free(HTable *ht, int index);
static void _htable_add_uniq(HTable *ht, Bucket *bucket);

_Static_assert(
    sizeof(Hash_t) == sizeof(uint64_t),
    "Please use 64 bit Hash_t value"
);

_Static_assert(
    sizeof(size_t) == 8,
    "Only 64 bit size_t supported"
);


static inline uint32_t get_aligned_size(uint32_t size) {
    int mod = size % 16;
    return size - mod + (((mod + 15) >> 4) << 4);
}

static inline void *get_key(const Bucket *bucket) {
    assert(bucket);
    return (char*)bucket + get_aligned_size(sizeof(*bucket));
}

static inline void *get_value(const Bucket *bucket) {
    assert(bucket);
    assert(bucket->key_len > 0);
    return (char*)bucket + get_aligned_size(sizeof(*bucket)) +
                           get_aligned_size(bucket->key_len);
}

void htable_fprint(HTable *ht, FILE *f) {
    assert(ht);
    assert(f);
    fprintf(f, "-----\n");
    for (int i = 0; i < ht->cap; i++) {
        if (ht->arr[i])
            fprintf(
                f, "[%.3d] %10.10s = %10d, hash mod cap = %.3zu\n",
                i, (char*)get_key(ht->arr[i]), *((int*)get_value(ht->arr[i])),
                ht->arr[i]->hash % ht->cap
            );
        else
            fprintf(f, "[%.3d]\n", i);
    }
    fprintf(f, "-----\n");
}

void htable_print(HTable *ht) {
    htable_fprint(ht, stdout);
}

Bucket *bucket_alloc(int key_len, int value_len) {
    uint32_t size = get_aligned_size(sizeof(Bucket)) + 
                    get_aligned_size(key_len) + value_len;
    return calloc(size, 1);
}

void htable_extend(HTable *ht) {
    //printf("\n\nhtable_extend\n\n");
    assert(ht);

    HTable tmp = *ht;
    tmp.cap = ht->cap * 2 + 1;
    tmp.arr = calloc(tmp.cap, sizeof(ht->arr[0]));
    tmp.taken = 0;

    assert(ht->arr);
    for (int j = 0; j < ht->cap; j++) {
        if (ht->arr[j]) {
            ht->arr[j]->hash = ht->hash_func(
                get_key(ht->arr[j]), ht->arr[j]->key_len
            );
            _htable_add_uniq(&tmp, ht->arr[j]);
        }
    }

    free(ht->arr);
    *ht = tmp;
}

static void bucket_free(HTable *ht, int index) {
    assert(ht);
    if (ht->arr[index]) {
        if (ht->on_remove)
            ht->on_remove(
                get_key(ht->arr[index]),
                ht->arr[index]->key_len,
                get_value(ht->arr[index]),
                ht->arr[index]->value_len
            );
        free(ht->arr[index]);
        ht->arr[index] = NULL;
    }
}

void _htable_add_uniq(HTable *ht, Bucket *bucket) {
    assert(ht);
    assert(bucket);
    assert(ht->taken < ht->cap);

    int index = bucket->hash % ht->cap;

    while (ht->arr[index])
        index = (index + 1) % ht->cap;

    ht->arr[index] = bucket;
    ht->taken++;
}

void *htable_add(
    HTable *ht, const void *key, int key_len, 
    const void *value, int value_len
) {

    assert(ht);
    if (!key) return NULL;

    //print_table(ht);

    int index = _htable_get(ht, key, key_len, NULL);

    if (index != -1) {
        if (value_len != ht->arr[index]->value_len) {
            bucket_free(ht, index);
            ht->arr[index] = bucket_alloc(key_len, value_len);
            memmove(get_key(ht->arr[index]), key, key_len);
        }
        ht->arr[index]->value_len = value_len;
        ht->arr[index]->key_len = key_len;
        memmove(get_value(ht->arr[index]), value, value_len);
        return get_value(ht->arr[index]);
    }

    if (ht->taken >= ht->cap * 0.7)
        htable_extend(ht);

    assert(ht->taken < ht->cap);

    Hash_t hash = ht->hash_func(key, key_len);
    index = hash % ht->cap;

    while (ht->arr[index])
        index = (index + 1) % ht->cap;

    ht->arr[index] = bucket_alloc(key_len, value_len);
    ht->arr[index]->value_len = value_len;
    ht->arr[index]->key_len = key_len;
    ht->arr[index]->hash = hash;
    memmove(get_key(ht->arr[index]), key, key_len);
    if (value)
        memmove(get_value(ht->arr[index]), value, value_len);
    ht->taken++;
    return get_value(ht->arr[index]);
}

void *htable_add_s(HTable *ht, const char *key, void *value, int value_len) {
    return htable_add(ht, key, strlen(key) + 1, value, value_len);
}

void htable_clear(HTable *ht) {
    for (int k = 0; k < ht->cap; k++) {
        bucket_free(ht, k);
    }
    ht->taken = 0;
}

void htable_each(HTable *ht, HTableEachCallback cb, void *udata) {
    assert(ht);
    if (!cb)
        return;
    for (int j = 0; j < ht->cap; j++) {
        Bucket *b = ht->arr[j];
        if (b) {
            HTableAction action = cb(
                get_key(b), b->key_len,
                get_value(b), b->value_len, udata
            );
            switch (action) {
                case HTABLE_ACTION_BREAK:
                    return;
                case HTABLE_ACTION_NEXT:
                    break;
            }
        }
    }
}

void htable_free(HTable *ht) {
    assert(ht);
    htable_clear(ht);
    if (ht->arr)
        free(ht->arr);
    free(ht);
}

int _htable_get(HTable *ht, const void *key, int key_len, int *value_len) {
    assert(ht);

    int index = ht->hash_func(key, key_len) % ht->cap;
    for (int i = 0; i < ht->cap; i++) {
        if (!ht->arr[index]) break;
        if (memcmp(get_key(ht->arr[index]), key, key_len) == 0)
            break;
        index = (index + 1) % ht->cap;
    }

    if (ht->arr[index] && memcmp(get_key(ht->arr[index]), key, key_len) == 0) {
        if (value_len)
            *value_len = ht->arr[index]->value_len;
        return index;
    }

    return -1;
}

void *htable_get(HTable *ht, const void *key, int key_len, int *value_len) {
    int index = _htable_get(ht, key, key_len, value_len);
    if (index != -1 ) {
        return get_value(ht->arr[index]);
    }
    return NULL;
}

void *htable_get_s(HTable *ht, const char *key, int *value_len) {
    return htable_get(ht, key, strlen(key) + 1, value_len);
}

HTable *htable_new(struct HTableSetup *setup) {
    HTable *ht = calloc(1, sizeof(HTable));
    if (setup && setup->cap > 0) {
        ht->cap = setup->cap;
    } else
        ht->cap = 17;

    if (setup) {
        ht->on_remove = setup->on_remove;
        ht->hash_func = setup->hash_func;
    }

    if (!ht->hash_func)
        ht->hash_func = koh_hasher_mum;

    ht->arr = calloc(ht->cap, sizeof(ht->arr[0]));
    ht->taken = 0;
    return ht;
}

static void rec_shift(HTable *ht, int index, int hashi) {
    int initial_index = index;
    index = (index + 1) % ht->cap;
    for (int i = 0; i < ht->cap; i++) {
        if (!ht->arr[index]) {
            break;
        }

        /*
        printf(
            "[%3d] key %10s value %.4d hash_mod %.3u\n",
            index,
            (char*)get_key(ht->arr[index]),
            *(int*)get_value(ht->arr[index]),
            ht->arr[index]->hash % ht->cap
        );
        */

        if (ht->arr[index]->hash % ht->cap <= hashi) {
            ht->arr[initial_index] = ht->arr[index];
            ht->arr[index] = NULL;
            rec_shift(ht, index, hashi + 1);
        }

        index = (index + 1) % ht->cap;
    }
}

void _htable_remove(HTable *ht, int remove_index) {
    assert(ht);
    assert(remove_index >= 0 && remove_index < ht->cap);

    int hashi = ht->arr[remove_index]->hash % ht->cap;
    bucket_free(ht, remove_index);

    rec_shift(ht, remove_index, hashi);

    ht->taken--;
}

void htable_remove(HTable *ht, const void *key, int key_len) {
    assert(ht);
    int index = _htable_get(ht, key, key_len, NULL);
    if (index != -1) {
        _htable_remove(ht, index);
    }
}

void htable_remove_s(HTable *ht, const char *key) {
    htable_remove(ht, key, strlen(key) + 1);
}

int htable_count(HTable *ht) {
    assert(ht);
    return ht->taken;
}

