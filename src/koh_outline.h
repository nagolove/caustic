#pragma once

#include "raylib.h"

typedef struct Outline Outline;

struct Outline_def {
    float size;
    Color color;
};

Outline *outline_new(struct Outline_def def);
void outline_free(Outline *outline);

void outline_render_pro(
    Outline *outline, Texture2D tex, 
    Rectangle src, Rectangle dst, 
    Vector2 origin, float rotation,
    Color color
);
