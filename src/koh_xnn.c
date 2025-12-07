#include "koh_xnn.h"

// ---------------- Внутренние функции (static) ----------------

static f32 xnn__act(xnn_activation act, f32 x) {
    switch (act) {
        case XNN_ACT_LINEAR:  return x;
        case XNN_ACT_SIGMOID: return 1.0f / (1.0f + expf(-x));
        case XNN_ACT_TANH:    return tanhf(x);
        case XNN_ACT_RELU:    return x > 0.0f ? x : 0.0f;
        default:              return x;
    }
}

// Производная активации по выходу y = f(x)
static f32 xnn__act_deriv_from_output(xnn_activation act, f32 y) {
    switch (act) {
        case XNN_ACT_LINEAR:  return 1.0f;
        case XNN_ACT_SIGMOID: return y * (1.0f - y);
        case XNN_ACT_TANH:    return 1.0f - y * y;
        case XNN_ACT_RELU:    return y > 0.0f ? 1.0f : 0.0f;
        default:              return 1.0f;
    }
}

static xnn_layer* xnn__layer_create(void) {
    xnn_layer *l = (xnn_layer*)calloc(1, sizeof(xnn_layer));
    return l;
}

static void xnn__layer_free(xnn_layer *l) {
    if (!l) return;
    free(l->weights);
    free(l->bias);
    free(l->output);
    free(l->delta);
    free(l);
}

// ---------------- Публичный API ----------------

// Обнулить структуру движка
void xnn_init(xnn_t *net) {
    if (!net) return;
    memset(net, 0, sizeof(*net));
}

// Освободить всё, что внутри
void xnn_shutdown(xnn_t *net) {
    if (!net) return;
    if (net->layers) {
        for (i32 i = 0; i < net->layer_count; ++i) {
            xnn__layer_free(net->layers[i]);
        }
        free(net->layers);
    }
    memset(net, 0, sizeof(*net));
}

// Внутреннее добавление слоя в массив
static xnn_layer* xnn__push_layer(xnn_t *net) {
    if (net->layer_count == net->layer_capacity) {
        i32 new_cap = net->layer_capacity == 0 ? 4 : net->layer_capacity * 2;
        xnn_layer **new_layers = (xnn_layer**)realloc(
            net->layers,
            (size_t)new_cap * sizeof(xnn_layer*)
        );
        if (!new_layers) {
            return NULL;
        }
        net->layers = new_layers;
        net->layer_capacity = new_cap;
    }
    xnn_layer *l = xnn__layer_create();
    if (!l) return NULL;
    net->layers[net->layer_count++] = l;
    return l;
}

// Входной слой: просто запоминаем размер входа
xnn_layer* xnn_add_input_layer(xnn_t *net, i32 size) {
    if (!net || size <= 0) return NULL;
    xnn_layer *l = xnn__push_layer(net);
    if (!l) return NULL;
    l->type = XNN_LAYER_INPUT;
    l->input_size = size;
    l->output_size = size;
    net->input_size = size;
    return l;
}

// Плотный слой (fully-connected)
xnn_layer* xnn_add_dense_layer(xnn_t *net, i32 neurons, xnn_activation act) {
    if (!net || neurons <= 0) return NULL;
    if (net->layer_count == 0) {
        // должен быть хотя бы входной слой
        return NULL;
    }
    xnn_layer *prev = net->layers[net->layer_count - 1];
    i32 input_size = prev->output_size;
    if (input_size <= 0) return NULL;

    xnn_layer *l = xnn__push_layer(net);
    if (!l) return NULL;

    l->type = XNN_LAYER_DENSE;
    l->activation = act;
    l->input_size = input_size;
    l->output_size = neurons;

    size_t w_count = (size_t)input_size * (size_t)neurons;
    l->weights = (f32*)calloc(w_count, sizeof(f32));
    l->bias    = (f32*)calloc((size_t)neurons, sizeof(f32));
    l->output  = (f32*)calloc((size_t)neurons, sizeof(f32));
    l->delta   = (f32*)calloc((size_t)neurons, sizeof(f32));

    if (!l->weights || !l->bias || !l->output || !l->delta) {
        xnn__layer_free(l);
        net->layer_count--;
        return NULL;
    }

    net->output_size = neurons;
    return l;
}

i32 xnn_get_input_size(const xnn_t *net)  { return net ? net->input_size  : 0; }
i32 xnn_get_output_size(const xnn_t *net) { return net ? net->output_size : 0; }

// Случайная инициализация весов в диапазоне [-scale; scale]
static f32 xnn__randf_signed(void) {
    return (f32)rand() / (f32)RAND_MAX * 2.0f - 1.0f;
}

void xnn_randomize(xnn_t *net, f32 scale) {
    if (!net) return;
    if (scale <= 0.0f) scale = 1.0f;
    for (i32 li = 0; li < net->layer_count; ++li) {
        xnn_layer *l = net->layers[li];
        if (!l || l->type != XNN_LAYER_DENSE) continue;
        size_t w_count = (size_t)l->input_size * (size_t)l->output_size;
        for (size_t i = 0; i < w_count; ++i) {
            l->weights[i] = xnn__randf_signed() * scale;
        }
        for (i32 j = 0; j < l->output_size; ++j) {
            l->bias[j] = 0.0f;
        }
    }
}

