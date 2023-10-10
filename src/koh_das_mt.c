// vim: set colorcolumn=85
// vim: fdm=marker

/*
TODO Добавить параметры генерации: 
    30% - море, 30% - горы. 30% - суша.
    10% - море, 10% - горы. 80% - суш.
*/

// {{{ Includes
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "raylib.h"

#include "koh_das_interface.h"
#include "koh_das_mt.h"
#include "koh_rand.h"
// }}}

#include <threads.h>

typedef struct Thread_Context Thread_Context;

typedef enum ThreadState {
    THREAD_STATE_SPIN    = 0,
    THREAD_STATE_DIAMOND = 1,
    THREAD_STATE_SQUARE  = 2,
    THREAD_STATE_EXIT    = 3,
} ThreadState;

typedef struct {
    double           *map; // 2d array of heights
    //bool             *is_set_map;
    int              mapSize;
    int              step_size, initial_step_size;

    xorshift32_state rnd;
    int              mapn;

    Thread_Context   *threads;
    int              threadsnum;
    double           elapsedtime;
} DAS_Context_mt;

typedef struct Thread_Context {
    _Atomic(int)   state;
    thrd_t         thrd;
    int            num;
    int            starti, startj, finali, finalj;
    /*int            maxsteps, steps;*/
    DAS_Context_mt *ctx;
} Thread_Context;

static const char *cache_path = "assets/";
static const char *cache_dirname = "cache";
static const double SUPER_MIN = -99999;
static const double SUPER_MAX =  99999;

void dasmt_free(DAS_Context_mt *ctx) {
    if (ctx->map) {
        free(ctx->map);
        //free(ctx->is_set_map);
        ctx->map = NULL;
    }
}

int worker(void *p);

inline static double map_set(DAS_Context_mt *ctx, int i, int j, double value) {
#ifdef NDEBUG
    if (i < 0 || i >= ctx->mapSize) {
        printf("map_set(): i out of range 0..%d\n", ctx->mapSize);
        abort();
    }
    if (j < 0 || j >= ctx->mapSize) {
        printf("map_set(): j out of range 0..%d\n", ctx->mapSize);
        abort();
    }
#endif
    return ctx->map[j * ctx->mapSize + i] = value;
}

inline static double map_get(DAS_Context_mt *ctx, int i, int j) {
#ifdef NDEBUG
    if (i < 0 || i >= ctx->mapSize) {
        printf("map_get(): i out of range 0..%d\n", ctx->mapSize);
        abort();
    }
    if (j < 0 || j >= ctx->mapSize) {
        printf("map_get(): j out of range 0..%d\n", ctx->mapSize);
        abort();
    }
#endif
    return ctx->map[j * ctx->mapSize + i];
}

static double internal_random(DAS_Context_mt *ctx) {
    double v = (double)xorshift32_rand(&ctx->rnd) / (double)UINT32_MAX;
    return v;
}

static double random_range(DAS_Context_mt *ctx, double min, double max) {
    return min + internal_random(ctx) * (max - min);
}

static void reset(DAS_Context_mt *ctx) {
    ctx->step_size = ctx->initial_step_size;

    int limit = ctx->mapSize - 1;
    struct {
        int i, j;
    } corners[4] = {
        { .i = 0, .j = 0},
        { .i = limit, .j = 0},
        { .i = limit, .j = limit},
        { .i = 0, .j = limit},
    };

    for(int corner_idx = 0; corner_idx < 4; ++corner_idx) {
        int i = corners[corner_idx].i;
        int j = corners[corner_idx].j;

        double rnd_value = internal_random(ctx);
        map_set(ctx, i, j, rnd_value);
    }
}

static void dasmt_init(DAS_Context_mt *ctx, int mapn, xorshift32_state *rnd) {
    assert(ctx);

    ctx->mapSize = pow(2, mapn) + 1;
    ctx->mapn = mapn;
    ctx->initial_step_size = ctx->mapSize - 1;
    ctx->step_size = ctx->initial_step_size;
    ctx->map = calloc(sizeof(double), ctx->mapSize * ctx->mapSize);
    //ctx->is_set_map = calloc(sizeof(bool), ctx->mapSize * ctx->mapSize);
    ctx->threadsnum = 2;

    printf(
        "map allocated %zu\n",
        sizeof(double) * ctx->mapSize * ctx->mapSize
    );

    ctx->rnd = rnd ? *rnd : xorshift32_init();

    printf(
            "allocated %.2f MB\n", 
            sizeof(double) * ctx->mapSize * ctx->mapSize / 1024. / 1024.
    );

    //reset(ctx);
}

