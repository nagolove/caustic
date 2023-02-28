// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "rand.h"
#include "raylib.h"
#include "stages.h"

typedef struct Stage_OuterRandom {
    Stage            parent;
    RenderTexture    rt;
    xorshift32_state rng;
    Font             fnt;
} Stage_OuterRandom;

Stage *stage_orandom_new(void);
void stage_orandom_init(Stage_OuterRandom *st, void *data);
void stage_orandom_update(Stage_OuterRandom *st);
void stage_orandom_shutdown(Stage_OuterRandom *st);



