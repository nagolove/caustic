#pragma once

#include "raylib.h"
#include "koh_reasings.h"       

typedef struct CameraProcessor {
    Camera2D    *cam;
    KeyboardKey mod_key_down_scale; 
    int         mouse_btn_move;
    float       dscale_value;

    // Easing
    EaseFunc    zoom_ease;
    EaseFunc    move_ease;
    float       zoom_time_total;
    float       move_time_total;

    // Internal easing state
    float       zoom_elapsed;
    float       move_elapsed;

    float       zoom_start;
    float       zoom_target;

    Vector2     offset_start;
    Vector2     offset_target;
} CameraProcessor;

typedef struct CameraProcessorOpts {
    Camera2D    *cam;
    KeyboardKey mod_key_down_scale; 
    int         mouse_btn_move;
} CameraProcessorOpts;

void camp_init(CameraProcessor *cp, CameraProcessorOpts opts);
void camp_shutdown(CameraProcessor *cp);
void camp_update(CameraProcessor *cp);
void camp_reset(CameraProcessor *cp);
