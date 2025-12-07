// xnn.h / xnn.c - минималистичный однофайловый MLP-движок (C99)
//
// Публичный API:
//   - typedef struct xnn xnn_t;
//   - void       xnn_init(xnn_t *net);
//   - void       xnn_shutdown(xnn_t *net);
//   - xnn_layer* xnn_add_input_layer(xnn_t *net, i32 size);
//   - xnn_layer* xnn_add_dense_layer(xnn_t *net, i32 neurons, xnn_activation act);
//   - void       xnn_randomize(xnn_t *net, f32 scale);
//   - void       xnn_forward(const xnn_t *net, const f32 *input, f32 *output);
//   - void       xnn_backward(xnn_t *net, const f32 *input, const f32 *target, f32 learning_rate);
//   - void       xnn_train_sample(xnn_t *net, const f32 *input, const f32 *target, f32 learning_rate);
//   - i32        xnn_get_input_size(const xnn_t *net);
//   - i32        xnn_get_output_size(const xnn_t *net);

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "koh_common.h"

typedef struct xnn_layer xnn_layer;

// Тип слоя
typedef enum {
    XNN_LAYER_INPUT = 0,
    XNN_LAYER_DENSE = 1
} xnn_layer_type;

// Тип активации
typedef enum {
    XNN_ACT_LINEAR  = 0,
    XNN_ACT_SIGMOID = 1,
    XNN_ACT_TANH    = 2,
    XNN_ACT_RELU    = 3
} xnn_activation;

// Описание слоя
struct xnn_layer {
    xnn_layer_type type;
    xnn_activation activation;

    i32 input_size;
    i32 output_size;

    f32 *weights; // [output_size * input_size]
    f32 *bias;    // [output_size]

    f32 *output;  // [output_size] — выход слоя (кэш для backprop)
    f32 *delta;   // [output_size] — dL/d(net) для каждого нейрона
};

// Основной объект сети
typedef struct xnn {
    xnn_layer **layers;
    i32 layer_count;
    i32 layer_capacity;

    i32 input_size;
    i32 output_size;
} xnn_t;

void xnn_init(xnn_t *net);
void xnn_shutdown(xnn_t *net);

// Входной слой: просто запоминаем размер входа
xnn_layer* xnn_add_input_layer(xnn_t *net, i32 size);
// Плотный слой (fully-connected)
xnn_layer* xnn_add_dense_layer(xnn_t *net, i32 neurons, xnn_activation act);
i32 xnn_get_input_size(const xnn_t *net);
i32 xnn_get_output_size(const xnn_t *net);
void xnn_randomize(xnn_t *net, f32 scale);

// Прямой прогон
void xnn_forward(const xnn_t *net, const f32 *input, f32 *output);
// Обратное распространение + обновление весов (один сэмпл, MSE)
void xnn_backward(xnn_t *net, const f32 *input, const f32 *target, f32 learning_rate);

// Удобный wrapper: один шаг обучения = forward + backward
void xnn_train_sample(xnn_t *net, const f32 *input, const f32 *target, f32 learning_rate);
