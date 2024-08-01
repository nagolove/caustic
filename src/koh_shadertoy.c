// vim: set colorcolumn=85
// vim: fdm=marker
#include "koh_shadertoy.h"

/*#include <stdlib.h>*/
#include "koh_logger.h"
#include <assert.h>

void shadertoy_init(ShadertoyCtx *ctx, Shader shader) {
    assert(ctx);
    ctx->shader = shader;
    /*int resUniformLoc = GetShaderLocation(golRenderShader, "resolution");*/

    ctx->loc_iTime = GetShaderLocation(shader, "iTime");
    ctx->loc_iTimeDelta = GetShaderLocation(shader, "iTimeDelta");
    ctx->loc_iFrame = GetShaderLocation(shader, "iFrame");
    ctx->loc_iMouse = GetShaderLocation(shader, "iMouse");
    ctx->loc_iResolution = GetShaderLocation(shader, "iResolution");

    trace(
        "shadertoy_init: get locations: "
        "iTime %d, "
        "iTimeDelta %d, "
        "iFrame %d, "
        "iMouse %d, "
        "iResolution %d\n",
        ctx->loc_iTime,
        ctx->loc_iTimeDelta,
        ctx->loc_iFrame,
        ctx->loc_iMouse,
        ctx->loc_iResolution
    );

}

void shadertoy_pass(ShadertoyCtx *ctx, int rt_width, int rt_height) {
    assert(ctx);
    assert(rt_width > 0);
    assert(rt_height > 0);

    if (!ctx->shader.id)
        return;

    /*
    // {{{
vec3 iMouse
vec2 iResolution

uniform vec3 iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform float iFrame;
uniform float iChannelTime[4];
uniform vec4 iMouse;
uniform vec4 iDate;
uniform float iSampleRate;
uniform vec3 iChannelResolution[4];
uniform samplerXX iChanneli;

uniform float iTime: Время, прошедшее с начала запуска шейдера, в секундах.
uniform float iTimeDelta: Время, прошедшее между последними двумя кадрами, в секундах.
uniform int iFrame: Номер текущего кадра.
uniform vec4 iMouse: Положение курсора мыши. Первый и второй компоненты содержат текущие координаты курсора, а третий и четвертый - координаты при последнем нажатии мыши.
uniform vec2 iResolution: Разрешение текущего рендер-таргета в пикселях.
uniform float iChannelTime[4]: Время, прошедшее с момента запуска каждого из четырех возможных каналов.
uniform vec3 iChannelResolution[4]: Разрешение каждого из четырех возможных входных каналов.
uniform samplerXX iChannel0..3: Текстурные сэмплеры для четырех возможных входных каналов. Тип сэмплера (sampler2D, samplerCube, и т.д.) зависит от типа назначенного входного ресурса.
uniform vec4 iDate: Текущая дата и время в формате (год, месяц, день, секунды).
uniform float iSampleRate: Частота сэмплирования для аудиоданных.
// }}}
     */

    float time = GetTime();
    SetShaderValue(ctx->shader, ctx->loc_iTime, &time, SHADER_UNIFORM_FLOAT);

    float resolution[3] = { rt_width, rt_height, 1. };
    SetShaderValue(
        ctx->shader, ctx->loc_iResolution, resolution, SHADER_UNIFORM_VEC3
    );

    float time_delta = GetFrameTime();
    SetShaderValue(
        ctx->shader, ctx->loc_iTimeDelta, &time_delta, SHADER_UNIFORM_FLOAT
    );
}
