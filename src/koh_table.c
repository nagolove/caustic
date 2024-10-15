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

Случай хранения ключа без значения
------------------------------------------------------
| key_len | 0         | hash | key data | 
------------------------------------------------------
*/
typedef struct Bucket {
    // XXX: Добавить функцию обратного вызова на удаление в каждую корзинку?
    Hash_t key_hash;
    int    key_len, value_len;
} Bucket;

typedef struct HTable {
    Bucket          **arr;
    int64_t         taken, cap;
    // Порог 0..1 при котором будет выполняться расширение таблицы
    float           extend_coef;
    float           cap_mult, cap_add;

    HTableOnRemove  f_on_remove;
    HashFunction    f_hash;
    HTableData2Str  f_key2str, f_val2str;

    void            *userdata;
} HTable;

static int64_t _htable_get_index(
    HTable *ht, const void *key, int key_len, int *value_len
);
static void _htable_remove(HTable *ht, int64_t index);
static void bucket_free(HTable *ht, int64_t index);
static void _htable_add_uniq(HTable *ht, Bucket *bucket);
static Bucket *bucket_alloc(int key_len, int value_len);

// {{{ 64bits static assertions
_Static_assert(
    sizeof(Hash_t) == sizeof(int64_t),
    "Please use 64 bit Hash_t value"
);

_Static_assert(
    sizeof(size_t) == 8,
    "Only 64 bit size_t supported"
);

// }}}

static inline void htable_assert(HTable *t) {
    assert(t);
    assert(t->arr);
    assert(t->f_hash);
    assert(t->cap >= 0);
    assert(t->taken >= 0);
}

#define htable_aligment 16
static inline uint32_t get_aligned_size(uint32_t size) {
    _Static_assert(htable_aligment == 16, "only 16 bytes aligment supported");
    return (size + (htable_aligment - 1)) & ~(htable_aligment - 1);
}

static inline void *bucket_get_key(const Bucket *bucket) {
    assert(bucket);
    return (char*)bucket + get_aligned_size(sizeof(*bucket));
}

static inline void *bucket_get_value(const Bucket *bucket) {
    assert(bucket);
    assert(bucket->key_len > 0);
    return (char*)bucket + get_aligned_size(sizeof(*bucket)) +
                           get_aligned_size(bucket->key_len);
}

// Обновить значение ключа в корзинке. Если новое значение короче или длиннее -
// перевыделить память для корзинки. Возвращает измененную или корзину.
static inline Bucket *bucket_update_value(
    Bucket *bucket, const void *value, int64_t val_len
) {
    assert(bucket);
    assert(bucket->key_len > 0);
    assert(bucket->value_len >= 0);
    assert(val_len >= 0);

    // XXX: Нужно ли вызывать on_remove в случае пересоздания корзинки?

    // Перевыделить память если длина значения иная
    if (val_len != bucket->value_len) {
        Bucket *new_buck = bucket_alloc(bucket->key_len, val_len);
        memmove(
            bucket_get_key(new_buck),
            bucket_get_key(bucket),
            bucket->key_len
        );
        memmove(bucket_get_value(new_buck), value, val_len);
        free(bucket);
        bucket = new_buck;
    } else {
        assert(bucket->value_len == val_len);
        // XXX: Нужно ли где-то еще учитывать value == NULL?
        if (value)
            memmove(bucket_get_value(bucket), value, bucket->value_len);
    }
    return bucket;
}

