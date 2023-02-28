// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "stages.h"
#include "hotkey.h"
#include "tile.h"

typedef           struct Stage_Terrain {
    Stage         parent;
    float         camera_move_k;
    Font          fnt;
    HotkeyStorage *hk_storage;
    Tiles         t;
    float         zoom_step;
    int           mapn, threads;
    Camera2D      *prev_cam;
    bool          is_run;
} Stage_Terrain;

Stage *stage_terrain_new(HotkeyStorage *hk_store);



