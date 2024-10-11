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
    HTableKey2Str   key2str_func;
    int64_t         cap;
    void            *userdata;
} HTableSetup;

// Добавляет значение по ключу в таблицу. Возвращает указатель на скопированные 
// внутрь данные.
// XXX: Можно ли добавлять значения нулевой длины или NULL? Чтобы получилось
// множество.
void *htable_add(
    HTable *ht, const void *key, int key_len, const void *value, int value_len
);
void *htable_add_s(HTable *ht, const char *key, void *value, int value_len);
void htable_clear(HTable *ht);
int64_t htable_count(HTable *ht);
void htable_each(HTable *ht, HTableEachCallback cb, void *udata);
void htable_free(HTable *ht);
void *htable_get(HTable *ht, const void *key, int key_len, int *value_len);
void *htable_get_s(HTable *ht, const char *key, int *value_len);
HTable *htable_new(struct HTableSetup *setup);
void htable_remove(HTable *ht, const void *key, int key_len);
void htable_remove_s(HTable *ht, const char *key);
void htable_print(HTable *ht);
void htable_fprint(HTable *ht, FILE *f);
void htable_extend(HTable *ht, int64_t new_cap);
void htable_shrink(HTable *ht);

void *htable_userdata_get(HTable *ht);
void htable_userdata_set(HTable *ht, void *userdata);

void htable_print_tabular(HTable *ht);
char *htable_print_tabular_alloc(HTable *ht);

// TODO: Сделать итераторы
typedef struct HTableIterator {
    HTable  *t;
    int64_t index;
} HTableIterator;

KOH_INLINE HTableIterator htable_iter_new(HTable *t);
KOH_INLINE void htable_iter_next(HTableIterator *i);
KOH_INLINE bool htable_iter_valid(HTableIterator *i);

extern bool htable_verbose;
extern MunitSuite test_htable_suite_internal;
