#pragma once

#include <stdbool.h>

#include "koh_rand.h"

typedef void (*das_init_t)(void *ctx, int mapn, xorshift32_state *rnd);
typedef void (*das_free_t)(void *ctx);
typedef double (*das_get_t)(void *ctx, int i, int j);
typedef void (*das_set_t)(void *ctx, int i, int j, double value);
// Если flat > 0. то установить все высоты равными значению flat
typedef void (*das_eval_t)(void *ctx, double flat);
typedef bool (*das_is_inited_t)(void *ctx);
typedef int (*das_get_mapsize_t)(void *ctx);
typedef char *(*das_get_state_t)(void *ctx);
//typedef bool (*das_eval2_t) (void *ctx);
typedef void (*das_set_threadsnum_t)(void *ctx, int num);
typedef int (*das_get_threadsnum_t)(void *ctx);
typedef double (*das_get_elapsedtime_t)(void *ctx);

typedef bool (*das_eval_from_cache_t)(
    void *ctx, int mapn, xorshift32_state *rnd
);
typedef void (*das_cache_t)(void *ctx);

typedef struct {
    das_eval_from_cache_t   eval_from_cache;
    das_cache_t             cache;
    das_init_t              init;
    das_free_t              free;
    das_get_t               get;
    das_set_t               set;
    das_eval_t              eval;
    das_get_mapsize_t       get_mapsize;
    das_get_state_t         get_state;
    das_set_threadsnum_t    set_threadsnum;
    das_get_threadsnum_t    get_threadsnum;
    das_get_elapsedtime_t   get_elapsedtime;
    void                    *ctx;
} DAS_Interface;
      