void htable_fprint(HTable *ht, FILE *f) {
    assert(ht);
    assert(f);
    /*fprintf(f, "-----\n");*/
    for (int64_t i = 0; i < ht->cap; i++) {
        if (ht->arr[i])
            fprintf(
                f, "[%.3zu] %10.10s = %10d, hash mod cap = %.3zu\n",
                i, (char*)bucket_get_key(ht->arr[i]), *((int*)bucket_get_value(ht->arr[i])),
                ht->arr[i]->key_hash % ht->cap
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
    lua_State *l = luaL_newstate();
    luaL_openlibs(l);

    lua_newtable(l);  
    for (int64_t i = 0; i < ht->cap; i++) {
        Bucket *b = ht->arr[i];
        if (!b) 
            continue;;

        // Первый элемент таблицы
        lua_pushnumber(l, i + 1);  // t[1] (lua использует 1-индексацию)
        lua_newtable(l);  // создаем подтаблицу

        // index = 0
        lua_pushstring(l, "index");
        lua_pushnumber(l, i);
        lua_settable(l, -3);  // t[1]["index"] = 0

        // key = "k1"
        if (ht->f_key2str) {
            const char *tmp = ht->f_key2str(bucket_get_key(b), b->key_len);
            assert(tmp);
            lua_pushstring(l, "key");
            lua_pushstring(l, tmp);
            lua_settable(l, -3);  // t[1]["key"] = "k1"
        }

        // val = "v1"
        /*
        lua_pushstring(l, "val");
        if (b->value_len && ht->f_val2str) {
            const char *tmp = ht->f_val2str(bucket_get_value(b), b->value_len);
            lua_pushstring(l, tmp);
        } else {
            lua_pushnil(l);
        }
        lua_settable(l, -3);  // t[1]["val"] = "v1"
        */
        if (b->value_len && ht->f_val2str) {
            const char *tmp = ht->f_val2str(bucket_get_value(b), b->value_len);
            assert(tmp);
            lua_pushstring(l, "val");
            lua_pushstring(l, tmp);
            lua_settable(l, -3);  // t[1]["val"] = "v1"
        } else {
            /*lua_pushnil(l);*/
        }

        // hash_index = 1
        lua_pushstring(l, "hash_index");
        lua_pushnumber(l, b->key_hash % ht->cap);
        lua_settable(l, -3);  // t[1]["hash_index"] = 1

        lua_settable(l, -3);  // Добавляем подтаблицу в основную таблицу (t[1] = {...})
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
    Bucket *b = calloc(1, size);
    assert(b);
    b->key_len = key_len;
    b->value_len = value_len;
    return b;
}

void htable_extend(HTable *ht, int64_t new_cap) {
    assert(ht);
    assert(ht->cap >= 0);
    assert(ht->taken);
    assert(ht->f_hash);
    assert(ht->arr);
    assert(new_cap > 0);
    assert(ht->cap < new_cap);

    HTable tmp = *ht;
    /*int64_t new_cap = ht->cap * 1.5 + 1;*/
    /*int64_t new_cap = ht->cap * 15 + 1;*/
    printf(
        "\n\033[32m htable_extend: cap %ld, cap %ld\033[0m\n",
        ht->cap, new_cap
    );
    tmp.cap = new_cap;
    tmp.arr = calloc(tmp.cap, sizeof(ht->arr[0]));
    tmp.taken = 0;

    assert(ht->arr);
    for (int64_t j = 0; j < ht->cap; j++) {
        if (ht->arr[j]) {
            // XXX: Зачем пересчитывать значение хэша?
            ht->arr[j]->key_hash = ht->f_hash(
                bucket_get_key(ht->arr[j]), ht->arr[j]->key_len
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
    assert(ht->arr);
    assert(ht->cap >= 0);
    assert(ht->taken >= 0);
    assert(index >= 0);

    if (ht->arr[index]) {
        if (ht->f_on_remove)
            ht->f_on_remove(
                bucket_get_key(ht->arr[index]), ht->arr[index]->key_len,
                bucket_get_value(ht->arr[index]), ht->arr[index]->value_len,
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

    int index = bucket->key_hash % ht->cap;

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
    assert((value && value_len > 0) || (!value && value_len == 0));

    if (value_len == 0)
        value = NULL;

    if (!value)
        value = NULL;

    int64_t index = _htable_get_index(ht, key, key_len, NULL);

    if (htable_verbose)
        printf(
            "htable_add: key '%s', key_len %d, value %p, value_len %d"
            ", index %zu, cap %zu\n",
            (char*)key, key_len, value, value_len, index, ht->cap
        );
    // */

    // Ключ есть в таблице, обновить значение
    if (index != INT64_MAX) {
        ht->arr[index] = bucket_update_value(
            ht->arr[index], value, value_len
        );
        return value ? bucket_get_value(ht->arr[index]) : NULL;
    }

    // Расширяю таблицу при определенном пороге загрузки
    if (ht->taken >= ht->cap * ht->extend_coef)
        htable_extend(ht, ht->cap * ht->cap_mult + ht->cap_add);

    assert(ht->taken < ht->cap);

    Hash_t hash = ht->f_hash(key, key_len);
    index = hash % ht->cap;

    while (ht->arr[index])
        index = (index + 1) % ht->cap;

    ht->arr[index] = bucket_alloc(key_len, value_len);
    ht->arr[index]->key_hash = hash;
    memmove(bucket_get_key(ht->arr[index]), key, key_len);

    if (value)
        memmove(bucket_get_value(ht->arr[index]), value, value_len);
    ht->taken++;

    // Вернуть NULL если нет значения
    return value ? bucket_get_value(ht->arr[index]) : NULL;
}

void *htable_add_s(HTable *ht, const char *key, void *value, int value_len) {
    /*
    if (value_len == 4) {
        if (htable_verbose)
            printf(
                "htable_add: key '%s', key_len %zu, value %d, value_len %d\n",
                key, strlen(key) + 1, *(int*)value, value_len
            );
    }
    */
    return htable_add(ht, key, strlen(key) + 1, value, value_len);
}

void *htable_add_f32(HTable *ht, float key, void *value, int value_len) {
    if (value_len == 4) {
        if (htable_verbose)
            printf(
                "htable_add_f32: key %e, value %d, value_len %d\n",
                key, *(int*)value, value_len
            );
    }
    return htable_add(ht, &key, sizeof(key), value, value_len);
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
                bucket_get_key(b), b->key_len,
                bucket_get_value(b), b->value_len, udata
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

// В случае неудачи поиска возращает INT64_MAX
// В случае неудачи поиска value_len не изменяется
int64_t _htable_get_index(
    HTable *ht, const void *key, int key_len, int *value_len
) {
    htable_assert(ht);
    assert(key_len > 0);
    assert(key);

    int64_t index = ht->f_hash(key, key_len) % ht->cap;

    // Максимальное время поиска - проверить все элементы массива
    for (int64_t i = 0; i < ht->cap; i++) {
        if (!ht->arr[index]) break;

        Bucket *buck = ht->arr[index];
        char *key_t = bucket_get_key(buck);
        assert(key_t);

        if (buck->key_len != key_len)
            break;

        int trgt_key_len = key_len > buck->key_len ? buck->key_len : key_len;
        if (memcmp(key_t, key, trgt_key_len) == 0)
            break;
        index = (index + 1) % ht->cap;
    }

    Bucket *buck = ht->arr[index];

    if (!buck)
        return INT64_MAX;

    char *key_t = bucket_get_key(buck);
    assert(key_t);

    if (htable_verbose) 
        printf("_htable_get: index %ld\n", index);

    if (htable_verbose && ht->f_key2str) {
        printf("_htable_get: key '%s'\n", ht->f_key2str(key_t, buck->key_len));
    }

    if (buck->key_len != key_len)
        return INT64_MAX;

    int trgt_key_len = key_len > buck->key_len ? buck->key_len : key_len;
    if (!memcmp(key_t, key, trgt_key_len)) {
        if (value_len)
            *value_len = buck->value_len;
        return index;
    }

    return INT64_MAX;
}

void *htable_get(HTable *ht, const void *key, int key_len, int *value_len) {
    int64_t index = _htable_get_index(ht, key, key_len, value_len);
    return index == INT64_MAX ? NULL : bucket_get_value(ht->arr[index]);
}

void *htable_get_s(HTable *ht, const char *key, int *value_len) {
    return htable_get(ht, key, strlen(key) + 1, value_len);
}

void *htable_get_f32(HTable *ht, float key, int *value_len) {
    return htable_get(ht, &key, sizeof(key), value_len);
}

bool htable_exist(HTable *ht, const void *key, int key_len) {
    assert(ht);
    assert(key);
    assert(key_len > 0);
    int64_t ret =  _htable_get_index(ht, key, key_len, NULL);
    printf("htable_exist: ret %ld\n", ret);
    return ret != INT64_MAX;
}

bool htable_exist_s(HTable *ht, const char *key) {
    assert(key);
    /*printf("htable_exist_s: '%s'\n", key);*/
    return htable_exist(ht, key, strlen(key) + 1);
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
        ht->f_on_remove = setup->on_remove;
        ht->f_hash = setup->hash_func;
        ht->f_key2str = setup->f_key2str;
        ht->f_val2str = setup->f_val2str;
    }

    if (!ht->f_hash)
        ht->f_hash = koh_hasher_mum;

    if (htable_verbose)
        printf(
            "htable_new: capacity %zu, hash functions %s\n",
            ht->cap, koh_hashers_name_by_funcptr(ht->f_hash)
        );

    ht->arr = calloc(ht->cap, sizeof(ht->arr[0]));
    ht->taken = 0;
    ht->extend_coef = 0.7;
    ht->cap_mult = 2.;
    ht->cap_add = 1;
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

        if (ht->arr[index]->key_hash % ht->cap <= hashi) {
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

    int hashi = ht->arr[remove_index]->key_hash % ht->cap;
    bucket_free(ht, remove_index);

    rec_shift(ht, remove_index, hashi);

    ht->taken--;
}

void htable_remove(HTable *ht, const void *key, int key_len) {
    assert(ht);
    int64_t index = _htable_get_index(ht, key, key_len, NULL);
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

KOH_INLINE KOH_HIDDEN HTableIterator htable_iter_new(HTable *t) {
    htable_assert(t);
    HTableIterator i = {
        .index = 0,
        .t = t,
    };

    // Поиск первого существующего элемента
    for (int i = 0; i < t->cap; i++) {
        if (t->arr[i])
            break;
    }

    return i;
}

static inline void htable_iter_assert(HTableIterator *i) {
    assert(i);
    assert(i->t);
    assert(i->index >= 0);
    assert(i->index < i->t->cap);
    htable_assert(i->t);
}

KOH_INLINE KOH_HIDDEN void htable_iter_next(HTableIterator *i) {
    htable_iter_assert(i);

    /*while (i->index + 1 < i->t->cap) {*/
    // XXX: 0 or 1 ?
    while (i->index + 1 < i->t->cap) {
        i->index++;
        if (i->t->arr[i->index])
            break;
    }
}

KOH_INLINE KOH_HIDDEN bool htable_iter_valid(HTableIterator *i) {
    htable_iter_assert(i);
    return i->index < i->t->cap;
}

KOH_INLINE void *htable_iter_key(HTableIterator *i, int *key_len) {
    htable_iter_assert(i);
    if (i->t->arr[i->index]) {
        if (key_len)
            *key_len = i->t->arr[i->index]->key_len;
        return bucket_get_key(i->t->arr[i->index]);
    }
    return NULL;
}

KOH_INLINE void *htable_iter_value(HTableIterator *i, int *value_len) {
    htable_iter_assert(i);
    if (i->t->arr[i->index]) {
        if (value_len)
            *value_len = i->t->arr[i->index]->value_len;
        return bucket_get_value(i->t->arr[i->index]);
    }
    return NULL;
}

// {{{ Tests code

typedef struct TableNodeStr {
    char        *key;
    bool        taken;
    int         val_i;
    char        val_s[32];
} TableNodeStr;

typedef struct TableNodeFloat {
    float       key;
    bool        taken;
    int         val_i;
    char        val_s[32];
} TableNodeFloat;

static TableNodeStr strings[] = {
    // {{{

    { "Wade" },
    { "Frank" },
    { "Leo" },
    { "Leo" },
    { "Hank" },
    { "Charlie" },
    { "Rose" },
    { "David" },
    { "Leo" },
    { "Karen" },
    { "Zane" },
    { "Paul" },
    { "Quinn" },
    { "Jack" },
    { "Hank" },
    { "Ivy" },
    { "Xena" },
    { "Jack" },
    { "Bob" },
    { "Mona" },
    { "Mona" },
    { "Hank" },
    { "Bob" },
    { "Zane" },
    { "mc15[5KsqCMEV}[" },
    { ":TO.[uAR" },
    { "\"poTcGj#W,X47l\"" },
    { "o8?Ip&" },
    { "\"{,jk:0FH^964\"" },
    { "\"@eOU|L9.N98!,-1\"" },
    { "R9-" },
    { "1gx{h#G" },
    { "hIrfL_)X" },
    { "\"eZ,s.EB3}vnJ]\"" },
    { "])pJM]" },
    { "^l>.]|p$4iF" },
    { "QSFq.*Emw&LwWP" },
    { "6M:]+K)bg" },
    { "02M0AX_Xib{x" },
    { "n)!3R5;ndHOXp>" },
    { "}f\\(" },
    { "Q2P" },
    { "\",57PKwi<]Qki8?8\"" },
    { "oy#r+5y<9\\pcQ[C" },
    { "\\{jOiK=cPknUk" },
    { "K3<" },
    { "?pzCx\\U2bp" },
    { "=a6Fn7-lYeW^!s" },
    { "{LJg)-6wD(X" },
    { "P|b!" },
    { "Gb_?$b[K|jO" },
    { "(wV:9R6" },
    { "^/y" },
    { "3/$/m" },
    { "Ycp;T" },
    { "k#Q_hlF@f" },
    { "IK0" },
    { "iPN" },
    { "S/Hf&lYoQ" },
    { "?lZN{Ez:EMT?za" },
    { "ywkyEy" },
    { "\"*bBEp6vt@<,+\"" },
    { "97J" },
    { "!*@b2#OfoG" },
    { "]0n}E{b//(" },
    { "4oag59?#5ix+=)" },
    { "B!NNS" },
    { "7j!" },
    { "dH!zbHiA3" },
    { "amzvcB4}RAw$o/(" },
    { ".347M" },
    { "vEfiM+ifbY4am?K" },
    { "b!RjZDq^L.#D3F" },
    { "<c)" },
    { "Wq/I0zt2dbwV_X." },
    { "2)ug4N" },
    { "cT8&|" },
    { "FSbi" },
    { "xz2](B(KSG" },
    { "@Rzlf" },
    { "g+ypF" },
    { "3v\\b" },
    { "9Qe(1see" },
    { "_}D9\\Sw0+7}I" },
    { "U0_71DN{F1Z5" },
    { ";?n[U4qH[I" },
    { "4;&*3" },
    { "bEBGPWi7/" },
    { "43cS*vJWeXJ" },
    { "w}XC<id>" },
    { "}{fTq\\=D^}M" },
    { "HAw?D4+|+" },
    { "rW1q" },
    { "ZQp-M11<[" },
    { "\\LE)0Z\\{" },
    { "ZmQ@/=p=" },
    { "\"y\\]U[,l0w-:2XB!\"" },
    { "U!*\\%#%bw" },
    { "Rj]" },
    { "T8{-q%E" },

/*
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
*/

    { NULL, false },
    // }}}
};

static TableNodeFloat floats[] = {
    // {{{
    { 4.943336 },
    { 772.963073 },
    { 0.034438 },
    { 9.692035 },
    { -20.955014 },
    { 219.924252 },
    { 231.618535 },
    { 361.073581 },
    { 318.046002 },
    { 0.013028 },
    { 0.026503 },
    { 106.746893 },
    { 0.028147 },
    { 392.658536 },
    { 148.597312 },
    { 493.608157 },
    { -651.0205 },
    { 0.044727 },
    { 122.782966 },
    { -59.162963 },
    { -74.452084 },
    { 497.280727 },
    { 0.029835 },
    { 431.063803 },
    { -745.292864 },
    { -822.939409 },
    { 2.352235 },
    { -987.779427 },
    { 0.006696 },
    { 966.095997 },
    { -94.106158 },
    { 1.099225 },
    { 253.696199 },
    { 893.579006 },
    { 0.068385 },
    { 698.314656 },
    { 108.129072 },
    { -49.098587 },
    { -23.763124 },
    { 976.18851 },
    { 8.630558 },
    { 342.956564 },
    { -46.477419 },
    { 5.841642 },
    { 4.964333 },
    { -991.006196 },
    { -991.008103 },
    { 678.233252 },
    { 328.049018 },
    { -944.203071 },
    { -15.085484 },
    { 8.311962 },
    { 831.282425 },
    { 0.068799 },
    { -816.516573 },
    { 335.590783 },
    { -99.93811 },
    { 445.987873 },
    { 421.482428 },
    { 912.421463 },
    { -34.401144 },
    { 0.048802 },
    { -5.809312 },
    { -946.632033 },
    { -53.300733 },
    { 890.417432 },
    { 432.75132 },
    { 566.95337 },
    { 348.434932 },
    { 723.943825 },
    { 9.520789 },
    { -571.386131 },
    { 174.2654 },
    { 1.438797 },
    { -15.895031 },
    { 329.496133 },
    { 733.269315 },
    { 3.453228 },
    { 8.628067 },
    { 0.076574 },
    { -893.790041 },
    { -593.096359 },
    { 2.436424 },
    { 0.036448 },
    { -29.367521 },
    { -946.422178 },
    { -958.690589 },
    { -51.261052 },
    { -20.379344 },
    { 6.224731 },
    { -577.562844 },
    { 788.700251 },
    { 0.008561 },
    { -84.07956 },
    { -1.664232 },
    { 796.317177 },
    { 0.098209 },
    { 185.570685 },
    { -10.682563 },
    { 114.858396 },
    { -16.907487 },
    { -504.803216 },
    { -41.37134 },
    { -822.098718 },
    { 4.241078 },
    { 465.190335 },
    { -56.83032 },
    { -517.038028 },
    { 0.068661 },
    { 9.546056 },
    { 0.082907 },
    { 109.127709 },
    { -955.622871 },
    { 7.473715 },
    { -818.587105 },
    { 469.885207 },
    { 6.372742 },
    { 5.87206 },
    { 102.31967 },
    { -930.175441 },
    { 0.019957 },
    { 0.012171 },
    { 407.903497 },
    { -593.230444 },
    { 986.306819 },
    { 729.801166 },
    { -91.622699 },
    { 9.178959 },
    { 0.001392 },
    { 965.929429 },
    { -64.997287 },
    { 261.400412 },
    { 9.908389 },
    { 0.033655 },
    { 0.011978 },
    { 2.233896 },
    { 937.83554 },
    { -10.56191 },
    { -512.431536 },
    { 106.937674 },
    { 0.057203 },
    { 7.697811 },
    { 0.044175 },
    { 3.026071 },
    { 857.662322 },
    { 339.198219 },
    { 0.063349 },
    { 871.507033 },
    { 139.970272 },
    { -503.272446 },
    { -17.794742 },
    { 365.502242 },
    { 0.057052 },
    { -580.901954 },
    { -32.789842 },
    { 266.85856 },
    { -98.634682 },
    { 0.08638 },
    { 547.113779 },
    { -617.169817 },
    { 901.538991 },
    { -8.814531 },
    { 105.003724 },
    { -30.40213 },
    { 0.080829 },
    { -38.636886 },
    { 235.834135 },
    { -20.657313 },
    { 0.074436 },
    { -85.904058 },
    { 772.127421 },
    { 0.074597 },
    { -830.617696 },
    { 0.060657 },
    { 515.552314 },
    { -84.675194 },
    { -87.955795 },
    { 646.571555 },
    { 6.971725 },
    { 5.444923 },
    { 196.684733 },
    { 5.001285 },
    { -48.837189 },
    { 3.431107 },
    { 393.691971 },
    { 0.085771 },
    { -543.043996 },
    { -61.337313 },
    { -20.537408 },
    { 6.749422 },
    { 493.26818 },
    { -747.215095 },
    { -982.665157 },
    { -561.362562 },
    { -89.618146 },
    { 0.048577 },
    { -2.454918 },
    { -996.597947 },
    { 107.089944 },
    { -611.439258 },
    { INFINITY },
    { -INFINITY },
    // }}}
};

static bool strings_find(const char *s, int *index) {
    assert(s);
    for (int i = 0; strings[i].key; i++) {
        if (!strcmp(strings[i].key, s)) {
            if (index)
                *index = i;
            return true;
        }
    }
    return false;
}

static void strings_print(TableNodeStr *strings) {
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

static TableNodeStr *strings_permuted_alloc() {
    size_t strings_num = sizeof(strings) / sizeof(strings[0]);
    TableNodeStr *new_strings = calloc(strings_num, sizeof(strings[0]));

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

static MunitResult test_htable_internal_strings_find(
    const MunitParameter params[], void* data
) {
    for (int k = 0; strings[k].key; k++) {
        munit_assert(strings_find(strings[k].key, NULL));
    }
    return MUNIT_OK;
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

static MunitResult test_htable_internal_get(
    const MunitParameter params[], void* data
) {
    printf("\n");
    /*htable_verbose = true;*/
    htable_verbose = false;

    for (int j = 0; j < 100; j++) {
        HTable *t = htable_new(&(HTableSetup) {
            .cap = j,
            .f_key2str = htable_str_str,
        });

        munit_assert(htable_get_s(t, "", NULL) == NULL);
        for (int i = 0; strings[i].key; i++) {
            munit_assert(htable_get_s(t, strings[i].key, NULL) == NULL);
        }

        htable_free(t);
    }

    htable_verbose = false;
    return MUNIT_OK;
}

static MunitResult test_htable_internal_exists(
    const MunitParameter params[], void* data
) {
    HTable *t = htable_new(NULL);
    htable_add_s(t, "hello", NULL, 0);
    munit_assert(htable_exist_s(t, "hello"));
    htable_free(t);
    return MUNIT_OK;
}

static MunitResult test_htable_internal_get2(
    const MunitParameter params[], void* data
) {
    printf("\n");

    // Создавать таблица с разной вместимостью
    //for (int j = 0; j < 100; j += 10) {
    /*for (int j = 0; j < 20; j += 10) {*/
    {
        int j = 10;

        HTable *t = htable_new(&(HTableSetup) {
            .cap = j,
            .f_key2str = htable_str_str,
        });

        /*htable_verbose = true;*/
        htable_verbose = false;

        // Добавить строковые ключи, без данных
        for (int i = 0; strings[i].key; i++) {
            htable_add_s(t, strings[i].key, NULL, 0);
        }
        htable_verbose = false;

        char *table = htable_print_tabular_alloc(t);
        if (table) {
            printf("%s\n", table);
            free(table);
        }

        // Все строки должны присутствовать в табличке
        for (int i = 0; strings[i].key; i++) {
            printf("--------------------\n");
            if (!htable_exist_s(t, strings[i].key)) {
                printf(
                    "test_htable_internal_get2: key '%s' does not exists\n",
                    strings[i].key
                );
            }
            /*munit_assert(htable_exist_s(t, strings[i].key) == true);*/
        }

        htable_free(t);
    }

    return MUNIT_OK;
}

static MunitResult test_htable_internal_iterator(
    const MunitParameter params[], void* data
) {
    HTable *t = htable_new(NULL);

    // Добавить известные ключи и значения
    for (int i = 0; strings[i].key; i++) {
        strings[i].taken = false;
        strings[i].val_i = i;
        htable_add_s(
            t, strings[i].key,
            &strings[i].val_i, sizeof(strings[i].val_i)
        );
    }

    // Проходит ли итератор по всем значениям ключам?
    for (HTableIterator i = htable_iter_new(t);
         htable_iter_valid(&i);
         htable_iter_next(&i)) {

        char *key = htable_iter_key(&i, NULL);
        int *val = htable_iter_value(&i, NULL);

        munit_assert_not_null(key);
        munit_assert_not_null(val);

        // Поиск ключа
        int j = -1;
        if (strings_find(key, &j)) {
            strings[j].taken = true;
        }
    }

    for (int i = 0; strings[i].key; i++) {
        munit_assert(strings[i].taken);
    }

    htable_free(t);
    return MUNIT_OK;
}

static MunitResult test_htable_internal_bucket_allocation2(
    const MunitParameter params[], void* data
) {
    // {{{
    printf("\ntest_htable_internal_bucket_allocation2:\n");

    struct {
        char *key, *val, *new_val;
    } combos[] = {
        { "1234567", "123", "123", },
        { "1234567", "23", "123", },
        { "1234567", "", "123", },
        { "1234567", "", "", },
        { "1234567", NULL, NULL, }, // XXX: Что делать с таким случаем?
        { NULL, NULL, NULL, },
    };

    for (int j = 0; combos[j].key; j++) {

        /*
        printf(
            "key '%s', val '%s', new_val '%s'\n",
            combos[j].key,
            combos[j].val,
            combos[j].new_val
        );
        */

        char *key = combos[j].key;
        int key_len = strlen(key) + 1;

        char *val = combos[j].val;
        int val_len =val ? strlen(val) + 1 : 0;

        char *new_val = combos[j].new_val;
        int new_value_len = new_val ? strlen(new_val) + 1 : 0;

        Bucket *buck = bucket_alloc(key_len, val_len);

        if (val_len) {
            strcpy(bucket_get_key(buck), key);
            strcpy(bucket_get_value(buck), val);

            munit_assert(strcmp(bucket_get_value(buck), val) == 0);
            munit_assert(strcmp(bucket_get_key(buck), key) == 0);

            buck = bucket_update_value(buck, new_val, new_value_len);

            munit_assert(strcmp(bucket_get_value(buck), new_val) == 0);
            munit_assert(strcmp(bucket_get_key(buck), key) == 0);
        }

        free(buck);
    }

    return MUNIT_OK;
    // }}}
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
    buck->key_hash = hash_val;

    char *key_res = bucket_get_key(buck);
    strcpy(key_res, key);
    char *value_res = bucket_get_value(buck);
    strcpy(value_res, value);

    {
        //    Hash_t hash;
        //    int    key_len, value_len;
        char *p = (char*)buck;

        printf("hash_val %lu\n", *(Hash_t*)p);
        munit_assert(*(Hash_t*)p == hash_val);
        p += sizeof(buck->key_hash);

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

    TableNodeStr *node = userdata;
    /*const char *key_src = userdata;*/
    munit_assert(strcmp(node->key, key) == 0);
    munit_assert(strlen(node->key) + 1 /* \0 byte */ == key_len);
    munit_assert(*(int*)value == node->val_i);
    munit_assert(value_len == 4);
}

void _test_htable_internal_add_strings(
    TableNodeStr *strings, HTableSetup *table_setup
) {
    assert(strings);
    assert(table_setup);

    // Создать таблицу с определенной хэш функцией
    HTable *t = htable_new(table_setup);

    bool htable_verbose_prev = htable_verbose;
    /*htable_verbose = true;*/
    htable_verbose = false;

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
        .f_key2str = htable_str_str,
    });

    // Добавляю ключи и значения
    for (int i = 0; strings[i].key; i++) {
        int val = strings[i].val_i = i;
        htable_add_s(t, strings[i].key, &val, sizeof(val));
    }

    // Проверяю наличие значений
    for (int i = 0; strings[i].key; i++) {
        int *val = htable_get_s(t, strings[i].key, NULL);
        munit_assert_not_null(val);

        printf(
            "_test_htable_internal_extend: *val %d, strings[i].val_i %d\n",
            *val, strings[i].val_i
        );

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
        int cap_initial,    // начальная вместимость;
            cap_mult,       // коэф. умножения мнестимости;
            cap_add;        // число, добавляемое к вместимости
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

    TableNodeStr *strings_permuted = strings_permuted_alloc();
    _test_htable_internal_add_strings(strings_permuted, &(HTableSetup) {
    /*_test_htable_internal_add_strings(strings, &(HTableSetup) {*/
        /*.cap = 100,*/
        .hash_func = koh_hasher_djb2,
        .f_key2str = htable_str_str,
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

static void _test_htable_internal_add_get_remove_get_float(HashFunction f) {
    assert(f);

    printf(
        "\n_test_htable_internal_add_get_remove_get_float: hasher '%s'\n",
        koh_hashers_name_by_funcptr(f)
    );

    htable_verbose = false;
    // Создать таблицу с определенной хэш функцией
    HTable *t = htable_new(&(HTableSetup) {
        .hash_func = f,
        .f_key2str = htable_i32_str,
        .f_val2str = htable_i32_str,
    });

    for (int j = 0; floats[j].key < INFINITY; j++) {
        floats[j].val_i = j;
        floats[j].taken = false;
    }

    for (int j = 0; floats[j].key < INFINITY; j++) {
        int val = floats[j].val_i;
        const float key_src = floats[j].key, *val_get;

        /*
        printf(
            "test_htable_internal_remove: test hash '%s' for key '%s'\n",
            koh_hashers[i].fname,
            key_src
        );
        */

        // Проверить несуществующий ключ
        val_get = htable_get_f32(t, key_src, NULL);
        munit_assert(val_get == NULL);

        // Добавить ключ
        htable_add_f32(t, key_src, &val, sizeof(val));
        // Проверить ключ
        val_get = htable_get_f32(t, key_src, NULL);
        munit_assert(*(int*)val_get == val);

        // Удалить ключ
        htable_remove_f32(t, key_src);
        munit_assert(htable_count(t) == 0);

        // Снова проверить ключ
        val_get = htable_get_f32(t, key_src, NULL);
        munit_assert(val_get == NULL);

    }

    htable_free(t);
    htable_verbose = false;
}

static void _test_htable_internal_add_get_remove_get_str(HashFunction f) {
    assert(f);

    printf(
        "\n_test_htable_internal_add_get_remove_get: hasher '%s'\n",
        koh_hashers_name_by_funcptr(f)
    );

    htable_verbose = false;
    // Создать таблицу с определенной хэш функцией
    HTable *t = htable_new(&(HTableSetup) {
        .hash_func = f,
        .f_key2str = htable_i32_str,
        .on_remove = on_remove_example,
    });

    for (int j = 0; strings[j].key; j++) {
        strings[j].val_i = j;
        strings[j].taken = false;
    }

    for (int j = 0; strings[j].key; j++) {
        int val = strings[j].val_i, *val_get = NULL;
        const char *key = strings[j].key;

        // Для проверки в on_remove_example
        htable_userdata_set(t, &strings[j]);

        /*
        printf(
            "test_htable_internal_remove: test hash '%s' for key '%s'\n",
            koh_hashers[i].fname,
            key_src
        );
        */

        // Проверить несуществующий ключ
        val_get = htable_get_s(t, key, NULL);
        munit_assert(val_get == NULL);

        // Добавить ключ
        htable_add_s(t, key, &val, sizeof(val));
        // Проверить ключ
        val_get = htable_get_s(t, key, NULL);
        munit_assert(*val_get == val);

        // Удалить ключ
        htable_remove_s(t, key);
        munit_assert(htable_count(t) == 0);

        // Снова проверить ключ
        val_get = htable_get_s(t, key, NULL);
        munit_assert(val_get == NULL);

    }

    htable_free(t);
    htable_verbose = false;
}


// Проверить несуществующий ключ
// Добавить ключ
// Проверить ключ
// Удалить ключ
// Снова проверить ключ
static MunitResult test_htable_internal_add_get_remove_get_str(
    const MunitParameter params[], void* data
) {
    // Разные хэшеры
    for (int i = 0; koh_hashers[i].f; i++) {
        _test_htable_internal_add_get_remove_get_str(koh_hashers[i].f);
    }
    return MUNIT_OK;
}

// Проверить несуществующий ключ
// Добавить ключ
// Проверить ключ
// Удалить ключ
// Снова проверить ключ
static MunitResult test_htable_internal_add_get_remove_get_float(
    const MunitParameter params[], void* data
) {
    // Разные хэшеры
    for (int i = 0; koh_hashers[i].f; i++) {
        _test_htable_internal_add_get_remove_get_float(koh_hashers[i].f);
    }
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
        int64_t val = i * 11;
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

static MunitResult test_htable_internal_data2str(
    const MunitParameter params[], void* data
) {
    int i32 = 10;
    float f32 = 112;
    int64_t i64 = -1000;
    char *str = "hello";
    char c = 101;

#define eq munit_assert_string_equal
    eq(htable_i32_str(&i32, sizeof(i32)), "10");
    eq(htable_f32_str(&f32, sizeof(f32)), "112.000000");
    eq(htable_i64_str(&i64, sizeof(i64)), "-1000");
    eq(htable_str_str(str, strlen(str)), "hello");
    eq(htable_char_str(&c, sizeof(c)), "101");
#undef eq

    return MUNIT_OK;
}

// }}}

// {{{ Tests definitions
static MunitTest test_htable_internal[] = {

    {
        (char*) "/test_htable_internal_data2str",
        test_htable_internal_data2str,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "/test_htable_internal_strings_find",
        test_htable_internal_strings_find,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "/test_htable_internal_new_free",
        test_htable_internal_new_free,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    /*
    {
        (char*) "/test_htable_internal_get",
        test_htable_internal_get,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },
    */

    {
        (char*) "/test_htable_internal_exists",
        test_htable_internal_exists,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },


    {
        (char*) "/test_htable_internal_get2",
        test_htable_internal_get2,
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
        (char*) "/test_htable_internal_bucket_allocation2",
        test_htable_internal_bucket_allocation2,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "/test_htable_internal_iterator",
        test_htable_internal_iterator,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "test_htable_internal_add_get_remove_get_float",
        test_htable_internal_add_get_remove_get_float,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "test_htable_internal_add_get_remove_get_str",
        test_htable_internal_add_get_remove_get_str,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    {
        (char*) "test_htable_internal_extend",
        test_htable_internal_extend,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },

    /*
    {
        (char*) "test_htable_internal_add_strings",
        test_htable_internal_add_strings,
        NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
    },
    */

    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

MunitSuite test_htable_suite_internal = {
    (char*) "htable_suite_internal",
    test_htable_internal,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

// }}}

#undef htable_aligment
