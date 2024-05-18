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


