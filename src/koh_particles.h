#pragma once

#include "raylib.h"

typedef struct PartsEngine PartsEngine;

struct Parts_ExplositionDef {
    double  lifetime, vel_min, vel_max;
    // Если == 0, то частицы частицы создаются единомементно
    float   spread_time; // Время создания в секундах. 
    int     num_in_sec; // Количество частиц создаваемое в секунду.
    Color   color;
    Vector2 pos;
    float   min_radius, max_radius;
};

/*
spread_time 0.1     - сколько раз вызовется parts_update()?


num_in_sec 1000
(вычистить количество секунд, прошедших с прошлого вызова эмитера) * num_in_sec
 */

PartsEngine *parts_new();
void parts_free(PartsEngine *pe);
void parts_update(PartsEngine *pe);
void parts_draw(PartsEngine *pe);
void parts_explode(PartsEngine *pe, struct Parts_ExplositionDef *def);
