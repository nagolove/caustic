#pragma once

#include "raylib.h"
#include "koh_reasings.h"       
#include "koh_tmr.h"

enum { CAMP_INERTIA_FACTOR = 5, };
#define CAMP_GLIDE_DURATION   0.3f

typedef struct CameraProcessor {
    Camera2D    *cam;
    KeyboardKey mod_key_down_scale; 
    int         mouse_btn_move;
    float       dscale_value;

    // Zoom easing
    EaseFunc    zoom_ease;
    float       zoom_time_total;
    float       zoom_elapsed;
    float       zoom_start;
    float       zoom_target;

    // Move drag + glide
    EaseFunc    move_ease;
    bool        is_dragging;
    Vector2     drag_velocity;
    Tmr         glide_tmr;
    Vector2     move_start;
    Vector2     move_target;

    // Easing для zoom при сдвиге offset
    float       move_elapsed;
    float       move_time_total;
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
