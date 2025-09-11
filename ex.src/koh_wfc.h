#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "raylib.h"

typedef struct wfc_stack {
    int i1, t1;
} wfc_stack;

typedef enum wfc_heuristic {
    WFC_HEURISTIC_ENTROPY,
    WFC_HEURISTIC_MRV,
    WFC_HEURISTIC_SCANLINE,
} wfc_heuristic;

typedef struct wfc_arr {
} wfc_arr;

typedef struct wfc_model {
    int wave_len;
    bool **wave;
    int ***propagator;
    int **propagator_len;
    int ***compatible;
    int *observed;
    wfc_stack *stack;
    int stacksize, observed_so_far;

    int MX, MY, T, N;
    bool periodic, ground;

    double *weights;
    int weights_len;
    double *weight_log_weights, *distribution;

    int *sums_of_ones;
    double sum_of_weights, sum_of_weight_log_weights, starting_entropy;
    double *sums_of_weights, *sums_of_weight_log_weights, *entropies;
    wfc_heuristic heuristic;
} wfc_model;

typedef struct wfc_model_overlapping {
    wfc_model parent;
    uint8_t **patterns;
    int patterns_len, patterns_cap;
    uint32_t *colors;
    int colors_num, colors_cap;
} wfc_model_overlapping;

void wfc_model_init(
    wfc_model *model, 
    int width, 
    int height, 
    int N, 
    bool periodic,
    wfc_heuristic heuristic
);
void wfc_model_shutdown(wfc_model *model);
bool wfc_model_run(wfc_model *model, uint32_t seed, int limit);

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
);
void wfc_model_overlapping_save(wfc_model_overlapping *model, Image *img);
void wfc_model_overlapping_shutdown(wfc_model_overlapping *model);