// Прямой прогон
void xnn_forward(const xnn_t *net, const f32 *input, f32 *output) {
    if (!net || !input || net->layer_count == 0) return;
    const f32 *cur_in = input;

    for (i32 li = 0; li < net->layer_count; ++li) {
        xnn_layer *l = net->layers[li];
        if (!l) continue;

        if (l->type == XNN_LAYER_INPUT) {
            cur_in = input; // просто указываем на вход
            continue;
        } else if (l->type == XNN_LAYER_DENSE) {
            for (i32 j = 0; j < l->output_size; ++j) {
                f32 sum = l->bias[j];
                f32 *w_row = l->weights + (size_t)j * (size_t)l->input_size;
                for (i32 i = 0; i < l->input_size; ++i) {
                    sum += w_row[i] * cur_in[i];
                }
                l->output[j] = xnn__act(l->activation, sum);
            }
            cur_in = l->output;
        }
    }

    if (output && net->output_size > 0) {
        memcpy(output, cur_in, (size_t)net->output_size * sizeof(f32));
    }
}

// Обратное распространение + обновление весов (один сэмпл, MSE)
void xnn_backward(xnn_t *net, const f32 *input, const f32 *target, f32 learning_rate) {
    if (!net || !input || !target || net->layer_count == 0) return;
    if (learning_rate <= 0.0f) return;

    // 1) считаем дельты (от последнего плотного слоя к первому)
    xnn_layer *next_dense = NULL;
    for (i32 li = net->layer_count - 1; li >= 0; --li) {
        xnn_layer *l = net->layers[li];
        if (!l || l->type != XNN_LAYER_DENSE) continue;

        if (!next_dense) {
            // выходной слой
            for (i32 j = 0; j < l->output_size; ++j) {
                f32 y = l->output[j];
                f32 err = y - target[j]; // dL/dy для MSE
                f32 dact = xnn__act_deriv_from_output(l->activation, y);
                l->delta[j] = err * dact;
            }
        } else {
            // скрытый слой: delta_j = f'(y_j) * sum_k w_next[k,j] * delta_next[k]
            for (i32 j = 0; j < l->output_size; ++j) {
                f32 sum = 0.0f;
                for (i32 k = 0; k < next_dense->output_size; ++k) {
                    f32 *w_row_next = next_dense->weights
                                      + (size_t)k * (size_t)next_dense->input_size;
                    sum += w_row_next[j] * next_dense->delta[k];
                }
                f32 y = l->output[j];
                f32 dact = xnn__act_deriv_from_output(l->activation, y);
                l->delta[j] = sum * dact;
            }
        }

        next_dense = l;
    }

    // 2) обновляем веса и смещения, используя кэшированные выходы как входы
    const f32 *last_output = NULL;
    for (i32 li = 0; li < net->layer_count; ++li) {
        xnn_layer *l = net->layers[li];
        if (!l) continue;

        if (l->type == XNN_LAYER_INPUT) {
            last_output = input;
            continue;
        }
        if (l->type != XNN_LAYER_DENSE) continue;

        const f32 *in = last_output;
        if (!in) continue; // не должно случаться при нормальной топологии

        for (i32 j = 0; j < l->output_size; ++j) {
            f32 *w_row = l->weights + (size_t)j * (size_t)l->input_size;
            f32 delta = l->delta[j];
            for (i32 i = 0; i < l->input_size; ++i) {
                f32 grad_w = delta * in[i];
                w_row[i] -= learning_rate * grad_w;
            }
            l->bias[j] -= learning_rate * delta;
        }

        last_output = l->output;
    }
}

// Удобный wrapper: один шаг обучения = forward + backward
void xnn_train_sample(xnn_t *net, const f32 *input, const f32 *target, f32 learning_rate) {
    if (!net) return;
    xnn_forward(net, input, NULL);
    xnn_backward(net, input, target, learning_rate);
}

#ifdef XNN_DEMO
// ---------------- Пример использования: обучение XOR ----------------
#include <stdio.h>
#include <time.h>

int main(void) {
    srand((unsigned)time(NULL));

    xnn_t net;
    xnn_init(&net);

    // 2 входа -> скрытый слой 4 нейрона -> 1 выход
    xnn_add_input_layer(&net, 2);
    xnn_add_dense_layer(&net, 4, XNN_ACT_TANH);
    xnn_add_dense_layer(&net, 1, XNN_ACT_SIGMOID);

    xnn_randomize(&net, 1.0f);

    f32 data[4][2] = {
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f}
    };
    f32 target_xor[4][1] = {
        {0.0f},
        {1.0f},
        {1.0f},
        {0.0f}
    };

    f32 lr = 0.1f;
    for (int epoch = 0; epoch < 10000; ++epoch) {
        f32 loss = 0.0f;
        for (int n = 0; n < 4; ++n) {
            xnn_train_sample(&net, data[n], target_xor[n], lr);
            f32 out[1];
            xnn_forward(&net, data[n], out);
            f32 err = out[0] - target_xor[n][0];
            loss += 0.5f * err * err;
        }
        loss /= 4.0f;
        if (epoch % 1000 == 0) {
            printf("epoch %d, loss=%f\n", epoch, loss);
        }
    }

    printf("XOR results:\n");
    for (int n = 0; n < 4; ++n) {
        f32 out[1];
        xnn_forward(&net, data[n], out);
        printf("%g xor %g -> %g\n", data[n][0], data[n][1], out[0]);
    }

    xnn_shutdown(&net);
    return 0;
}
#endif // XNN_DEMO
