// vim: set colorcolumn=85
// vim: fdm=marker

/*
// {{{
TODO Добавить параметры генерации: 
    30% - море, 30% - горы. 30% - суша.
    10% - море, 10% - горы. 80% - суш.
// }}}
*/

// {{{ Includes
#include "koh_das.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <threads.h>

#include <raylib.h>

#include "koh_das_interface.h"
#include "koh_rand.h"
// }}}
//
typedef struct DAS_Context {
    double           *map; // 2d array of heights
    int              mapSize;
    int              chunkSize, initialChunkSize;
    xorshift32_state rnd;
    int              mapn;
    double           elapsedtime;
} DAS_Context;

static const double SUPER_MIN = -99999;
static const double SUPER_MAX =  99999;

static void print_map(DAS_Context *ctx, int filenum) {
    assert(filenum >= 0);
    char fname[64] = {0, };
    sprintf(fname, "map.c.%d.txt", filenum);

    FILE *file = fopen(fname, "w+");
    //for(int i = 0; i < ctx->chunkSize; i++) {
        //for(int j = 0; j < ctx->chunkSize; j++) {
    for(int i = 0; i < ctx->mapSize; i++) {
        for(int j = 0; j < ctx->mapSize; j++) {
            /*printf("%f ", ctx->map[ctx->mapSize * i + j]);*/
            fprintf(file, "%f ", ctx->map[ctx->mapSize * i + j]);
        }
        printf("\n");
        fprintf(file, "\n");
    }
    fclose(file);
}

// Освобождает внутренние структуры данных если объект больше не будет
// использоваться.
void das_free(DAS_Context *ctx) {
    if (ctx->map) {
        free(ctx->map);
        ctx->map = NULL;
    }
}