inline static double *value(DAS_Context_mt *ctx, int i, int j) {
    if (i >= 0 && i < ctx->mapSize && j >= 0 && j < ctx->mapSize) {
        if (ctx->map[j * ctx->mapSize + i] > SUPER_MIN) {
            return &ctx->map[j * ctx->mapSize + i];
        } else {
            printf("value == NULL\n");
            return NULL;
        }
    } else {
        //TraceLog(LOG_WARNING, "value is NULL for [%d, %d]\n", i, j);
        return NULL;
    }
}

inline static double min_value(double a, double b) {
    /*return a < b ? a : b;*/

    if (a < b) {
        return a;
    } else {
        return b;
    }

}

inline static double max_value(double a, double b) {
    /*return a > b ? a : b;*/

    if (a > b) {
        return a;
    } else {
        return b;
    }

}

static void normalize_implace(DAS_Context_mt *ctx) {
    for(int i = 0; i < ctx->mapSize - 1; ++i) {
        for(int j = 0; j < ctx->mapSize - 1; ++j) {
            const double *v = value(ctx, i, j);
            if (v) {
                if (*v > 1.) {
                    map_set(ctx, i, j, 1.);
                } else if (*v < 0) {
                    map_set(ctx, i, j, 0.);
                }
            }
        }
    }
}

static void square_value(DAS_Context_mt *ctx, int i, int j, double *min, double *max) {
    assert(min);
    assert(max);

    *min = SUPER_MAX;
    *max = SUPER_MIN;

    struct {
        int i, j;
    } corners[4] = {
        { 
            .i = i,
            .j = j
        },
        { 
            .i = i + ctx->step_size,
            .j = j 
        },
        { 
            .i = i,
            .j = j + ctx->step_size 
        },
        { 
            .i = i + ctx->step_size,
            .j = j + ctx->step_size 
        },
    };

    for(int corner_idx = 0; corner_idx < 4; ++corner_idx) {
        const double *v = value(ctx, corners[corner_idx].i, corners[corner_idx].j);
        if (v) {
            *min = min_value(*min, *v);
            *max = max_value(*max, *v);
        }
    }
}

static void diamond_value(
        DAS_Context_mt *ctx, 
        int i, int j, int half, 
        double *min, double *max
) {
    assert(min);
    assert(max);

    *min = SUPER_MAX;
    *max = SUPER_MIN;

    struct {
        int i, j;
    } corners[4] = {
        {
            .i = i, 
            .j = j - half
        }, 
        {
            .i = i  +  half,
            .j = j
        }, 
        {   
            .i = i,
            .j = j  +  half
        }, 
        {
            .i = i - half,
            .j = j
        },
    };

    for(int corner_idx = 0; corner_idx < 4; ++corner_idx) {
        const double *v = value(ctx, corners[corner_idx].i, corners[corner_idx].j);
        if (v) {
            *min = min_value(*min, *v);
            *max = max_value(*max, *v);
        }
    }
}

static void square(DAS_Context_mt *ctx, int starti, int finali) {
    //TraceLog(LOG_INFO, "square\n");
    int half = floor(ctx->step_size / 2.);
    //for(int i = 0; i < ctx->mapSize - 1; i += ctx->step_size) {
    for(int i = starti; i < finali; i += ctx->step_size) {
        for(int j = 0; j < ctx->mapSize - 1; j += ctx->step_size) {
            double min = 0., max = 0.;
            square_value(ctx, i, j, &min, &max);
            double rnd_value = random_range(ctx, min, max);
            map_set(ctx, i + half, j + half, rnd_value);
        }
    }
}

static void diamond(DAS_Context_mt *ctx, int starti, int finali) {
    int half = floor(ctx->step_size / 2.); 
    int mapSize = ctx->mapSize;
    int step_size = ctx->step_size;
    //XXX: или цикл до mapSize - 1 ??
    //for(int i = 0; i < mapSize; i += half) {
    for(int i = starti; i < finali; i += half) {
        /*printf("diamond i = %d\n", i);*/
        for(int j = (i + half) % step_size; j < mapSize - 1; j += step_size) {
            //printf("diamond j = %d\n", i);
            double min = 0., max = 0.;
            diamond_value(ctx, i, j, half, &min, &max);
            double rnd_value = random_range(ctx, min, max);
            map_set(ctx, i, j, rnd_value);
        }
    }
}

void wait_for_spin(DAS_Context_mt *ctx) {
    while (true) {
        int counter = 0;
        for (int i = 0; i < ctx->threadsnum; ++i) {
            Thread_Context *tx = &ctx->threads[i];
            int state = atomic_load(&tx->state);
            if (state == THREAD_STATE_SPIN) {
                counter++;
            }
        }
        //printf("counter %d\n", counter);
        if (counter == ctx->threadsnum)
            break;
    }
    // */

    /*
    int done[ctx->threadsnum];
    memset(done, 0, sizeof(bool) * ctx->threadsnum);
    int done_num = ctx->threadsnum;

    for (int i = 0; i < done_num; i++) {
        done[i] = i;
    }

    while (true) {
        int counter = 0;
        for (int i = 0; i < done_num; ++i) {
            Thread_Context *tx = &ctx->threads[done[i]];
            int state = atomic_load(&tx->state);
            if (state == THREAD_STATE_SPIN) {

                if (counter == done_num)
                    goto exit;

                counter++;
                done[i] = done[done_num - 1];
                done_num--;
            }
        }
        //printf("counter %d\n", counter);
        //if (counter == ctx->threadsnum)
        if (counter == done_num)
            break;
    }

exit:
    return;
    */
    //printf("wait_for_spin\n");
}

