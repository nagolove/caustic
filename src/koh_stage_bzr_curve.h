// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "hotkey.h"
#include "raylib.h"
#include "stages.h"

typedef struct Stage_BzrCurve {
    Stage         parent;
    HotkeyStorage *hk_store;
    Vector2       guides[4];
    Vector2       *points;
    int           pointsnum;
    int           drag_index;
    Font          fnt;
} Stage_BzrCurve;

Stage *stage_bzr_curve_new(HotkeyStorage *hk_store);



