#pragma once

#include <stdbool.h>

typedef struct StrSet StrSet;
// Если возвращает ложь, то элемент удаляется из множества
typedef bool (*StrSetEachCallback)(const char *key, void *udata);

StrSet *strset_new();
bool strset_exist(StrSet *set, const char *key);
void strset_add(StrSet *set, const char *key);
void strset_clear(StrSet *set);
void strset_free(StrSet *set);
void strset_remove(StrSet *set, const char *key);
void strset_each(StrSet *set, StrSetEachCallback cb, void *udata);
