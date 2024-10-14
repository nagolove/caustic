// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "koh_common.h"
#include "koh_hashers.h"
#include <stdbool.h>
#include <stdio.h>
#include <munit.h>

typedef enum HTableAction {
    HTABLE_ACTION_NEXT,
    HTABLE_ACTION_BREAK,
    HTABLE_ACTION_REMOVE,
} HTableAction;

typedef struct HTable HTable;

typedef HTableAction (*HTableEachCallback)(
    const void *key, int key_len, void *value, int value_len, void *udata
);
typedef void (*HTableOnRemove)(
    const void *key, int key_len, void *value, int value_len, void *userdata
);
typedef const char *(*HTableKey2Str)(const void *key, int key_len);

typedef struct HTableSetup {
    HTableOnRemove  on_remove;
    HashFunction    hash_func;
    // TODO: Сделать заготовки для вывода строк, целых чисел и 
    // чисел с плавающей запятой
    HTableKey2Str   key2str_func;
    int64_t         cap;
    void            *userdata;
} HTableSetup;

// TODO: ВОзможность хранения ключей без значений(множество)


// Добавляет значение по ключу в таблицу. Возвращает указатель на скопированные 
// внутрь данные.
// Можно ли добавлять значения нулевой длины или передавать NULL для значения..
void *htable_add(
    HTable *ht, const void *key, int key_len, const void *value, int value_len
);
void *htable_add_s(HTable *ht, const char *key, void *value, int value_len);
void *htable_add_f32(HTable *ht, float key, void *value, int value_len);

// Доступ к элементам
void *htable_get(HTable *ht, const void *key, int key_len, int *value_len);
void *htable_get_s(HTable *ht, const char *key, int *value_len);
// TODO: htable_get_f32, htable_get_i32, htable_get_i64, htable_get_char,
// в виде встраиваемых функций.
void *htable_get_f32(HTable *ht, float key, int *value_len);

// Существование элемента. Работа в режиме множества.
bool htable_exist(HTable *ht, const void *key, int key_len);
bool htable_exist_s(HTable *ht, const char *key);

HTable *htable_new(HTableSetup *setup);
void htable_free(HTable *ht);

void htable_remove(HTable *ht, const void *key, int key_len);
void htable_remove_s(HTable *ht, const char *key);
static inline void htable_remove_f32(HTable *ht, float key);

void htable_print(HTable *ht);
void htable_fprint(HTable *ht, FILE *f);
void htable_extend(HTable *ht, int64_t new_cap);
void htable_shrink(HTable *ht);
void htable_clear(HTable *ht);
int64_t htable_count(HTable *ht);

// Возвращает новую таблицу, объединяя а и б. 
// Если ключи равны, то берется значение из a
HTable *htable_union(HTable *a, HTable *b);
// Возвращает новую таблицу равную разности a - b
HTable *htable_subtract(HTable *a, HTable *b);

void htable_each(HTable *ht, HTableEachCallback cb, void *udata);

void *htable_userdata_get(HTable *ht);
void htable_userdata_set(HTable *ht, void *userdata);

void htable_print_tabular(HTable *ht);
// Возвращает строку для печати, с таблицей строковых ключей. Память нужно
// освобождать через free()
char *htable_print_tabular_alloc(HTable *ht);

// TODO: Сделать итераторы
typedef struct HTableIterator {
    HTable  *t;
    int64_t index;
} HTableIterator;

/*

HTable *t = htable_new(NULL);
for (HTableIterator i = htable_iter_new(t);
     htable_iter_valid(&i);
     htable_iter_next(&i)
    ) {
}

htable_free(t);
 */
// Во время итерации нельзя удалять ключи

//#define KOH_INLINE_ITER KOH_INLINE
#define KOH_INLINE_ITER 
KOH_INLINE_ITER HTableIterator htable_iter_new(HTable *t);
KOH_INLINE_ITER void htable_iter_next(HTableIterator *i);
KOH_INLINE_ITER bool htable_iter_valid(HTableIterator *i);
KOH_INLINE_ITER void *htable_iter_key(HTableIterator *i, int *key_len);
KOH_INLINE_ITER void *htable_iter_value(HTableIterator *i, int *value_len);
#undef KOH_INLINE_ITER

extern bool htable_verbose;
extern MunitSuite test_htable_suite_internal;

static inline void htable_remove_f32(HTable *ht, float key) {
    htable_remove(ht, &key, sizeof(key));
}
