#include "koh_strset.h"

#include "koh_hashers.h"
#include "koh_common.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

bool strset_verbose = false;

struct Bucket {
    char   *key;
    size_t key_len;
    Hash_t hash;
    bool   taken;
};

typedef struct StrSet {
    struct Bucket   *arr;
    size_t          cap, taken;
    HashFunction    hasher;
} StrSet;

StrSet *strset_new(struct StrSetSetup *setup) {
    StrSet *set = calloc(1, sizeof(StrSet));
    assert(set);

    if (!setup) {
        set->cap = 11;
        set->hasher = koh_hasher_mum;
    } else {
        set->cap = setup->capacity ? setup->capacity : 1;
        set->hasher = setup->hasher;
    }
    /*set->hasher = koh_hasher_fnv64;*/

    set->arr = calloc(set->cap, sizeof(set->arr[0]));
    assert(set->arr);
    return set;
}

static size_t _strset_get(const StrSet *set, const char *key) {
    assert(set);

    if (!key)
        return SIZE_MAX;

    // XXX: Не хранится длина ключа, может понадобиться для длинных ключей
    Hash_t index = set->hasher(key, strlen(key)) % set->cap;
    assert(index < set->cap);
    for (size_t i = 0; i < set->cap; i++) {
        if (!set->arr[index].taken)
            break;
        if (!strcmp(set->arr[index].key, key))
            break;
        index = (index + 1) % set->cap;
    }

    if (set->arr[index].taken && !strcmp(set->arr[index].key, key))
        return index;

    return SIZE_MAX;
}

bool strset_exist(const StrSet *set, const char *key) {
    return strset_existn(set, key, strlen(key));
}

void strset_extend(StrSet *set) {
    /*trace("strset_extend:\n");*/
    assert(set);
    struct Bucket *old_arr = set->arr;
    size_t old_cap = set->cap;

    set->cap = set->cap * 2 + 1;

    size_t sz = sizeof(set->arr[0]);

    if (strset_verbose)
        printf(
            "strset_extend: old_cap %zu, cap %zu, sz %zu\n",
            old_cap, set->cap, sz
        );

    set->arr = calloc(set->cap, sz);
    assert(set->arr);
    set->taken = 0;
    for (size_t j = 0; j < old_cap; j++) {
        if (old_arr[j].taken) {
            strset_add(set, old_arr[j].key);
            free(old_arr[j].key);
        }
    }

    free(old_arr);
}

void strset_add(StrSet *set, const char *key) {
    strset_addn(set, key, strlen(key));
}

void strset_addn(StrSet *set, const char *key, size_t key_len) {
    assert(set);
    if (!key) return;

    if (strset_exist(set, key))
        return;

    if (set->taken >= set->cap * 0.7)
        strset_extend(set);

    assert(set->taken < set->cap);

    Hash_t hash = set->hasher(key, key_len); 
    /*printf("strset_addn: hash %lu\n", hash);*/
    Hash_t index = hash % set->cap;

    while (set->arr[index].taken)
        index = (index + 1) % set->cap;

    set->arr[index].taken = true;
    set->arr[index].key = strdup(key);
    set->arr[index].hash = hash;
    set->taken++;
}

