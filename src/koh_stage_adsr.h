// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "raylib.h"

#include "koh_adsr.h"
#include "koh_hotkey.h"
#include "koh_stages.h"

#define MAX_ENVELOPES   4
#define GUIDES_NUM      3

struct Column {
    double value;
    Color  color;
};

struct ADSR_view {
    Vector2         pos; // точка рисования
    struct Column   *colums;
    int             columns_num, columns_cap;
    //Rectangle       rect;
    int             height;
};

struct ADSR_guides {
    Vector2       circles[GUIDES_NUM];
};

struct DragIndex {
    int guide, circle;
};

typedef struct Stage_ADSR {
    Stage               parent;
    //HotkeyStorage       *hk_store;

    struct ADSR          envelopes[MAX_ENVELOPES];
    struct ADSR_view     views[MAX_ENVELOPES];
    struct ADSR_guides   guides[MAX_ENVELOPES];
    struct DragIndex     drag_index;
    int                  envelopes_num;
} Stage_ADSR;

Stage *stage_adsr_new();
void stage_adsr_init(Stage_ADSR *st);
void stage_adsr_update(Stage_ADSR *st);
void stage_adsr_shutdown(Stage_ADSR *st);



