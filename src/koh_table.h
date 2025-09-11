// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "koh_common.h"
#include "koh_hashers.h"
#include "munit.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum HTableAction {
    HTABLE_ACTION_NEXT,
    HTABLE_ACTION_BREAK,
    HTABLE_ACTION_REMOVE,
} HTableAction;

typedef struct HTable HTable;

// Вызывается для каждого элемента
typedef HTableAction (*HTableEachCallback)(
    const void *key, int key_len, void *value, int value_len, void *userdata
);
// Вызывается при удалении элемента
typedef void (*HTableOnRemove)(
    const void *key, int key_len, void *value, int value_len, void *userdata
);
// Преобразует ключ или значение в статическую строку и возвращает её.
typedef const char *(*HTableData2Str)(const void *data, int data_len);
// Если ключи равны, то возвращается 0. Нужна для сложных по
// структуре ключей. Для простых ключей можно использовать memcmp()
typedef int (*HTableKeyCmp)(const void *a, const void *b, size_t len);
// memcmp(void *a, void *b, size_t len);

typedef struct HTableSetup {
    // По умолчанию для сравнения ключей используется memcmp()
    // Для таблиц, которые хранять числа с плавающей точкой использовать 
    // специальные функции сравнения cmp_f32, cmp_f64
    HTableKeyCmp    f_keycmp;
    HTableOnRemove  f_on_remove;
    HashFunction    f_hash;
    HTableData2Str  f_key2str, f_val2str;
    int64_t         cap;
    void            *userdata;
} HTableSetup;

// TODO: ВОзможность хранения ключей без значений(множество)

// Добавляет значение по ключу в таблицу. 
// Возвращает указатель на скопированные внутрь данные значения. 
// Если длина значения равно нулю или указатель на значение пустой, то функция 
// возвращает NULL. В таком случае таблица работает в режиме множества
void *htable_add(
    HTable *ht, const void *key, int key_len, const void *value, int value_len
);
void *htable_add_s(HTable *ht, const char *key, void *value, int value_len);

static inline void *htable_add_f32(
    HTable *ht, float key, const void *value, int value_len
);
static inline void *htable_add_i32(
    HTable *ht, int32_t key, const void *value, int value_len
);
static inline void *htable_add_u32(
    HTable *ht, uint32_t key, const void *value, int value_len
);
static inline void *htable_add_i64(
    HTable *ht, int64_t key, const void *value, int value_len
);
static inline void *htable_add_u64(
    HTable *ht, u64 key, const void *value, int value_len
);

// Доступ к элементам
void *htable_get(HTable *ht, const void *key, int key_len, int *value_len);
void *htable_get_s(HTable *ht, const char *key, int *value_len);
// TODO: htable_get_f32, htable_get_i32, htable_get_i64, htable_get_char,
// в виде встраиваемых функций.
void *htable_get_f32(HTable *ht, float key, int *value_len);

// Существование элемента. Работа в режиме множества.
bool htable_exist(HTable *ht, const void *key, int key_len);
bool htable_exist_s(HTable *ht, const char *key);

// TODO: Сравнение таблиц на тождественность
// В тесте 
// munit_assert(htable_compare(t1, t2) == true)
//bool htable_compare(HTable *t1, HTable *t2);

// Сравнение таблиц по ключам как множеств.
bool htable_compare_keys(HTable *t1, HTable *t2);

HTable *htable_new(HTableSetup *setup);
void htable_free(HTable *ht);

// Возвращает истину если ключ был найден в таблице и удален.
bool htable_remove(HTable *ht, const void *key, int key_len);
bool htable_remove_s(HTable *ht, const char *key);

static inline bool htable_remove_f32(HTable *ht, float key);
static inline bool htable_remove_i32(HTable *ht, int key);
static inline bool htable_remove_i64(HTable *ht, int64_t key);

void htable_print(HTable *ht);
void htable_fprint(HTable *ht, FILE *f);
void htable_extend(HTable *ht, int64_t new_cap);
void htable_shrink(HTable *ht);
void htable_clear(HTable *ht);
int64_t htable_count(HTable *ht);

// Только если установлены функции обратного вызова для ключей и значений.
// Возвращает выделенную память.
//char *htable_save_alloc(HTable *ht);

// Загрузить из Луа таблицы представленной строкой
bool htable_load(HTable *ht, const char *lua_code);

// Возвращает новую таблицу, объединяя а и б. 
// Только для таблиц с одинаковыми функция хеширования
// Если ключи равны, то берется значение из a
// Параметры новой таблицы (функция хэширования, преобразователи ключей,
// функции удаления, указатель на пользовательские данные) берутся из a
HTable *htable_union(HTable *a, HTable *b);

// Возвращает новую таблицу равную разности a - b
/*HTable *htable_subtract(HTable *a, HTable *b);*/

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

// {{{ Функции конверторы данных

static inline const char *htable_i32_str(const void *data, int len) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    assert(data);
    sprintf(buf, "%d", *(const int*)data);
    return buf;
}

static inline const char *htable_u32_str(const void *data, int len) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    assert(data);
    sprintf(buf, "%u", *(const unsigned int*)data);
    return buf;
}

static inline const char *htable_f32_str(const void *data, int len) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    assert(data);
    sprintf(buf, "%f", *(const float*)data);
    return buf;
}

static inline const char *htable_i64_str(const void *data, int len) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    assert(data);
    sprintf(buf, "%lld", (long long)*(const int64_t*)data);
    return buf;
}

static inline const char *htable_str_str(const void *data, int len) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    assert(data);
    sprintf(buf, "%.*s", (int)sizeof(buf) - 10, (const char*)data);
    return buf;
}

static inline const char *htable_char_str(const void *data, int len) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    assert(data);
    sprintf(buf, "%d", *(const char*)data);
    return buf;
}
// }}}

// {{{ Inlines
static inline void *htable_add_f32(
    HTable *ht, float key, const void *value, int value_len
) {
    return htable_add(ht, &key, sizeof(key), value, value_len);
}

static inline void *htable_add_i32(
    HTable *ht, int32_t key, const void *value, int value_len
) {
    return htable_add(ht, &key, sizeof(key), value, value_len);
}

static inline void *htable_add_u32(
    HTable *ht, uint32_t key, const void *value, int value_len
) {
    return htable_add(ht, &key, sizeof(key), value, value_len);
}

static inline void *htable_add_u64(
    HTable *ht, u64 key, const void *value, int value_len
) {
    return htable_add(ht, &key, sizeof(key), value, value_len);
}

static inline void *htable_add_i64(
    HTable *ht, int64_t key, const void *value, int value_len
) {
    return htable_add(ht, &key, sizeof(key), value, value_len);
}

static inline bool htable_remove_f32(HTable *ht, float key) {
    return htable_remove(ht, &key, sizeof(key));
}

static inline bool htable_remove_i64(HTable *ht, int64_t key) {
    return htable_remove(ht, &key, sizeof(key));
}

static inline bool htable_remove_i32(HTable *ht, int key) {
    return htable_remove(ht, &key, sizeof(key));
}
// }}} 

// Для таблиц, которые хранять числа с плавающей точкой использовать 
// специальные функции сравнения cmp_f32, cmp_f64

static inline int cmp_f32(const void *a, const void *b, size_t len) {
    float fa = *(const float*)a, fb = *(const float*)b;
    return (fa > fb) - (fa < fb); // не учитывает NaN!
}

static inline int cmp_f64(const void *a, const void *b, size_t len) {
    double fa = *(const double*)a, fb = *(const double*)b;
    return (fa > fb) - (fa < fb); // не учитывает NaN!
}
