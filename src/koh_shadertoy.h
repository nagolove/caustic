// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "raylib.h"

typedef struct ShadertoyCtx {
    // {{{
    //uniform float iTime: Время, прошедшее с начала запуска шейдера, в секундах.
    //uniform float iTimeDelta: Время, прошедшее между последними двумя кадрами, в секундах.
    //uniform int iFrame: Номер текущего кадра.
    //uniform vec4 iMouse: Положение курсора мыши. Первый и второй компоненты содержат текущие координаты курсора, а третий и четвертый - координаты при последнем нажатии мыши.
    //uniform vec2 iResolution: Разрешение текущего рендер-таргета в пикселях.
    // }}}
    int    loc_iTime, loc_iTimeDelta, loc_iFrame, loc_iMouse, loc_iResolution;
    Shader shader;
} ShadertoyCtx;

void shadertoy_init(ShadertoyCtx *ctx, Shader shader);
void shadertoy_pass(ShadertoyCtx *ctx, int rt_width, int rt_height);
void shadertoy_pass_custom(
    ShadertoyCtx *ctx, Shader shader, int rt_width, int rt_height
);
