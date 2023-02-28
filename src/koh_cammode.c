#include "koh_cammode.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct {
    int mode; char *str;
} cam_mode2str[] = {
    { CM_FREE,  "FREE" },
    { CM_FIXED, "FIXED" },
    { -1, NULL }
};

static Camera2D cam = {0};

Camera2D *cam_get() {
    return &cam;
}

const char *cam_get_mode(CamMode cm) {
    const char *r = NULL;
    int i = 0;
    while (cam_mode2str[i].str) {
        if (cam_mode2str[i].mode == cm) {
            return cam_mode2str[i].str;
        }
        i++;
    }
    return r;
}

void cam_init() {
    printf("cam_init\n");
    memset(&cam, 0, sizeof(cam));
    cam.zoom = 1.;
}

void cam_mode_next(CamMode *camera_mode) {
    if (!camera_mode)
        return;

    if (*camera_mode + 1 >= CM_LAST_UNUSED)
        *camera_mode = CM_FIRST_UNUSED + 1;
    else
        *camera_mode += 1;
}

void cam_shutdown() {
    printf("cam_shutdown\n");
}

void camera_reset() {
    memset(&cam, 0, sizeof(cam));
    cam.zoom = 1.;
}


