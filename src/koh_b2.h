// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "box2d/box2d.h"
#include "box2d/color.h"
#include "box2d/debug_draw.h"
#include "box2d/types.h"
#include "raylib.h"

inline static Color b2Color_to_Color(b2Color c) {
    // {{{
    return (Color) {
        .r = c.r * 255.,
        .g = c.g * 255.,
        .b = c.b * 255.,
        .a = c.a * 255.,
    };
    // }}}
}

b2DebugDraw b2_world_dbg_draw_create();
char *b2Vec2_tostr_alloc(b2Vec2 *verts, int num);

