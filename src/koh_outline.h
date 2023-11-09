#pragma once

#include "raylib.h"

typedef struct koh_Outline koh_Outline;

struct koh_OutlineDef {
    float size;
    Color color;
};

koh_Outline *outline_new(struct koh_OutlineDef def);
void outline_free(koh_Outline *outline);

void outline_render_pro(
    koh_Outline *outline, Texture2D tex, 
    Rectangle src, Rectangle dst, 
    Vector2 origin, float rotation,
    Color color
);
