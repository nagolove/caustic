#pragma once

#include "koh_hashers.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum HTableAction {
    HTABLE_ACTION_NEXT,
    HTABLE_ACTION_BREAK,
    //HTABLE_ACTION_REMOVE,
} HTableAction;

typedef struct HTable HTable;
typedef HTableAction (*HTableEachCallback)(
    const void *key, int key_len, void *value, int value_len, void *udata
);
typedef void (*HTableOnRemove)(
    const void *key, int key_len, void *value, int value_len 
);

typedef struct HTableSetup {
    HTableOnRemove  on_remove;
    HashFunction    hash_func;
    size_t          cap;
} HTableSetup;

// Добавляет значение по ключу в таблицу. Возвращает указатель на скопированные 
// внутрь данные.
void *htable_add(
    HTable *ht, const void *key, int key_len, const void *value, int value_len
);
void *htable_add_s(HTable *ht, const char *key, void *value, int value_len);
void htable_clear(HTable *ht);
size_t htable_count(HTable *ht);
void htable_each(HTable *ht, HTableEachCallback cb, void *udata);
void htable_free(HTable *ht);
void *htable_get(HTable *ht, const void *key, int key_len, int *value_len);
void *htable_get_s(HTable *ht, const char *key, int *value_len);
HTable *htable_new(struct HTableSetup *setup);
void htable_remove(HTable *ht, const void *key, int key_len);
void htable_remove_s(HTable *ht, const char *key);
void htable_print(HTable *ht);
void htable_fprint(HTable *ht, FILE *f);

extern bool htable_verbose;