static bool dasmt_flat(DAS_Context_mt *ctx, double flat) {
    for (int i = 0; i < ctx->mapSize; i++) 
        for (int j = 0; j < ctx->mapSize; j++)
            map_set(ctx, i, j, flat);

    return true;
}

static bool dasmt_eval(DAS_Context_mt *ctx, double flat) {
    assert(ctx);
    assert(ctx->map);
    printf("eval\n");

    if (flat > 0.)
        return dasmt_flat(ctx, flat);

    double time1 = GetTime();

    /*ctx->threadsnum = 2;*/
    ctx->threads = calloc(ctx->threadsnum, sizeof(ctx->threads[0]));
    /*int max_steps_per_thread = 128;*/
    /*int steps_per_subdivision = 1;*/

    for (int i = 0; i < ctx->threadsnum; ++i) {
        Thread_Context *tx = &ctx->threads[i];
        tx->state = THREAD_STATE_SPIN;
        tx->num = i;
        tx->ctx = ctx;
        thrd_create(&tx->thrd, worker, tx);
    }

    reset(ctx);

    int sq_starti = 0, sq_finali = ctx->mapSize - 1;
    int dm_starti = 0, dm_finali = ctx->mapSize;
    //XXX: Работает только при threadsnum == 2 ^ n
    int sq_inc = (sq_finali - sq_starti) / ctx->threadsnum;
    int dm_inc = (dm_finali - dm_starti) / ctx->threadsnum;

    /*printf("sq_inc %d\n", sq_inc);*/
    /*printf("dm_inc %d\n", dm_inc);*/

    while (ctx->step_size > 1) {
        /*printf("step_size %d\n", ctx->step_size);*/

        for (int i = 0; i < ctx->threadsnum; ++i) {
            Thread_Context *tx = &ctx->threads[i];
            tx->starti = sq_starti + i * sq_inc;
            tx->finali = sq_starti + (i + 1) * sq_inc;
            atomic_store(&tx->state, THREAD_STATE_SQUARE);
        }

        wait_for_spin(ctx);

        for (int i = 0; i < ctx->threadsnum; ++i) {
            Thread_Context *tx = &ctx->threads[i];
            tx->starti = dm_starti + i * dm_inc;
            tx->finali = dm_starti + (i + 1) * dm_inc;
            atomic_store(&tx->state, THREAD_STATE_DIAMOND);
        }

        wait_for_spin(ctx);

        ctx->step_size = ceil(ctx->step_size / 2.);
        /*steps_per_subdivision *= 2;*/
    } 

    for (int i = 0; i < ctx->threadsnum; ++i) {
        Thread_Context *tx = &ctx->threads[i];
        atomic_store(&tx->state, THREAD_STATE_EXIT);
    }

    for (int i = 0; i < ctx->threadsnum; ++i) {
        Thread_Context *tx = &ctx->threads[i];
        thrd_join(tx->thrd, NULL);
    }

    normalize_implace(ctx);

    free(ctx->threads);

    double time2 = GetTime();
    ctx->elapsedtime = time2 - time1;
    /*printf("eval for %f\n", time2 - time1);*/
    return true;
}

void dasmt_set(DAS_Context_mt *ctx, int i, int j, double value) {
    const char *format_msg = "diamond_and_square_set: '%s' out of range 0..%d";

    if (i < 0 || i >= ctx->mapSize) {
        printf(format_msg, "i", ctx->mapSize);
        abort();
    }

    if (j < 0 || j >= ctx->mapSize) {
        printf(format_msg, "j", ctx->mapSize);
        abort();
    }

    map_set(ctx, i, j, value);
}

double dasmt_get(DAS_Context_mt *ctx, int i, int j) {
    const char *format_msg = "diamond_and_square_get: '%s' out of range 0..%d";

    if (i < 0 || i >= ctx->mapSize) {
        printf(format_msg, "i", ctx->mapSize);
        abort();
    }

    if (j < 0 || j >= ctx->mapSize) {
        printf(format_msg, "j", ctx->mapSize);
        abort();
    }

    return map_get(ctx, i, j);
}

int dasmt_get_mapsize(DAS_Context_mt *ctx) {
    assert(ctx);
    return ctx->mapSize;
}

