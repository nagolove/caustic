#pragma once

#include "raylib.h"

typedef enum CamMode {
    CM_FIRST_UNUSED = 0,
    CM_FREE         = 1,
    CM_FIXED        = 2,
    CM_LAST_UNUSED  = 3,
} CamMode;

/*const char *get_cammode(CamMode cm);*/
/*void cammode_next(CamMode *camera_mode);*/

Camera2D *cam_get();
const char *cam_get_mode(CamMode cm);
void cam_init();
void cam_mode_next(CamMode *camera_mode);
void cam_shutdown();
void camera_reset();
