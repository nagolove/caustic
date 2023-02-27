#include "wfc.h"

#include "rand.h"

#include <math.h>
#include <assert.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int dx[] = {-1, 0, 1, 0};
static int dy[] = {0, 1, 0, -1};
static int opposite[] = {2, 3, 0, 1};

static void init(wfc_model *model);
static void ban(wfc_model *model, int i, int t);
static void observe(wfc_model *model, int node, xorshift32_state random);
static bool propagate(wfc_model *model);
static void clear(wfc_model *model);

void wfc_model_init(
    wfc_model *model, 
    int width, 
    int height, 
    int N, 
    bool periodic,
    wfc_heuristic heuristic
) {
    assert(model);
    model->MX = width;
    model->MY = height;
    model->N = N;
    model->periodic = periodic;
    model->heuristic = heuristic;
}

static void init(wfc_model *model) {
    model->wave_len = model->MX * model->MY;
    model->wave = calloc(model->wave_len, sizeof(model->wave[0]));
    model->compatible = calloc(
        model->wave_len,
        sizeof(model->compatible[0])
    );
    for (int i = 0; i < model->wave_len; i++) {
        model->wave[i] = calloc(model->T, sizeof(model->wave[0]));
        model->compatible[i] = calloc(model->T, sizeof(model->compatible[0]));
        for (int t = 0; t < model->T; t++) {
            model->compatible[i][t] = calloc(
                4, 
                sizeof(model->compatible[0][0])
            );
        }
    }
    model->distribution = calloc(model->T, sizeof(model->distribution[0]));
    model->observed = calloc(model->wave_len, sizeof(model->observed[0]));

    model->weight_log_weights = calloc(
        model->T,
        sizeof(model->weight_log_weights[0])
    );
    model->sum_of_weights = 0;
    model->sums_of_weight_log_weights = 0;

    for (int t = 0; t < model->T; t++) {
        double k = model->weights[t] * log(model->weights[t]);
        model->weight_log_weights[t] = k;
        model->sum_of_weights += model->weights[t];
        model->sum_of_weight_log_weights += model->weight_log_weights[t];
    }

    model->starting_entropy = log(model->sum_of_weights) - 
        model->sum_of_weight_log_weights / model->sum_of_weights;
    model->sums_of_ones = calloc(
        model->wave_len,
        sizeof(model->sums_of_ones[0])
    );
    model->sums_of_weights = calloc(
        model->wave_len, 
        sizeof(model->sums_of_weights[0])
    );
    model->sums_of_weight_log_weights = calloc(
        model->wave_len,
        model->sums_of_weight_log_weights[0]
    );
    model->entropies = calloc(model->wave_len, model->entropies[0]);
    model->stack = calloc(model->wave_len * model->T, sizeof(wfc_stack));
    model->stacksize = 0;
}

void wfc_model_shutdown(wfc_model *model) {
    assert(model);
    free(model->wave);
    for (int i = 0; i < model->wave_len; i++) {
        free(model->wave[i]);
        for (int t = 0; t < model->T; t++) {
            free(model->compatible[i][t]);
        }
        free(model->compatible[i]);
    }
    free(model->compatible);

    free(model->distribution);
    free(model->observed);
    free(model->weight_log_weights);
    free(model->sums_of_ones);
    free(model->sums_of_weights);
    free(model->sums_of_weight_log_weights);
    free(model->entropies);
    free(model->stack);

    memset(model, 0, sizeof(*model));
}

static void observe(wfc_model *model, int node, xorshift32_state random) {
    bool *w = model->wave[node];
    for (int t = 0; t < model->T; t++)
        model->distribution[t] = w[t] ? model->weights[t] : 0.;
    // XXX: какие значения принимает r? что за метод Random() у массива?
    //int r = distribution.Random(random.NextDouble());
    int r = 0;
    for (int t = 0; t < model->T; t++)
        if (w[t] != (t == r)) ban(model, node, t);
}

static void ban(wfc_model *model, int i, int t) {
    model->wave[i][t] = false;
    int *comp = model->compatible[i][t];
    for (int d = 0; d < 4; d++)
        comp[d] = 0;
    // XXX: как починить stack что-бы он рос
    model->stack[model->stacksize] = (wfc_stack) {
        .i1 = i,
        .t1 = t,
    };
    model->stacksize++;

    model->sums_of_ones[i] -= 1;
    model->sums_of_weights[i] -= model->weights[t];
    model->sums_of_weight_log_weights[i] -= model->weight_log_weights[t];

    double sum = model->sums_of_weights[i];
    model->entropies[i] = log(sum) - model->sums_of_weight_log_weights[i] / sum;
}

