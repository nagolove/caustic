#pragma once

#include "raylib.h"

typedef enum koh_CamMode {
    KOH_CM_FIRST_UNUSED = 0,
    KOH_CM_FREE         = 1,
    KOH_CM_FIXED        = 2,
    KOH_CM_LAST_UNUSED  = 3,
} koh_CamMode;

/*const char *get_cammode(CamMode cm);*/
/*void cammode_next(CamMode *camera_mode);*/

Camera2D *koh_cam_get();
const char *koh_cam_get_mode(enum koh_CamMode cm);
void koh_cam_init();
void koh_cam_mode_next(enum koh_CamMode *camera_mode);
void koh_cam_shutdown();
void koh_cam_reset();
