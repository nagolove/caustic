// vim: fdm=marker

#pragma once

#include "raylib.h"

typedef struct Envelope *Envelope_t;

typedef struct EnvelopeOpts {
    const char *name;
    int        tex_w, tex_h;
    int        line_thick;
    int        handle_size;
    /*int        tex_text_w, tex_text_h;*/
    int        default_points_cap;
    int        label_font_size;
    Color      background_color;
} EnvelopeOpts;

// XXX: Что делает функция?
EnvelopeOpts env_partial_opts(EnvelopeOpts opts);

Envelope_t env_new(EnvelopeOpts opts);
void env_free(Envelope_t e);

__attribute__((deprecated))
void env_draw_imgui_opts(Envelope_t e);
__attribute__((deprecated))
void env_draw_imgui_env(Envelope_t e);

void env_draw_opts_gui(Envelope_t e);
void env_draw_gui(Envelope_t e);

// 0. <= amount <= 1.
// Если amount не входит в диапазон границ, то устанавливается граничное 
// значение.
// Возвращает интерполированное значение высоты кривой
float env_eval(Envelope_t e, float amount);

void env_point_add(Envelope_t e, Vector2 pos);
void env_point_remove(Envelope_t e, int index);

/*
 Сохранить координаты точек в Луа строку.
[[
 { 
    -- XXX: какие значения сюда добавлять?
    optioins_value = {}, 
    points = { 
        { 10, 0 }, { 0, 0 } }
    }
]]
*/
char *env_export_alloc(Envelope_t e);
// Загрузить координаты точек из Луа строки.
void env_import(Envelope_t e, const char *lua_str);
