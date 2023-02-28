// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "raylib.h"
#include "stages.h"

#include "iface.h"

typedef struct Stage_iface {
    Stage parent;
    VContainer *mnu1, *mnu2;
    //bool is_mnu_open; //, is_first_iter;
    Vector2 mousep;
    Font fnt;
} Stage_iface;

Stage *stage_iface_new(void);



