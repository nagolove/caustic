#pragma once

#include <stdbool.h>

typedef enum koh_SetAction {
    koh_SA_next,
    koh_SA_break,
    koh_SA_remove_next,
    koh_SA_remove_break,
} koh_SetAction;

typedef struct koh_Set koh_Set;
// Если возвращает ложь, то элемент удаляется из множества
typedef koh_SetAction (*koh_SetEachCallback)(
    const void *key, int key_len, void *udata
);

typedef enum koh_SetResult {
    koh_SR_badargs = -1,
    koh_SR_exists  = 0,
    koh_SR_added   = 1,
} koh_SetResult;

koh_Set *set_new();
bool set_exist(koh_Set *set, const void *key, int key_len);
koh_SetResult set_add(koh_Set *set, const void *key, int key_len);
void set_clear(koh_Set *set);
void set_free(koh_Set *set);
bool set_remove(koh_Set *set, const void *key, int key_len);
void set_each(koh_Set *set, koh_SetEachCallback cb, void *udata);
bool set_compare(const koh_Set *s1, const koh_Set *s2);
int set_size(koh_Set *set);
