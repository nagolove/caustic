#pragma once

#include <stdbool.h>

typedef enum StrSetAction {
    SSA_next,
    SSA_remove,
    SSA_break,
} StrSetAction;

typedef struct StrSet StrSet;
// Если возвращает ложь, то элемент удаляется из множества
typedef StrSetAction (*StrSetEachCallback)(const char *key, void *udata);

StrSet *strset_new();
bool strset_exist(const StrSet *set, const char *key);
void strset_add(StrSet *set, const char *key);
void strset_clear(StrSet *set);
void strset_free(StrSet *set);
void strset_remove(StrSet *set, const char *key);
void strset_each(StrSet *set, StrSetEachCallback cb, void *udata);
bool strset_compare(const StrSet *s1, const StrSet *s2);
void strset_extend(StrSet *set);
