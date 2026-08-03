// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "koh.h"
/*#include "t80.h"*/

typedef struct Stage_SpriteLoader2Opts {
    const char *regex_pattern_images;
} Stage_SpriteLoader2Opts;

Stage *stage_sprite_loader_new2(Stage_SpriteLoader2Opts opts);
