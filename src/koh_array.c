#include "koh_array.h"

#include <memory.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void arr_init(Array *arr, size_t esize, size_t cap) {
    assert(arr);
    memset(arr, 0, sizeof(*arr));
    arr->esize = esize;
    if (cap != 0) {
        arr->cap = cap;
        arr->data = calloc(arr->cap, arr->esize);
    }
}

void arr_shutdown(Array *arr) {
    assert(arr);
    if (arr->data) {
        free(arr->data);
        arr->data = NULL;
    }
    memset(arr, 0, sizeof(*arr));
}

void *arr_add(Array *arr) {
    assert(arr);
    if (arr->num == arr->cap) {
        arr->cap += 10;
        arr->cap *= 1.5;
        arr->data = realloc(arr->data, arr->esize * arr->cap);
    }
    return (char*)arr->data + arr->num++ * arr->esize;
}

void *arr_get(Array *arr, size_t index) {
    assert(arr);
    assert(index < arr->num);
    return (char*)arr->data + arr->esize * index;
}

void arr_set(Array *arr, size_t index, void *value) {
    assert(arr);
    assert(index < arr->num);
    memcpy((char*)arr->data + arr->esize * index, value, arr->esize);
}