static bool propagate(wfc_model *model) {
    while (model->stacksize > 0) {
        wfc_stack top = model->stack[model->stacksize - 1];
        int i1 = top.i1, t1 = top.t1;
        model->stacksize--;

        int x1 = i1 % model->MX;
        int y1 = i1 / model->MX;

        for (int d = 0; d < 4; d++) {
            int x2 = x1 + dx[d];
            int y2 = y1 + dy[d];
            if (!model->periodic && 
                (x2 < 0 || y2 < 0 || 
                 x2 + model->N > model->MX || 
                 y2 + model->N > model->MY))
                continue;

            if (x2 < 0)
                x2 += model->MX;
            else if (x2 >= model->MX)
                x2 -= model->MY;
            if (y2 < 0)
                y2 += model->MY;
            else if (y2 >= model->MY)
                y2 -= model->MY;

            int i2 = x2 + y2 * model->MX;
            int *p = model->propagator[d][t1];
            int p_len = model->propagator_len[d][t1];
            int **compat = model->compatible[i2];

            for (int l = 0; l < p_len; l++) {
                int t2 = p[l];
                int *comp = compat[t2];
                comp[d]--;
                if (comp[d] == 0) ban(model, i2, t2);
            }
        }
    }

    return model->sums_of_ones[0] > 0;
}

static void clear(wfc_model *model) {
    for (int i = 0; i < model->wave_len; i++) {
        for (int t = 0; t < model->T; t++) {
            model->wave[i][t] = true;
            for (int d = 0; d < 4; d++) {
                /*model->compatible[i][t][d] = model->propagator[opposite[d]][t];*/
                model->compatible[i][t][d] = model->propagator_len[opposite[d]][t];
            }
        }
        model->sums_of_ones[i] = model->weights_len;
        model->sums_of_weights[i] = model->sum_of_weights;
        model->sums_of_weight_log_weights[i] = 
            model->sum_of_weight_log_weights;
        model->entropies[i] = model->starting_entropy;
        model->observed[i] = -1;
    }
    model->observed_so_far = 0;

    if (model->ground) {
        for (int x = 0; x < model->MX; x++) {
            for (int t = 0; t < model->T - 1; t++)
                ban(model, x + (model->MY - 1) * model->MX, t);
            for (int y = 0; y < model->MY - 1; y++)
                ban(model, x + y * model->MX, model->T - 1);
            propagate(model);
        }
    }
}

int next_unobserved_node(wfc_model *model, xorshift32_state random) {
    if (model->heuristic == WFC_HEURISTIC_SCANLINE) {
        for (int i = model->observed_so_far; i < model->wave_len; i++) {
            bool cond = i % model->MX + model->N > model->MX  ||
                        i / model->MX + model->N > model->MY;
            if (!model->periodic && cond)
                continue;
            if (model->sums_of_ones[i] > 1) {
                model->observed_so_far = i + 1;
                return i;
            }
        }
        return -1;
    }

    double min = 1E+4;
    int argmin = -1;
    for (int i = 0; i < model->wave_len; i++) {
        bool cond = i % model->MX + model->N > model->MX || 
                    i / model->MX + model->N > model->MY;
        if (!model->periodic && cond)
            continue;
        int remaining_values = model->sums_of_ones[i];
        double entropy = model->heuristic == WFC_HEURISTIC_ENTROPY ? 
                         model->entropies[i] : remaining_values;
        if (remaining_values > 1 && entropy <= min) {
            double noise = 1E-6 * xorshift32_rand1(&random);
            if (entropy + noise < min) {
                min = entropy + noise;
                argmin = i;
            }
        }
    }
    return argmin;
}

bool wfc_model_run(wfc_model *model, uint32_t seed, int limit) {
    if (!model->wave)
        init(model);

    clear(model);
    xorshift32_state rng = xorshift32_init();
    rng.a = seed;

    for (int l = 0; l < limit || limit < 0; l++) {
        int node = next_unobserved_node(model, rng);
        if (node >= 0) {
            observe(model, node, rng);
            bool success = propagate(model);
            if (!success)
                return false;
        } else {
            for (int i = 0; i < model->wave_len; i++)
                for (int t = 0; t < model->T; t++)
                    if (model->wave[i][t]) {
                        model->observed[i] = t;
                        break;
                        return true;
                    }
        }
    }
    return true;
}

#pragma pack(push, 1)
typedef struct PackedColor {
    unsigned char r: 8;        // Color red value
    unsigned char g: 8;        // Color green value
    unsigned char b: 8;        // Color blue value
    unsigned char a: 8;        // Color alpha value
} PackedColor;
#pragma pack(pop)

PackedColor pack_color(Color color) {
    return (PackedColor) {
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a,
    };
}

static inline uint32_t get_color(Image *img, int x, int y) {
    PackedColor pcolor = pack_color(GetImageColor(*img, x, y));
    return *((uint32_t*)&pcolor);
}