void strset_clear(StrSet *set) {
    for (size_t k = 0; k < set->cap; k++) {
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
    for (size_t k = 0; k < set->cap; k++)
        if (set->arr[k].taken)
            free(set->arr[k].key);
    if (set->arr)
        free(set->arr);
    free(set);
}

static void rec_shift(StrSet *set, Hash_t index, Hash_t hashi) {
    Hash_t initial_index = index;

    index = (index + 1) % set->cap;

    if (strset_verbose) {
        printf(
            "rec_shift: index %llu, hashi %llu\n",
            (long long)index, (long long)hashi
        );
    }

    for (size_t i = 0; i < set->cap; i++) {

        if (strset_verbose) {
            printf(
                "rec_shift: index %llu, taken %s, key '%s'\n",
                (long long)index, 
                set->arr[index].taken ? "true" : "false",
                set->arr[index].key
            );
        }

        if (!set->arr[index].taken) {
            break;
        }

        if (set->arr[index].hash % set->cap <= hashi) {
            set->arr[initial_index] = set->arr[index];
            set->arr[index].taken = false;
            set->arr[index].key_len = 0;
            set->arr[index].key = NULL;
            if (hashi + 1 < set->cap)
                rec_shift(set, index, hashi + 1);
        }

        index = (index + 1) % set->cap;
    }
}

void _strset_remove(StrSet *set, Hash_t remove_index) {
    assert(set);
    assert(remove_index >= 0 && remove_index < set->cap);

    Hash_t hashi = set->arr[remove_index].hash % set->cap;
    if (set->arr[remove_index].key) {
        free(set->arr[remove_index].key);
        set->arr[remove_index].key = NULL;
        memset(&set->arr[remove_index], 0, sizeof(set->arr[0]));
    }

    rec_shift(set, remove_index, hashi);
    --set->taken;
}

void strset_remove(StrSet *set, const char *key) {
    strset_removen(set, key, strlen(key));
}

void strset_each(StrSet *set, StrSetEachCallback cb, void *udata) {
    assert(set);
    if (!cb) return;

    for (size_t i = 0; i < set->cap; i++) {
        if (set->arr[i].taken) {
                StrSetAction action = cb(set->arr[i].key, udata);
                switch (action) {
                    case SSA_remove:
                        free(set->arr[i].key);
                        set->arr[i].taken = false;
                        set->taken--;
                        break;
                    case SSA_break:
                        goto _exit;
                        break;
                    case SSA_next:
                        break;
                }
            }
        }
_exit:
    return;
}

struct CompareCtx {
    const StrSet    *set;
    bool            eq;
};

static StrSetAction iter_compare(const char *key, void *udata) {
    struct CompareCtx *ctx = udata;
    if (!strset_exist(ctx->set, key)) {
        ctx->eq = false;
        return SSA_break;
    }
    return SSA_next;
}

bool strset_compare(StrSet *s1, StrSet *s2) {
    assert(s1);
    assert(s2);
    struct CompareCtx ctx = {
        .set = (const StrSet*)s2,
        .eq = true,
    };
    strset_each((StrSet*)s1, iter_compare, &ctx);
    return ctx.eq;
}

struct PrintCtx {
    FILE *f;
    const char *fmt;
};

static StrSetAction iter_print(const char *key, void *udata) {
    struct PrintCtx *ctx = udata;
    fprintf(ctx->f, ctx->fmt, key);
    return SSA_next;
}

void strset_print(StrSet *set, FILE *f) {
    assert(set);
    assert(f);

    strset_each(set, iter_print, &(struct PrintCtx) {
        .f = f,
        .fmt = "%s",
    });
}

struct DifferenceCtx {
    StrSet *s2, *difference;
};

static StrSetAction iter_diffence(const char *key, void *udata) {
    struct DifferenceCtx *ctx = udata;
    if (!strset_exist(ctx->s2, key)) {
        //trace("iter_diffence: add to difference '%s'\n", key);
        strset_add(ctx->difference, key);
    }
    return SSA_next;
}

StrSet *strset_difference(StrSet *s1, StrSet *s2) {
    assert(s1);
    assert(s2);

    struct DifferenceCtx ctx = {
        .s2 = (StrSet*)s2,
        .difference = strset_new(NULL),
    };
    strset_each((StrSet*)s1, iter_diffence, &ctx);

    return ctx.difference;
}

bool strset_compare_strs(const StrSet *s1, char **lines, size_t lines_num) {
    assert(s1);
    assert(lines);
    for (size_t i = 0; i < lines_num; i++) {
        if (!strset_exist(s1, lines[i]))
            return false;
    }
    return true;
}

size_t strset_count(const StrSet *set) {
    assert(set);
    return set->taken;
}

struct StrSetIter strset_iter_new(StrSet *set) {
    assert(set);
    struct StrSetIter iter = {
        .i = 0,
        .set = set,
    };

    for (size_t i = 0; i < set->cap; i++) {
        if (set->arr[i].taken) {
            iter.i = i;
            return iter;
        }
    }

    iter.i = set->cap;
    return iter;
}

KOH_FORCE_INLINE bool strset_iter_valid(struct StrSetIter *iter) {
    assert(iter);
    assert(iter->set);
    printf("strset_iter_valid: %zu %zu\n", iter->i, iter->set->cap);
    return iter->i < iter->set->cap;
}

KOH_FORCE_INLINE void strset_iter_next(struct StrSetIter *iter) {
    assert(iter);
    assert(iter->set);

    if (iter->i >= iter->set->cap) {
        const char *msg = "strset_iter_next: iterator reached end\n";
        fprintf(stderr, "%s", msg);
        fprintf(stdout, "%s", msg);
        abort();
    }

    if (iter->set->arr[iter->i + 1].taken) {
        iter->i++;
        return;
    } else {
        for (size_t i = iter->i + 1; i < iter->set->cap; i++) {
            if (iter->set->arr[i].taken) {
                iter->i = i;
                return;
            }
        }
    }
    iter->i = iter->set->cap;
}

KOH_FORCE_INLINE const char *strset_iter_get(struct StrSetIter *iter) {
    assert(iter);
    assert(iter->set);
    return iter->set->arr[iter->i].key;
}

void strset_removen(StrSet *set, const char *key, size_t key_len) {
    assert(set);
    size_t index = _strset_get(set, key);
    if (index != SIZE_MAX)
        _strset_remove(set, index);
}

bool strset_existn(const StrSet *set, const char *key, size_t key_len) {
    return _strset_get(set, key) != SIZE_MAX;
}

void strset_print_f(StrSet *set, FILE *f, const char *fmt) {
    assert(set);
    assert(f);
    assert(fmt);
    assert(strstr(fmt, "%s"));

    strset_each(set, iter_print, &(struct PrintCtx) {
        .f = f,
        .fmt = fmt,
    });
}
