#include "koh_set.h"

#include "koh_hashers.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*#include <unistd.h>*/

struct Bucket {
    char   *key;
    int    key_len;
    bool   taken;
    Hash_t hash;
};

typedef struct koh_Set {

    // void *keys; // гомогенный массив значений ключей
    // Что тогда делать с удалением и добавлением ключей, как распределять
    // память?

    struct Bucket *arr;
    int cap, taken;
} koh_Set;

static uint32_t hasher_djb2(const char *data, int key_len) {
    unsigned long hash = 5381;

    for (int i = 0; i < key_len; ++i) {
        hash = ((hash << 5) + hash) + data[i]; /* hash * 33 + c */
    }

    return hash;
}

koh_Set *set_new() {
    koh_Set *set = calloc(1, sizeof(koh_Set));
    assert(set);
    set->cap = 11;
    set->arr = calloc(set->cap, sizeof(set->arr[0]));
    assert(set->arr);
    return set;
}

static int _set_get(koh_Set *set, const char *key, int key_len) {
    assert(set);

    if (!key || !key_len)
        return -1;

    int index = hasher_djb2(key, key_len) % set->cap;
    for (int i = 0; i < set->cap; i++) {
        if (!set->arr[index].taken)
            break;
        if (!memcmp(set->arr[index].key, key, key_len))
            break;
        index = (index + 1) % set->cap;
    }

    if (set->arr[index].taken && !memcmp(set->arr[index].key, key, key_len))
        return index;

    return -1;
}

bool set_exist(koh_Set *set, const void *key, int key_len) {
    return _set_get(set, key, key_len) != -1;
}

void set_extend(koh_Set *set) {
    assert(set);
    struct Bucket *old_arr = set->arr;
    int old_cap = set->cap;

    set->cap = set->cap * 2 + 1;
    set->arr = calloc(set->cap, sizeof(set->arr[0]));
    assert(set->arr);
    set->taken = 0;
    for (int j = 0; j < old_cap; j++) {
        if (old_arr[j].taken) {
            set_add(set, old_arr[j].key, old_arr[j].key_len);
            free(old_arr[j].key);
        }
    }

    free(old_arr);
}

koh_SetResult set_add(koh_Set *set, const void *key, int key_len) {
    assert(set);

    if (!key || !key_len) 
        return koh_SR_badargs;

    if (set_exist(set, key, key_len))
        return koh_SR_exists;

    if (set->taken >= set->cap * 0.7)
        set_extend(set);

    assert(set->taken < set->cap);

    Hash_t hash = hasher_djb2(key, key_len); 
    int index = hash % set->cap;

    while (set->arr[index].taken)
        index = (index + 1) % set->cap;

    set->arr[index].taken = true;
    set->arr[index].key = malloc(key_len);
    assert(set->arr[index].key);
    memcpy(set->arr[index].key, key, key_len);
    set->arr[index].hash = hash;
    set->arr[index].key_len = key_len;
    set->taken++;

    return koh_SR_added;
}

void set_clear(koh_Set *set) {
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

void set_free(koh_Set *set) {
    assert(set);
    for (int k = 0; k < set->cap; k++)
        if (set->arr[k].taken)
            free(set->arr[k].key);
    if (set->arr)
        free(set->arr);
    free(set);
}

static void rec_shift(koh_Set *set, int index, int hashi) {
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

void _set_remove(koh_Set *set, int remove_index) {
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

bool set_remove(koh_Set *set, const void *key, int key_len) {
    assert(set);

    int index = _set_get(set, key, key_len);
    if (index != -1) {
        _set_remove(set, index);
        return true;
    }
    return false;
}

void set_each(koh_Set *set, koh_SetEachCallback cb, void *udata) {
    assert(set);
    if (!cb) return;

    for (int i = 0; i < set->cap; i++) {
        if (set->arr[i].taken) {
            koh_SetAction action = cb(
                set->arr[i].key, set->arr[i].key_len, udata
            );
            switch (action) {
                case koh_SA_remove_next:
                    //_set_remove(set, i);
                    free(set->arr[i].key);
                    set->arr[i].taken = false;
                    set->taken--;
                    break;
                case koh_SA_remove_break:
                    //_set_remove(set, i);
                    free(set->arr[i].key);
                    set->arr[i].taken = false;
                    set->taken--;
                    goto _exit;
                    break;
                case koh_SA_break:
                    goto _exit;
                    break;
                case koh_SA_next:
                    break;
            }
        }
    }
_exit:
    return;
}

struct CompareCtx {
    koh_Set *set;
    bool    eq;
};

static koh_SetAction iter_compare(const void *key, int key_len, void *udata) {
    struct CompareCtx *ctx = udata;
    if (!set_exist(ctx->set, key, key_len)) {
        ctx->eq = false;
        return koh_SA_break;
    }
    return koh_SA_next;
}

bool set_compare(const koh_Set *s1, const koh_Set *s2) {
    assert(s1);
    assert(s2);
    struct CompareCtx ctx = {
        .set = (koh_Set*)s2,
        .eq = true,
    };
    if (set_size(s1) != set_size(s2))
        return false;
    set_each((koh_Set*)s1, iter_compare, &ctx);
    return ctx.eq;
}

int set_size(const koh_Set *set) {
    assert(set);
    return set->taken;
}

/*
    индекс      taken
    ------------------
    0           false           
    1           false
    2           true        <-  set_each_begin()
    3           false
    4           true        <- set_each_next()
 */

bool koh_set_view_verbose = false;

struct koh_SetView set_each_begin(koh_Set *set) {
    assert(set);
    if (koh_set_view_verbose)
        printf("set_each_begin: cap %d\n", set->cap);
    struct koh_SetView view = {
        .set = set,
        .i = -1,
    };
    set_each_next(&view);
    if (koh_set_view_verbose)
        printf("set_each_begin: view.i %d\n", view.i);
    return view;
}

void set_each_next(struct koh_SetView *v) {
    assert(v);

    int start_i = v->i;

    int i;
    for (i = v->i + 1; i < v->set->cap; i++) {
        if (v->set->arr[i].taken)
            break;
    }
    v->i = i;
    if (v->i + 0 >= v->set->cap)
        v->finished = true;

    if (koh_set_view_verbose)
        printf("set_each_next: start v-> %d, end v->i %d\n", start_i, v->i);

}

bool set_each_valid(struct koh_SetView *v) {
    assert(v);
    if (v->i >= v->set->cap)
        v->finished = true;
    if (koh_set_view_verbose)
        printf(
            "set_each_valid: v->i %d, v->finished %s\n",
            v->i, v->finished ? "true" : "false");
    
    return !v->finished;
}

const void *set_each_key(struct koh_SetView *v) {
    assert(v);
    if (koh_set_view_verbose)
        printf(
            "set_each_key: i %d, key %p, taken %s\n",
            v->i,
            v->set->arr[v->i].key, 
            v->set->arr[v->i].taken ? "true" : "false"
        );

    return v->set->arr[v->i].key;
}

int set_each_key_len(struct koh_SetView *v) {
    assert(v);
    if (koh_set_view_verbose)
        printf("set_each_key_len: v->i %d\n", v->i);

    return v->set->arr[v->i].key_len;
}

#undef koh_set_view_verbose