void colors_add(wfc_model_overlapping *model, uint32_t color) {
    if (model->colors_num >= model->colors_cap) {
        model->colors_cap *= 2;
        model->colors = realloc(
            model->colors, 
            sizeof(uint32_t) * model->colors_cap
        );
    }
    model->colors[model->colors_num++] = color;
}

void colors_alloc(wfc_model_overlapping *model, int cap) {
    model->colors_cap = cap;
    model->colors_num = 0;
    model->colors = calloc(model->colors_cap, sizeof(uint32_t));
}

typedef uint8_t (*PatternFunc)(wfc_model_overlapping *model, int, int);

//static uint8_t *pattern(wfc_model_overlapping *model, PatternFunc f, int N) {
    //uint8_t *result = calloc(
        //model->parent.N * model->parent.N, 
        //sizeof(result[0])
    //);
    //for (int y = 0; y < model->parent.N; y++)
        //for (int x = 0; x < model->parent.N; x++)
            //result[x + y * model->parent.N] = f(model, x, y);
    //return result;
//}

//static uint8_t *rotate_f(wfc_model_overlapping *model, int x, int y, int N) {
    //[>return model-><]
    //return NULL;
//}

static uint8_t *rotate(wfc_model_overlapping *model, uint8_t *p, int N) {
    //[>return pattern(model, <]
    return NULL;
}

static uint8_t *reflect(wfc_model_overlapping *model, uint8_t *p, int N) {
    //[>return pattern(model, <]
    return NULL;
}

//static uint8_t *sample_f(wfc_model_overlapping *model, uint8_t *p, int N) {
    //return NULL;
//}

/*
static long int hash(const uint8_t *p, int p_len, int C) {
    long int result = 0, power = 1;
    for (int i = 0; i < p_len; i++) {
        result += p[p_len - 1 - i] * power;
        power *= C;
    }
    return result;
}
*/

static void patterns_alloc(wfc_model_overlapping *model) {
    model->patterns_cap = 1000;
    model->patterns_len = 0;
    model->patterns = calloc(model->patterns_cap, sizeof(model->patterns[0]));
}

/*
static void patterns_add(wfc_model_overlapping *model, uint8_t *pattern) {
    if (model->patterns_len == model->patterns_cap) {
        model->patterns_cap *= 2;
        model->patterns = realloc(
            model->patterns, 
            sizeof(uint8_t) * model->patterns_cap
        );
    }
    model->patterns[model->patterns_len++] = pattern;
}
*/

static void patterns_free(wfc_model_overlapping *model) {
    for (int i = 0; i < model->patterns_len; i++) {
        free(model->patterns[i]);
    }
    free(model->patterns);
    model->patterns = NULL;
}

void wfc_model_overlapping_init(
    wfc_model_overlapping *model,
    Image *img,
    int N,
    int width,
    int height,
    bool periodic_input,
    bool periodic,
    int symmetry,
    bool ground,
    wfc_heuristic heuristic
) {
    assert(model);
    wfc_model_init(&model->parent, width, height, N, periodic, heuristic);
    int sx = img->width;
    int sy = img->height;

    int len = sx * sy;
    uint8_t *sample = calloc(len, sizeof(uint8_t));

    colors_alloc(model, len / 2);

    for (int i = 0; i < len; i++) {
        // XXX: корректность вычисления x и y
        int x = i / sx;
        int y = i % sx;
        int color = get_color(img, x, y);
        int k = 0;
        for (; k < model->colors_num; k++)
            if (model->colors[k] == color)
                break;
        if (k == model->colors_num) {
            colors_add(model, color);
        }
        // XXX: возможно переполнение
        sample[i] = k;
    }

    patterns_alloc(model);
    //int C = model->colors_num;
    int xmax = periodic_input ? sx : sx - N + 1;
    int ymax = periodic_input ? sy : sy - N + 1;
    for (int y = 0; y < ymax; y++)
        for (int x = 0; x < xmax; x++) {
            uint8_t ** ps = calloc(8, sizeof(ps[0]));

            //ps[0] = pattern(model, sample_f, N);
            ps[1] = reflect(model, ps[0], N);
            ps[2] = rotate(model, ps[0], N);
            ps[3] = reflect(model, ps[2], N);
            ps[4] = rotate(model, ps[2], N);
            ps[5] = reflect(model, ps[4], N);
            ps[6] = rotate(model, ps[4], N);
            ps[7] = reflect(model, ps[6], N);

            for (int k = 0; k < symmetry; k++) {
                //uint8_t *p = ps[k];
                //int p_len = 0;
                //long int h = hash(p, p_len, C);
            }
        }
}

void wfc_model_overlapping_save(wfc_model_overlapping *model, Image *img) {
}

void wfc_model_overlapping_shutdown(wfc_model_overlapping *model) {
    wfc_model_shutdown(&model->parent);
    patterns_free(model);
    /*free(model->sample);*/
}
