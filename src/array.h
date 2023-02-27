#pragma once

#include <stddef.h>
#include <stdlib.h>

typedef struct Array {
    size_t num, cap, esize;
    void *data;
} Array;

void arr_init(Array *arr, size_t esize, size_t cap);
void arr_shutdown(Array *arr);
void *arr_add(Array *arr);
void *arr_get(Array *arr, size_t index);
void arr_set(Array *arr, size_t index, void *value);
