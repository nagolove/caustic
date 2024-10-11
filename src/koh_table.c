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

// XXX: POSIX shit
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>

bool htable_verbose = true;

/*
структура корзинки таблицы:
------------------------------------------------------
| key_len | value_len | hash | key data | value data |
------------------------------------------------------
*/
typedef struct Bucket {
    Hash_t hash;
    int    key_len, value_len;
/*} Bucket __attribute__((aligned(1)));*/
} Bucket;

typedef struct HTable {
    Bucket          **arr;
    int64_t         taken, cap;
    // TODO: Единообразные названия функций обратного вызова
    HTableOnRemove  on_remove;
    HashFunction    hash_func;
    HTableKey2Str   key2str_func;
    void            *userdata;
} HTable;

static int64_t _htable_get(
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

#define htable_aligment 16

static inline void htable_assert(HTable *t) {
    assert(t);
    assert(t->arr);
    assert(t->hash_func);
    assert(t->cap >= 0);
    assert(t->taken >= 0);
}

static inline uint32_t get_aligned_size(uint32_t size) {
    _Static_assert(htable_aligment == 16, "only 16 bytes aligment supported");
    return (size + (htable_aligment - 1)) & ~(htable_aligment - 1);
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
    /*fprintf(f, "-----\n");*/
    for (int64_t i = 0; i < ht->cap; i++) {
        if (ht->arr[i])
            fprintf(
                f, "[%.3zu] %10.10s = %10d, hash mod cap = %.3zu\n",
                i, (char*)get_key(ht->arr[i]), *((int*)get_value(ht->arr[i])),
                ht->arr[i]->hash % ht->cap
            );
        else
            fprintf(f, "[%.3zu]\n", i);
    }
    /*fprintf(f, "-----\n");*/
}

void htable_print(HTable *ht) {
    htable_fprint(ht, stdout);
}

char *htable_print_tabular_alloc(HTable *ht) {
    assert(ht);
    FILE *f = stdout;
    assert(f);
    lua_State *l = luaL_newstate();
    luaL_openlibs(l);

    lua_newtable(l);  
    for (int64_t i = 0; i < ht->cap; i++) {
        if (ht->arr[i]) {

            // Первый элемент таблицы
            lua_pushnumber(l, i + 1);  // t[1] (lua использует 1-индексацию)
            lua_newtable(l);  // создаем подтаблицу

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
            lua_pushnumber(l, *((int*)get_value(ht->arr[i])));
            lua_settable(l, -3);  // t[1]["val"] = "v1"

            // hash_index = 1
            lua_pushstring(l, "hash_index");
            lua_pushnumber(l, ht->arr[i]->hash % ht->cap);
            lua_settable(l, -3);  // t[1]["hash_index"] = 1

            lua_settable(l, -3);  // Добавляем подтаблицу в основную таблицу (t[1] = {...})
        }
    }

    /*lua_newtable(l);*/
    const char *global_name = "UNIQ_TABLE_NAME_981";
    lua_setglobal(l, global_name);

    char *str = L_tabular_alloc(l, global_name);
    assert(str);

    lua_close(l);
    return str;
}

void htable_print_tabular(HTable *ht) {
    char *msg = htable_print_tabular_alloc(ht);
    if (msg) {
        printf("%s\n", msg);
        free(msg);
    }
}

Bucket *bucket_alloc(int key_len, int value_len) {
    uint32_t size = get_aligned_size(sizeof(Bucket)) + 
                    get_aligned_size(key_len) + value_len;
    /*return calloc(size, 1);*/
    /*printf("bucket_alloc: size %u\n", size);*/
    /*return aligned_alloc(sizeof(void*), size);*/
    return calloc(1, size + 100);
}

void htable_extend(HTable *ht, int64_t new_cap) {
    assert(ht);
    assert(ht->cap >= 0);
    assert(ht->taken);
    assert(ht->hash_func);
    assert(ht->arr);
    assert(new_cap > 0);
    assert(ht->cap < new_cap);

    HTable tmp = *ht;
    /*int64_t new_cap = ht->cap * 1.5 + 1;*/
    /*int64_t new_cap = ht->cap * 15 + 1;*/
    printf(
        "\n\033[31m htable_extend: cap %ld, cap %ld\033[0m\n",
        ht->cap, new_cap
    );
    tmp.cap = new_cap;
    tmp.arr = calloc(tmp.cap, sizeof(ht->arr[0]));
    tmp.taken = 0;

    assert(ht->arr);
    for (int64_t j = 0; j < ht->cap; j++) {
        if (ht->arr[j]) {
            // XXX: Зачем пересчитывать значение хэша?
            ht->arr[j]->hash = ht->hash_func(
                get_key(ht->arr[j]), ht->arr[j]->key_len
            );
            _htable_add_uniq(&tmp, ht->arr[j]);
        }
    }

    free(ht->arr);
    *ht = tmp;
}

// TODO: Как-то можно проверить корректность работы данной функции?
static void bucket_free(HTable *ht, int64_t index) {
    assert(ht);
    if (ht->arr[index]) {
        if (ht->on_remove)
            ht->on_remove(
                get_key(ht->arr[index]), ht->arr[index]->key_len,
                get_value(ht->arr[index]), ht->arr[index]->value_len,
                ht->userdata
            );
        free(ht->arr[index]);
        ht->arr[index] = NULL;
    }
}

// Добавляет указатель на корзинку в таблицу согласно значению индекса 
// полученного из хэша хранимого в корзинке.
// Все добавляемые корзинки обязаны быть уникальны.
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

// TODO: Разбить на части и проверить каждую из них.
void *htable_add(
    HTable *ht, const void *key, int key_len, 
    const void *value, int value_len
) {

    assert(ht);
    if (!key) return NULL;

    //print_table(ht);
    assert(key);
    assert(key_len > 0);
    assert(value_len >= 0);

    int64_t index = _htable_get(ht, key, key_len, NULL);

    /*
    if (htable_verbose)
        printf(
            "htable_add: key %p, key_len %d, value %p, value_len %d"
            ", index %zu, cap %zu\n",
            key, key_len, value, value_len, index, ht->cap
        );
        */

    // Ключ есть в таблице, обновить значение
    if (index != INT64_MAX) {
        if (value_len != ht->arr[index]->value_len) {
            bucket_free(ht, index);
            ht->arr[index] = bucket_alloc(key_len, value_len);
            // XXX: Почему идет запись значения ключа, но сравнивается длина значения?
            memmove(get_key(ht->arr[index]), key, key_len);
        }
        ht->arr[index]->value_len = value_len;
        ht->arr[index]->key_len = key_len;
        memmove(get_value(ht->arr[index]), value, value_len);
        return get_value(ht->arr[index]);
    }

    if (ht->taken >= ht->cap * 0.7)
        htable_extend(ht, ht->cap * 2 + 1);

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
    if (value_len == 4) {
    if (htable_verbose)
        printf(
            "htable_add: key '%s', key_len %zu, value %d, value_len %d\n",
            key, strlen(key) + 1, *(int*)value, value_len
        );
    }
    return htable_add(ht, key, strlen(key) + 1, value, value_len);
}

void htable_clear(HTable *ht) {
    for (int64_t k = 0; k < ht->cap; k++) {
        bucket_free(ht, k);
    }
    ht->taken = 0;
}

// TODO: Сделать итератор со структурой
void htable_each(HTable *ht, HTableEachCallback cb, void *udata) {
    assert(ht);
    if (!cb)
        return;
    for (int64_t j = 0; j < ht->cap; j++) {
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
                // TODO: Как удалять? После удаления порядок обхода по таблице
                // меняется?
                // Или заполнить массив удаленных элементов?
                case HTABLE_ACTION_REMOVE: {
                    printf(
                       "htable_each: HTABLE_ACTION_REMOVE "
                       "is not implemented\n"
                    );
                    break;
                }
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

int64_t _htable_get(HTable *ht, const void *key, int key_len, int *value_len) {
    assert(ht);
    assert(ht->cap > 0);
    assert(ht->arr);
    assert(ht->hash_func);

    int64_t index = ht->hash_func(key, key_len) % ht->cap;
    for (int64_t i = 0; i < ht->cap; i++) {
    // XXX: Почему падает с uint64_t ?
    /*for (uint64_t i = 0; i < ht->cap; i++) {*/
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

    return INT64_MAX;

    /*
    size_t ret = SIZE_MAX;
    // XXX: Случится ли переполнение index если тип будет uint64_t?
    int64_t index = ht->hash_func(key, key_len) % ht->cap;
    for (size_t i = 0; i < ht->cap; i++) {
        if (!ht->arr[index]) break;
        if (memcmp(get_key(ht->arr[index]), key, key_len) == 0) {
            if (value_len)
                *value_len = ht->arr[index]->value_len;
            return index;
        }
        index = (index + 1) % ht->cap;
    }

    return ret;
    */
}

void *htable_get(HTable *ht, const void *key, int key_len, int *value_len) {
    int64_t index = _htable_get(ht, key, key_len, value_len);
    return index == INT64_MAX ? NULL : get_value(ht->arr[index]);
}

void *htable_get_s(HTable *ht, const char *key, int *value_len) {
    return htable_get(ht, key, strlen(key) + 1, value_len);
}

void *htable_userdata_get(HTable *ht) {
    assert(ht);
    return ht->userdata;
}

void htable_userdata_set(HTable *ht, void *userdata) {
    assert(ht);
    ht->userdata = userdata;
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
        ht->key2str_func = setup->key2str_func;
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
    for (int64_t i = 0; i < ht->cap; i++) {
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
    int64_t index = _htable_get(ht, key, key_len, NULL);
    if (index != INT64_MAX) {
        _htable_remove(ht, index);
    }
}

void htable_remove_s(HTable *ht, const char *key) {
    htable_remove(ht, key, strlen(key) + 1);
}

int64_t htable_count(HTable *ht) {
    assert(ht);
    assert(ht->taken >= 0);
    return ht->taken;
}
//

void htable_shrink(HTable *ht) {
    // XXX: Здесь ничего и никого..
}

// {{{ Tests

typedef struct TableNode {
    const char  *key;
    bool        taken;
    int         val_i;
    char        val_s[32];
} TableNode;

static TableNode strings[] = {
    // {{{
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
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", false },
    { "hash_table_test", false },
    { "hash collision test", false },
    { NULL, false },
    // }}}
};

static void strings_print(TableNode *strings) {
    lua_State *l = luaL_newstate();
    luaL_openlibs(l);
    char *s = calloc(2048, 1);

    strcat(s, "{ ");
    for (int i = 0; strings[i].key; i++) {
        /*printf("i %d\n", i);*/
        strcat(s, "{ ");
        char line[128] = {};
        sprintf(
            line, "'%s', '%s', %d, %s",
            strings[i].key,
            strings[i].val_s,
            strings[i].val_i,
            strings[i].taken ? "true" : "false"
        );
        strcat(s, line);
        strcat(s, " }, ");
    }
    strcat(s, " }");

    /*printf("\n\ns = %s\n\n", start);*/

    char *table = L_tabular_alloc_s(l, s);
    printf("%s\n", table);
    free(table);
    free(s);
    lua_close(l);
}

static TableNode *strings_permuted_alloc() {
    size_t strings_num = sizeof(strings) / sizeof(strings[0]);
    TableNode *new_strings = calloc(strings_num, sizeof(strings[0]));

    strings_print(strings);

    int permuted[strings_num];
    for (int i = 0; i < strings_num; i++) {
        permuted[i] = rand() % strings_num;
    }

    for (int i = 0; strings[i].key; i++) {
        strings[i].val_i = i * 100 + i;
    }

    for (int i = 0; strings[i].key; i++) {
        new_strings[i].key = strings[permuted[i]].key;
        new_strings[i].val_i = strings[permuted[i]].val_i;
    }

    strings_print(new_strings);

    return new_strings;
}

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

// Проверяет верно ли работет выделение памяти под корзинку и функции обращения
// к ключу и значению.
static MunitResult test_htable_internal_bucket_allocation(
    const MunitParameter params[], void* data
) {
    // {{{
    printf("\ntest_htable_internal_bucket_allocation:\n");

    const char *key = "1234567";
    int key_len = strlen(key) + 1;
    const char *value = "890";
    int value_len = strlen(value) + 1;
    Bucket *buck = bucket_alloc(key_len, value_len);

    /*
    структура корзинки таблицы:
    ------------------------------------------------------
    | key_len | value_len | hash | key data | value data |
    ------------------------------------------------------
    */

    Hash_t hash_val = 171;
    buck->value_len = value_len;
    buck->key_len = key_len;
    buck->hash = hash_val;

    char *key_res = get_key(buck);
    strcpy(key_res, key);
    char *value_res = get_value(buck);
    strcpy(value_res, value);

    {
        //    Hash_t hash;
        //    int    key_len, value_len;
        char *p = (char*)buck;

        printf("hash_val %lu\n", *(Hash_t*)p);
        munit_assert(*(Hash_t*)p == hash_val);
        p += sizeof(buck->hash);

        printf("key_len %d\n", *(int*)p);
        munit_assert(*(int*)p == key_len);

        p += sizeof(buck->key_len);

        printf("value_len %d\n", *(int*)p);
        munit_assert(*(int*)p == value_len);

        p += sizeof(buck->value_len);

        char *k = p;
        printf("strcmp %d, k '%s'\n", strcmp(k, key), k);
        munit_assert(strcmp(k, key) == 0);

        p += key_len + 0;

        char *v = p;
        printf("strcmp %d, v '%s'\n", strcmp(v, value), v);
        // TODO: Доделать
        /*munit_assert(strcmp(v, value) == 0);*/
    }

    free(buck);
    return MUNIT_OK;
    // }}}
}

static void on_remove_example(
    const void *key, int key_len, void *value, int value_len, void *userdata
) {

    /*
    printf(
        "on_remove_example: key '%s', key_len %d, value %d, value_len %d\n",
        (char*)key, key_len, *(int*)value, value_len
    );
    */

    const char *key_src = userdata;
    munit_assert(strcmp(key_src, key) == 0);
    munit_assert(strlen(key_src) + 1 /* \0 byte */ == key_len);
    munit_assert(*(int*)value == 1400);
    munit_assert(value_len == 4);
}

void _test_htable_internal_add_strings(
    TableNode *strings, HTableSetup *table_setup
) {
    assert(strings);
    assert(table_setup);

    // Создать таблицу с определенной хэш функцией
    HTable *t = htable_new(table_setup);

    bool htable_verbose_prev = htable_verbose;
    htable_verbose = true;

    for (int j = 0; strings[j].key; j++) {
        int val = strings[j].val_i;
        const char *key_src = strings[j].key;

        /*
        printf(
            "test_htable_internal_remove: test hash '%s' for key '%s'\n",
            koh_hashers[i].fname,
            key_src
        );
        */

        // Проверить несуществующий ключ
        //munit_assert(htable_get_s(t, key_src, NULL) == NULL);

        // Добавить ключ

        /*
        printf(
            "test_htable_internal_add_strings: add key_src '%s'\n",
            key_src
        );
        // */
        
        htable_add_s(t, key_src, &val, sizeof(val));
    }
    htable_verbose = htable_verbose_prev;

    for (int j = 0; strings[j].key; j++) {
        int val = strings[j].val_i;
        const char *key_src = strings[j].key;

        // Проверить ключ
        /*
        printf(
            "test_htable_internal_add_strings: get key_src '%s'\n",
            key_src
        );
        // */

        const int *val_get = htable_get_s(t, key_src, NULL);
        munit_assert_not_null(val_get);
        munit_assert_int(*val_get, ==, val);

        // Удалить ключ
        htable_remove_s(t, key_src);

        // Снова проверить ключ
        val_get = htable_get_s(t, key_src, NULL);
        munit_assert(val_get == NULL);
    }

    htable_free(t);
}

static MunitResult _test_htable_internal_extend(
        int cap_initial, int cap_mult, int cap_add
) {
    printf("\n");

    HTable *t = htable_new(&(HTableSetup) {
        .cap = cap_initial,
    });

    // Добавляю ключи и значения
    for (int i = 0; strings[i].key; i++) {
        strings[i].val_i = i * 10;
        htable_add_s(t, strings[i].key, &strings[i].val_i, sizeof(int));
    }

    // Проверяю наличие значений
    for (int i = 0; strings[i].key; i++) {
        int *val = htable_get_s(t, strings[i].key, NULL);
        munit_assert_not_null(val);
        munit_assert_int(*val, ==, strings[i].val_i);
    }

    // Увеличиваю вместимость таблицы
    htable_extend(t, t->cap * cap_mult + cap_add);

    // Снова проверяю наличие значений
    for (int i = 0; strings[i].key; i++) {
        int *val = htable_get_s(t, strings[i].key, NULL);
        munit_assert_not_null(val);
        munit_assert_int(*val, ==, strings[i].val_i);
    }

    htable_free(t);
    return MUNIT_OK;
}

// Проверка расширения таблицы и перехэширования
static MunitResult test_htable_internal_extend(
    const MunitParameter params[], void* data
) {
    printf("\n");

    struct {
        int cap_initial, cap_mult, cap_add;
    } tests[] = {

        { 0, 1, 1},
        { 1, 1, 1},
        { 2, 1, 1},
        { 10, 2, 0},
        { 100, 1, 1},

        { 0, 2, 1},
        { 1, 2, 1},
        { 2, 2, 1},
        { 10, 2, 0},
        { 100, 2, 1},

    };
    int tests_num = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < tests_num; i++) {
        if (!_test_htable_internal_extend(
            tests[i].cap_initial,
            tests[i].cap_mult,
            tests[i].cap_add
        ))
            return MUNIT_FAIL;
    }

    return MUNIT_OK;
}

static MunitResult test_htable_internal_add_strings(
    const MunitParameter params[], void* data
) {
    printf("\n");

    for (int i = 0; strings[i].key; i++) {
        strings[i].val_i = i * 10;
    }

    TableNode *strings_permuted = strings_permuted_alloc();
    _test_htable_internal_add_strings(strings_permuted, &(HTableSetup) {
    /*_test_htable_internal_add_strings(strings, &(HTableSetup) {*/
        /*.cap = 100,*/
        .hash_func = koh_hasher_djb2,
    });

    /*
    for (int i = 0; koh_hashers[i].f; i++) {
        _test_htable_internal_add_strings(strings, &(HTableSetup) {
            .cap = 180,
            //.hash_func = koh_hasher_fnv64,
            .hash_func = koh_hashers[i].f,
        });
        //_test_htable_internal_add_strings(strings);
    }
    */

    free(strings_permuted);

    return MUNIT_OK;
}

static MunitResult test_htable_internal_remove(
    const MunitParameter params[], void* data
) {
    htable_verbose = false;
    for (int i = 0; koh_hashers[i].f; i++) {
        for (int j = 0; strings[j].key; j++) {
            // Создать таблицу с определенной хэш функцией
            HTable *t = htable_new(&(HTableSetup) {
                .hash_func = koh_hashers[i].f,
                .key2str_func = NULL,
                .on_remove = on_remove_example,
            });

            int val = 1400;
            const char *key_src = strings[j].key, *val_get;
            htable_userdata_set(t, (void*)key_src);

            /*
            printf(
                "test_htable_internal_remove: test hash '%s' for key '%s'\n",
                koh_hashers[i].fname,
                key_src
            );
            */

            // Проверить несуществующий ключ
            val_get = htable_get_s(t, key_src, NULL);
            munit_assert(val_get == NULL);

            // Добавить ключ
            htable_add_s(t, key_src, &val, sizeof(val));
            // Проверить ключ
            val_get = htable_get_s(t, key_src, NULL);
            munit_assert(*(int*)val_get == val);

            // Удалить ключ
            htable_remove_s(t, key_src);
            munit_assert(htable_count(t) == 0);

            // Снова проверить ключ
            val_get = htable_get_s(t, key_src, NULL);
            munit_assert(val_get == NULL);

            htable_free(t);
        }
    }
    htable_verbose = false;

    return MUNIT_OK;
}

// Проверка соответствия форматированного вывода таблицы
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
        htable_add_s(t, lines[i], &val, sizeof(val));
    }

    char *table[] = {
        // {{{
        "┌─────┬─────────────────────┐\n",
        "│278 :│┌────────────┬─────┐ │\n",
        "│     ││hash_index :│277.0│ │\n",
        "│     ││index .....:│277.0│ │\n",
        "│     ││key .......:│apple│ │\n",
        "│     ││val .......:│0.0  │ │\n",
        "│     │└────────────┴─────┘ │\n",
        "│822 :│┌────────────┬─────┐ │\n",
        "│     ││hash_index :│821.0│ │\n",
        "│     ││index .....:│821.0│ │\n",
        "│     ││key .......:│juice│ │\n",
        "│     ││val .......:│22.0 │ │\n",
        "│     │└────────────┴─────┘ │\n",
        "│823 :│┌────────────┬──────┐│\n",
        "│     ││hash_index :│822.0 ││\n",
        "│     ││index .....:│822.0 ││\n",
        "│     ││key .......:│banana││\n",
        "│     ││val .......:│11.0  ││\n",
        "│     │└────────────┴──────┘│\n",
        "└─────┴─────────────────────┘\n",
        // }}}
        NULL,
    };

    const char *tmp_fname = "/tmp/temp_output.txt";
    int saved_stdout = dup(STDOUT_FILENO);

    FILE *temp_file = fopen(tmp_fname, "w");
    assert(temp_file);

    dup2(fileno(temp_file), STDOUT_FILENO);

    htable_print_tabular(t);

    dup2(saved_stdout, STDOUT_FILENO);
    fclose(temp_file);

    temp_file = fopen(tmp_fname, "r");
    assert(temp_file);

    fseek(temp_file, 0, SEEK_SET);
    int i = 0;
    char buffer[1024] = {};
    while (fgets(buffer, sizeof(buffer), temp_file) != NULL) {
        int res = strcmp(buffer,table[i]);
        munit_assert(res == 0);
        i++;
    }

    fclose(temp_file);

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

    {
        (char*) "/test_htable_internal_bucket_allocation",
        test_htable_internal_bucket_allocation,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "test_htable_internal_remove",
        test_htable_internal_remove,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },


    {
        (char*) "test_htable_internal_extend",
        test_htable_internal_extend,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "test_htable_internal_add_strings",
        test_htable_internal_add_strings,
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

KOH_INLINE KOH_HIDDEN HTableIterator htable_iter_new(HTable *t) {
    htable_assert(t);
    HTableIterator i = {
        .index = 0,
        .t = t,
    };
    return i;
}

KOH_INLINE KOH_HIDDEN void htable_iter_next(HTableIterator *i) {
    assert(i);
    htable_assert(i->t);
}

KOH_INLINE KOH_HIDDEN bool htable_iter_valid(HTableIterator *i) {
    assert(i);
    htable_assert(i->t);
    return false;
}

#undef htable_aligment
