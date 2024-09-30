// vim: set colorcolumn=85
// vim: fdm=marker
#include "koh_table.h"

#include "koh_lua_tools.h"
#include "koh_hashers.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

bool htable_verbose = true;

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

static size_t _htable_get(
    HTable *ht, const void *key, int key_len, int *value_len
);
static void _htable_remove(HTable *ht, int64_t index);
static void bucket_free(HTable *ht, int64_t index);
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
    // Написать что значит в понятном виде
    // Выравнивание на 32?
    return size - mod + (((mod + 15) >> 4) << 4);
    // */
    /*return size;*/
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
    for (size_t i = 0; i < ht->cap; i++) {
        if (ht->arr[i])
            fprintf(
                f, "[%.3zu] %10.10s = %10d, hash mod cap = %.3zu\n",
                i, (char*)get_key(ht->arr[i]), *((int*)get_value(ht->arr[i])),
                ht->arr[i]->hash % ht->cap
            );
        else
            fprintf(f, "[%.3zu]\n", i);
    }
    fprintf(f, "-----\n");
}

void htable_print(HTable *ht) {
    htable_fprint(ht, stdout);
}

void htable_print_tabular(HTable *ht) {
    assert(ht);
    FILE *f = stdout;
    assert(f);
    fprintf(f, "-----\n");
    lua_State *l = luaL_newstate();
    luaL_openlibs(l);

    printf("htable_print_tabular: [%s]\n", L_stack_dump(l));

    lua_newtable(l);  // t = {}
    printf("htable_print_tabular: capacity %zu\n", ht->cap);
    for (size_t i = 0; i < ht->cap; i++) {
        if (ht->arr[i]) {

            /*
            fprintf(
                f, "[%.3zu] %10.10s = %10d, hash mod cap = %.3zu\n",
                i, (char*)get_key(ht->arr[i]), *((int*)get_value(ht->arr[i])),
                ht->arr[i]->hash % ht->cap
            );
            */

            /*printf("htable_print_tabular: [%s]\n", L_stack_dump(l));*/

            // Первый элемент таблицы
            lua_pushnumber(l, i + 1);  // t[1] (lua использует 1-индексацию)
            lua_newtable(l);  // создаем подтаблицу

            /*printf("htable_print_tabular: [%s]\n", L_stack_dump(l));*/

            // index = 0
            lua_pushstring(l, "index");
            lua_pushnumber(l, i);
            lua_settable(l, -3);  // t[1]["index"] = 0

            // key = "k1"
            lua_pushstring(l, "key");
            lua_pushstring(l, (char*)(get_key(ht->arr[i])));
            lua_settable(l, -3);  // t[1]["key"] = "k1"

            // val = "v1"
            lua_pushstring(l, "val");
            /*lua_pushstring(l, "v1");*/
            lua_pushnumber(l, *((int*)get_value(ht->arr[i])));
            lua_settable(l, -3);  // t[1]["val"] = "v1"

            // hash_index = 1
            lua_pushstring(l, "hash_index");
            lua_pushnumber(l, ht->arr[i]->hash % ht->cap);
            lua_settable(l, -3);  // t[1]["hash_index"] = 1

            /*printf("htable_print_tabular: [%s]\n", L_stack_dump(l));*/
            lua_settable(l, -3);  // Добавляем подтаблицу в основную таблицу (t[1] = {...})
            /*printf("htable_print_tabular: [%s]\n", L_stack_dump(l));*/

            /*printf("htable_print_tabular: i %zu\n", i);*/
        } else {
            /*fprintf(f, "[%.3zu]\n", i);*/
        }
    }

    /*lua_newtable(l);*/
    const char *global_name = "UNIQ_TABLE_NAME_981";
    lua_setglobal(l, global_name);

    L_tabular_print(l, global_name);

    fprintf(f, "-----\n");

    lua_close(l);
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
    for (size_t j = 0; j < ht->cap; j++) {
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

static void bucket_free(HTable *ht, int64_t index) {
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

    size_t index = _htable_get(ht, key, key_len, NULL);

    if (htable_verbose)
        printf(
            "htable_add: key %p, key_len %d, value %p, value_len %d"
            ", index %zu, cap %zu\n",
            key, key_len, value, value_len, index, ht->cap
        );

    if (index != SIZE_MAX) {
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
    for (size_t k = 0; k < ht->cap; k++) {
        bucket_free(ht, k);
    }
    ht->taken = 0;
}

void htable_each(HTable *ht, HTableEachCallback cb, void *udata) {
    assert(ht);
    if (!cb)
        return;
    for (size_t j = 0; j < ht->cap; j++) {
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

size_t _htable_get(HTable *ht, const void *key, int key_len, int *value_len) {
    assert(ht);

    int64_t index = ht->hash_func(key, key_len) % ht->cap;
    for (size_t i = 0; i < ht->cap; i++) {
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

    return SIZE_MAX;
}

void *htable_get(HTable *ht, const void *key, int key_len, int *value_len) {
    size_t index = _htable_get(ht, key, key_len, value_len);
    if (index != SIZE_MAX) {
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

    if (htable_verbose)
        printf(
            "htable_new: capacity %zu, hash functions %s\n",
            ht->cap, koh_hashers_name_by_funcptr(ht->hash_func)
        );

    ht->arr = calloc(ht->cap, sizeof(ht->arr[0]));
    ht->taken = 0;
    return ht;
}

static void rec_shift(HTable *ht, int64_t index, int hashi) {
    int64_t initial_index = index;
    index = (index + 1) % ht->cap;
    for (size_t i = 0; i < ht->cap; i++) {
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

void _htable_remove(HTable *ht, int64_t remove_index) {
    assert(ht);
    assert(remove_index >= 0 && remove_index < ht->cap);

    int hashi = ht->arr[remove_index]->hash % ht->cap;
    bucket_free(ht, remove_index);

    rec_shift(ht, remove_index, hashi);

    ht->taken--;
}

void htable_remove(HTable *ht, const void *key, int key_len) {
    assert(ht);
    size_t index = _htable_get(ht, key, key_len, NULL);
    if (index != SIZE_MAX) {
        _htable_remove(ht, index);
    }
}

void htable_remove_s(HTable *ht, const char *key) {
    htable_remove(ht, key, strlen(key) + 1);
}

size_t htable_count(HTable *ht) {
    assert(ht);
    return ht->taken;
}
//

// {{{ Tests

static struct {
    const char  *key;
    bool        taken;
    int         val_i;
    char        val_s[32];
} strings[] = {
    { "apple", false },
    { "banana", false },
    { "orange", false },
    { "grape", false },
    { "pear", false },
    { "kiwi", false },
    { "watermelon", false },
    { "cherry", false },
    { "strawberry", false },
    { "blueberry", false },
    { "aaaaaaaaaaaa", false },
    { "zzzzzzzzzzzz", false },
    { "1234567890", false },
    { "!@#$%^&*()", false },
    { "Lorem ipsum dolor sit amet, consectetur adipiscing elit", false },
    { "the quick brown fox jumps over the lazy dog", false },
    /*{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", false },*/
    { "hash_table_test", false },
    { "hash collision test", false },
    { NULL, false },
};

static MunitResult test_htable_internal_new_free(
    const MunitParameter params[], void* data
) {
    printf("\n");
    htable_verbose = true;
    HTable *t = htable_new(NULL);
    htable_free(t);
    htable_verbose = false;
    return MUNIT_OK;
}

static MunitResult test_htable_internal_print_tabular(
    const MunitParameter params[], void* data
) {
    htable_verbose = false;
    HTable *t = htable_new(&(HTableSetup) {
        .cap = 1024,
        .hash_func = koh_hasher_fnv64,
    });

    const char *lines[] = {
        "apple",
        "banana",
        "juice",
        NULL,
    };

    for (int i = 0; lines[i]; i++) {
        uint64_t val = i * 11;
        /*htable_add_s(t, strings[i].key, &val, sizeof(val));*/
        htable_add_s(t, lines[i], &val, sizeof(val));
    }

    /*
┌─────┬─────────────────────┐
│278 :│┌────────────┬─────┐ │
│     ││hash_index :│277.0│ │
│     ││index .....:│277.0│ │
│     ││key .......:│apple│ │
│     ││val .......:│0.0  │ │
│     │└────────────┴─────┘ │
│822 :│┌────────────┬─────┐ │
│     ││hash_index :│821.0│ │
│     ││index .....:│821.0│ │
│     ││key .......:│juice│ │
│     ││val .......:│22.0 │ │
│     │└────────────┴─────┘ │
│823 :│┌────────────┬──────┐│
│     ││hash_index :│822.0 ││
│     ││index .....:│822.0 ││
│     ││key .......:│banana││
│     ││val .......:│11.0  ││
│     │└────────────┴──────┘│
└─────┴─────────────────────┘
*/


    htable_print_tabular(t);
    htable_free(t);
    return MUNIT_OK;
}

// }}}

static MunitTest test_htable_internal[] = {

    {
        (char*) "/test_htable_internal_new_free",
        test_htable_internal_new_free,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "/test_htable_internal_print_tabular",
        test_htable_internal_print_tabular,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

MunitSuite test_htable_suite_internal = {
    (char*) "htable_suite_internal",
    test_htable_internal,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

