#pragma once

#include "raylib.h"

typedef struct PartsEngine PartsEngine;

struct Parts_ExplositionDef {
    double  lifetime, vel_min, vel_max;
    float   spread_time; // seconds
    Color   color;
    Vector2 pos;
    int     num;
};

PartsEngine *parts_new();
void parts_free(PartsEngine *pe);
void parts_update(PartsEngine *pe);
void parts_draw(PartsEngine *pe);
void parts_explode(PartsEngine *pe, struct Parts_ExplositionDef *def);