inline static double map_set(DAS_Context *ctx, int i, int j, double value) {
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

inline static double map_get(DAS_Context *ctx, int i, int j) {
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

double internal_random(DAS_Context *ctx) {
    /*double value = rand() / (double)RAND_MAX;*/
    double value = (double)xorshift32_rand(&ctx->rnd) / (double)UINT32_MAX;
    /*printf("internal_random = %.3f\n", value);*/
    return value;
}

double random_range(DAS_Context *ctx, double min, double max) {
	/*local r = 4*(self.rng:random()-0.5)^3 + 0.5*/
/*--	https://www.desmos.com/calculator/toxjtsovev*/
    double value = min + internal_random(ctx) * (max - min);
    /*printf("random_range %f, %f, %f\n", min, max, value);*/
    return value;
}

void diamond_and_square_reset(DAS_Context *ctx) {
    ctx->chunkSize = ctx->initialChunkSize;

    int limit = ctx->mapSize - 1;
    struct {
        int i, j;
    } corners[4] = {
        { .i = 0, .j = 0},
        { .i = limit, .j = 0},
        { .i = limit, .j = limit},
        { .i = 0, .j = limit},
    };

    // XXX Использование константы в сравнеии (i < 4), лучше использовать
    // переменную, расчитанную от длины массива.
    for(int corner_idx = 0; corner_idx < 4; ++corner_idx) {
        int i = corners[corner_idx].i;
        int j = corners[corner_idx].j;

        double rnd_value = internal_random(ctx);
        // TODO исправить математику, откуда такие коэффициенты?
        /*rnd_value = 0.5 - 0.5 * cos(rnd_value * M_PI);*/
        printf("corner value %f\n", rnd_value);
        /*LOG("i = %d, j = %d\n", i, j);*/
        map_set(ctx, i, j, rnd_value);
    }
}

void das_init(DAS_Context *ctx, int mapn, xorshift32_state *rnd) {
    // {{{
    assert(ctx);

    ctx->mapSize = pow(2, mapn) + 1;
    ctx->mapn = mapn;
    ctx->initialChunkSize = ctx->mapSize - 1;
    ctx->chunkSize = ctx->initialChunkSize;
    ctx->map = calloc(sizeof(double), ctx->mapSize * ctx->mapSize);
    ctx->rnd = rnd ? *rnd : xorshift32_init();

    printf(
            "allocated %.2f MB\n", 
            sizeof(double) * ctx->mapSize * ctx->mapSize / 1024 / 1024.
    );

    /*
    const double init_value = SUPER_MIN; // Каким значением инициализировать?
    for(int i = 0; i < ctx->mapSize; i++) {
        for(int j = 0; j < ctx->mapSize; j++) {
            map_set(ctx, i, j, init_value);
        }
    }
    // */

    diamond_and_square_reset(ctx);
    // }}}
}

inline static double *value(DAS_Context *ctx, int i, int j) {
    /*if (i >= 0 && i < ctx->mapSize && j >= 0 && j < ctx->mapSize) {*/
    if (i >= 0 && i < ctx->mapSize && j >= 0 && j < ctx->mapSize) {
        //if (ctx->map[i * ctx->mapSize + j] > SUPER_MIN) {
            //return &ctx->map[i * ctx->mapSize + j];
        if (ctx->map[j * ctx->mapSize + i] > SUPER_MIN) {
            return &ctx->map[j * ctx->mapSize + i];
        } else {
            return NULL;
        }
    } else {
        TraceLog(LOG_WARNING, "value is NULL for [%d, %d]\n", i, j);
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

void normalize_implace(DAS_Context *ctx) {
    for(int i = 0; i < ctx->mapSize - 1; ++i) {
        for(int j = 0; j < ctx->mapSize - 1; ++j) {
            double *v = value(ctx, i, j);
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

void square_value(DAS_Context *ctx, int i, int j, double *min, double *max) {
    assert(min);
    assert(max);

    *min = SUPER_MAX;
    *max = SUPER_MIN;

    // Увеличение индексов
    // TODO Стоит вынести массив corners из функции и передавать как аргумент?
    struct {
        int i, j;
    } corners[4] = {
        { .i = i, .j = j},
        { .i = i + ctx->chunkSize, .j = j },
        { .i = i, .j = j + ctx->chunkSize },
        { .i = i + ctx->chunkSize, .j = j + ctx->chunkSize },
    };

    for(int corner_idx = 0; corner_idx < 4; ++corner_idx) {
        double *v = value(ctx, corners[corner_idx].i, corners[corner_idx].j);
        if (v) {
            *min = min_value(*min, *v);
            *max = max_value(*max, *v);
        }
    }
}

void diamond_value(
        DAS_Context *ctx, 
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
        {.i = i, .j = j - half}, 
        {.i = i  +  half, .j = j}, 
        {.i = i, .j = j  +  half}, 
        {.i = i - half, .j = j},
    };

    for(int corner_idx = 0; corner_idx < 4; ++corner_idx) {
        double *v = value(ctx, corners[corner_idx].i, corners[corner_idx].j);
        if (v) {
            *min = min_value(*min, *v);
            *max = max_value(*max, *v);
        }
    }
}

void square(DAS_Context *ctx) {
    TraceLog(LOG_INFO, "square\n");
    int half = floor(ctx->chunkSize / 2.);
    // XXX Использовать -1 или -2 ??
    for(int i = 0; i < ctx->mapSize - 1; i += ctx->chunkSize) {

#if !defined(PLATFORM_WEB)
        // XXX: Правильное время стоит?
        thrd_sleep(&(struct timespec){ .tv_nsec =  999999999}, NULL);
        //nanosleep(&(struct timespec){ .tv_sec = 0., .tv_nsec = 10000000000000}, NULL);
        //printf("blaaa\n");
        /*
        // Зачем использовать pselect()?
        pselect(
            0, NULL, NULL, NULL, 
            &(struct timespec){ .tv_sec = 0., .tv_nsec = 10000000000000}, 
            NULL
        );
        */
        //thrd_sleep((struct timespec*)&(struct timeval){ .tv_usec = 100000}, NULL);
#endif
        if (ctx->mapn > 10) {
            thrd_sleep(&(struct timespec){.tv_nsec = 1000000}, NULL);
        }

        for(int j = 0; j < ctx->mapSize - 1; j += ctx->chunkSize) {
    /*for(int i = 0; i < ctx->mapSize - 2; i += ctx->chunkSize) {*/
        /*for(int j = 0; j < ctx->mapSize - 2; j += ctx->chunkSize) {*/
            double min = 0., max = 0.;
            square_value(ctx, i, j, &min, &max);
            double rnd_value = random_range(ctx, min, max);
            map_set(ctx, i + half, j + half, rnd_value);
        }
    }
}

bool diamond(DAS_Context *ctx) {
    TraceLog(LOG_INFO, "diamond\n");
    // Деление чанков пополам, внутренние точки
    int half = floor(ctx->chunkSize / 2.); 
    TraceLog(LOG_INFO, "half = %d\n", half);
    int mapSize = ctx->mapSize;
    int chunkSize = ctx->chunkSize;
    for(int i = 0; i < mapSize - 1; i += half) {

        if (ctx->mapn > 10) {
            thrd_sleep(&(struct timespec){.tv_nsec = 100}, NULL);
        }

        for(int j = (i + half) % chunkSize; j < mapSize - 1; j += chunkSize) {
            /*LOG("i: %d j: %d\n", i, j);*/
            double min = 0., max = 0.;
            diamond_value(ctx, i, j, half, &min, &max);
            /*LOG("min, max %f, %f\n", min, max);*/
            double rnd_value = random_range(ctx, min, max);
            map_set(ctx, i, j, rnd_value);
        }
    }

    TraceLog(LOG_INFO, "ctx->chunkSize = %d\n", ctx->chunkSize);
    ctx->chunkSize = ceil(ctx->chunkSize / 2.);

    return ctx->chunkSize <= 1;
}

bool das_flat(DAS_Context *ctx, double flat) {
    for (int i = 0; i < ctx->mapSize; i++) 
        for (int j = 0; j < ctx->mapSize; j++)
            map_set(ctx, i, j, flat);

    return true;
}

void das_eval(DAS_Context *ctx, double flat) {
    //printf("das_eval\n");
    assert(ctx->map);

    if (flat > 0) {
        das_flat(ctx, flat);
        return;
    }

    SetTraceLogLevel(LOG_ERROR);

    double time1 = GetTime();
    diamond_and_square_reset(ctx);
    int filenum = 0;
    /*print_map(ctx, filenum++);*/

    bool stop = false;
    do {
        square(ctx);

        /*print_map(ctx, filenum++);*/

        stop = diamond(ctx);

        /*print_map(ctx, filenum++);*/
    } while (!stop);

    normalize_implace(ctx);
    double time2 = GetTime();
    ctx->elapsedtime = time2 - time1;
    print_map(ctx, filenum++);

    SetTraceLogLevel(LOG_ALL);
}

void das_set(DAS_Context *ctx, int i, int j, double value) {
    const char *format_msg = "das_set: '%s' out of range 0..%d";

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

double das_get(DAS_Context *ctx, int i, int j) {
    /*
    const char *format_msg = "diamond_and_square_get: '%s' out of range 0..%d";

    if (i < 0 || i >= ctx->mapSize) {
        printf(format_msg, "i", ctx->mapSize);
        abort();
    }

    if (j < 0 || j >= ctx->mapSize) {
        printf(format_msg, "j", ctx->mapSize);
        abort();
    }
    */

    return map_get(ctx, i, j);
}

int das_get_mapsize(DAS_Context *ctx) {
    assert(ctx);
    return ctx->mapSize;
}

char *das_get_state(DAS_Context *ctx) {
    assert(ctx);
    static char buff[200] = {0};
    sprintf(buff, "%d, %d", ctx->mapn, ctx->rnd.a);
    return buff;
}

static double get_elapsedtime(DAS_Context *ctx) {
    return ctx->elapsedtime;
}

static bool init_from_cache(void *ctx, int mapn, xorshift32_state *rnd) {
    //printf("init_from_cache: st\n");
    perror("init_from_cache: unimplemented");
    exit(EXIT_FAILURE);
    return false;
}

static void cache(void *ctx) {
    perror("cache: unimplemented");
    exit(EXIT_FAILURE);
}

DAS_Interface das_interface_init(void) {
    DAS_Interface c = {0};
    c.eval_from_cache = init_from_cache;
    c.cache = cache;
    c.init = (das_init_t)das_init;
    c.free = (das_free_t)das_free;
    c.eval = (das_eval_t)das_eval;
    c.set = (das_set_t)das_set;
    c.get = (das_get_t)das_get;
    c.get_mapsize = (das_get_mapsize_t)das_get_mapsize;
    c.get_state = (das_get_state_t)das_get_state;
    c.get_elapsedtime = (das_get_elapsedtime_t)get_elapsedtime;
    c.ctx = calloc(1, sizeof(DAS_Context));
    return c;
}

void das_interface_shutdown(DAS_Interface *ctx) {
    assert(ctx);
    if (ctx->ctx) {
        free(ctx->ctx);
        ctx->ctx = NULL;
    }
}
