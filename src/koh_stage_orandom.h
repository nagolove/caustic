// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "koh_rand.h"
#include "koh_stages.h"
#include "raylib.h"

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



