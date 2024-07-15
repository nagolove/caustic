#pragma once

#include "koh_hashers.h"
#include <stdbool.h>

/*
TODO: Попробовать сделать гомогенное множество, с хранением элементов 
множества в одном кусочке памяти.
 */

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

struct koh_SetSetup {
    HashFunction hash_func;
    // TODO: Добавить функцию обратного вызова используюмую 
    // при удалении элемента.
    void (*on_key_free)(const void *key, int key_len);
};

// TODO: Заменить int на size_t в key_len

koh_Set *set_new(struct koh_SetSetup *setup);
bool set_exist(koh_Set *set, const void *key, int key_len);
// Данные копируются в множество.
koh_SetResult set_add(koh_Set *set, const void *key, int key_len);
void set_clear(koh_Set *set);
void set_free(koh_Set *set);
bool set_remove(koh_Set *set, const void *key, int key_len);
void set_each(koh_Set *set, koh_SetEachCallback cb, void *udata);
bool set_compare(const koh_Set *s1, const koh_Set *s2);
int set_size(const koh_Set *set);

/*
    // Пример использования koh_SetView
    for (struct koh_SetView v = set_each_begin(set);
        set_each_valid(&v); set_each_next(&v)) {
        const void *key = set_each_key(&v);
        int key_len = set_each_key_len(&v);
    }
*/
struct koh_SetView {
    koh_Set *set;
    int     i;
    bool    finished;
};


struct koh_SetView set_each_begin(koh_Set *set);
bool set_each_valid(struct koh_SetView *v);
void set_each_next(struct koh_SetView *v);
const void *set_each_key(struct koh_SetView *v);
int set_each_key_len(struct koh_SetView *v);

extern bool koh_set_view_verbose;
