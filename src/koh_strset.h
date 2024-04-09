#pragma once

#include <stdbool.h>
#include <stdio.h>
#include "koh_common.h"
#include "koh_hashers.h"

typedef enum StrSetAction {
    SSA_next,
    SSA_remove,
    SSA_break,
} StrSetAction;

typedef struct StrSet StrSet;
// Если возвращает ложь, то элемент удаляется из множества
typedef StrSetAction (*StrSetEachCallback)(const char *key, void *udata);

struct StrSetSetup {
    HashFunction    hasher;
    size_t          capacity;
};

StrSet *strset_new(struct StrSetSetup *setup);
bool strset_exist(const StrSet *set, const char *key);
void strset_add(StrSet *set, const char *key);
void strset_addn(StrSet *set, const char *key, size_t key_len);
void strset_clear(StrSet *set);
void strset_free(StrSet *set);
void strset_remove(StrSet *set, const char *key);
void strset_each(StrSet *set, StrSetEachCallback cb, void *udata);
bool strset_compare(const StrSet *s1, const StrSet *s2);
bool strset_compare_strs(const StrSet *s1, char **lines, size_t lines_num);
// Создает новое множество строк из разницы s1 - s2
StrSet *strset_difference(const StrSet *s1, const StrSet *s2);
// Увеличивает емкость контейнера в двое, производит перехеширование
void strset_extend(StrSet *set);
void strset_print(StrSet *set, FILE *f);
size_t strset_count(const StrSet *set);

struct StrSetIter {
    size_t  i;
    StrSet  *set;
};

struct StrSetIter strset_iter_new(StrSet *set);
bool strset_iter_valid(struct StrSetIter *iter);
void strset_iter_next(struct StrSetIter *iter);
const char *strset_iter_get(struct StrSetIter *iter);

/*
KOH_FORCE_INLINE struct StrSetIter strset_iter_new(StrSet *set);
KOH_FORCE_INLINE bool strset_iter_valid(struct StrSetIter *iter);
KOH_FORCE_INLINE void strset_iter_next(struct StrSetIter *iter);
KOH_FORCE_INLINE const char *strset_iter_get(struct StrSetIter *iter);
*/