char *dasmt_get_state(DAS_Context_mt *ctx) {
    assert(ctx);
    static char buff[200] = {0};
    sprintf(buff, "%d, %d", ctx->mapn, ctx->rnd.a);
    return buff;
}

static double get_elapsedtime(DAS_Context_mt *ctx) {
    return ctx->elapsedtime;
}

static int get_threadsnum(DAS_Context_mt *ctx) {
    return ctx->threadsnum;
}

static void set_threadsnum(DAS_Context_mt *ctx, int num) {
    ctx->threadsnum = num;
}

static const char *get_cache_fname(DAS_Context_mt *ctx) {
    char prefix[64] = {0};
    sprintf(prefix, "%d_%u.binary", ctx->mapn, ctx->rnd.a);
    static char filename[256] = {0};
    memset(filename, 0, sizeof(filename));
    strcat(filename, cache_path);
    strcat(filename, cache_dirname);
    strcat(filename, prefix);
    return filename;
}

static bool eval_from_cache(void *tmp_ctx, int mapn, xorshift32_state *rnd) {
    printf("init_from_cache: mt\n");
    DAS_Context_mt *ctx = tmp_ctx;
    assert(ctx);

    const char *filename = get_cache_fname(ctx);

    if (!ctx->map) {
        perror("eval_from_cache: ctx->map == NULL\n");
    }

    FILE *datafile = fopen(filename, "r");

    if (datafile) {
        int num = ctx->mapSize * ctx->mapSize;
        if (fread(ctx->map, sizeof(ctx->map[0]), num, datafile) == num) {
            printf("map read from cache successfully\n");
        }
        fclose(datafile);
        return true;
    }

    return false;
}

static bool mk_cache_dir() {
    struct stat st = {0};

    char path[256] = {0};
    strcat(path, cache_path);
    strcat(path, cache_dirname);

    if (stat(path, &st) == -1) {
        //S_IRWXU | S_IRWXG | S_IRWXO
        if (mkdir(path, S_IRWXU) == -1) {
            printf(
                "mk_cache_dir: path '%s' failed with %s", 
                path,
                strerror(errno)
            );
            return false;
        }
    }

    return true;
}

static void cache(void *tmp_ctx) {
    DAS_Context_mt *ctx = tmp_ctx;
    if (!ctx->map)
        return;

    if (!mk_cache_dir())
        return;

    const char *filename = get_cache_fname(ctx);
    printf("cache: mt '%s'\n", filename);

    FILE *file = fopen(filename, "w");
    if (!file) return;

    int num = ctx->mapSize * ctx->mapSize;
    if (fwrite(ctx->map, sizeof(ctx->map[0]), num, file) == num) {
        printf("map cached successfully\n");
    }
    fclose(file);
}

DAS_Interface dasmt_interface_init(void) {
    DAS_Interface c = {0};
    c.eval_from_cache = eval_from_cache;
    c.cache = cache;
    c.init = (das_init_t)dasmt_init;
    c.free = (das_free_t)dasmt_free;
    c.get = (das_get_t)dasmt_get;
    c.set = (das_set_t)dasmt_set;
    c.get_mapsize = (das_get_mapsize_t)dasmt_get_mapsize;
    c.get_state = (das_get_state_t)dasmt_get_state;
    c.eval = (das_eval_t)dasmt_eval;
    c.get_elapsedtime = (das_get_elapsedtime_t)get_elapsedtime;
    c.get_threadsnum = (das_get_threadsnum_t)get_threadsnum;
    c.set_threadsnum = (das_set_threadsnum_t)set_threadsnum;
    c.ctx = calloc(1, sizeof(DAS_Context_mt));
    return c;
}

void dasmt_interface_shutdown(DAS_Interface *ctx) {
    assert(ctx);
    if (ctx->ctx) {
        free(ctx->ctx);
        ctx->ctx = NULL;
    }
}

int worker(void *p) {
    Thread_Context *tx = p;
    DAS_Context_mt *ctx = tx->ctx;

    while (true) {
        int state = atomic_load(&tx->state);
        switch (state) {
            case THREAD_STATE_EXIT: {
                goto exit;
            }
            case THREAD_STATE_SPIN: {
                thrd_sleep(&(struct timespec){.tv_nsec = 100}, NULL);
                //thrd_yield();
                break;
            }
            case THREAD_STATE_SQUARE: {
                square(ctx, tx->starti, tx->finali);
                atomic_store(&tx->state, THREAD_STATE_SPIN);
                break;
            }
            case THREAD_STATE_DIAMOND: {
                diamond(ctx, tx->starti, tx->finali);
                atomic_store(&tx->state, THREAD_STATE_SPIN);
                /*printf("diamond\n");*/
                break;
            }
        }
    }

exit:
    return 0;
}
