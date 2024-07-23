#include "koh_cammode.h"

#include "koh_logger.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct {
    int mode; char *str;
} koh_cam_mode2str[] = {
    { KOH_CM_FREE,  "FREE" },
    { KOH_CM_FIXED, "FIXED" },
    { -1, NULL }
};

static Camera2D cam = {0};

Camera2D *koh_cam_get() {
    return &cam;
}

const char *koh_cam_get_mode(koh_CamMode cm) {
    const char *r = NULL;
    int i = 0;
    while (koh_cam_mode2str[i].str) {
        if (koh_cam_mode2str[i].mode == cm) {
            return koh_cam_mode2str[i].str;
        }
        i++;
    }
    return r;
}

void koh_cam_init() {
    trace("koh_cam_init:\n");
    memset(&cam, 0, sizeof(cam));
    cam.zoom = 1.;
}

void koh_cam_mode_next(koh_CamMode *camera_mode) {
    if (!camera_mode)
        return;

    if (*camera_mode + 1 >= KOH_CM_LAST_UNUSED)
        *camera_mode = KOH_CM_FIRST_UNUSED + 1;
    else
        *camera_mode += 1;
}

void koh_cam_shutdown() {
    trace("koh_cam_shutdown:\n");
}

void koh_cam_reset(Camera2D *cam) {
    assert(cam);
    memset(cam, 0, sizeof(*cam));
    cam->zoom = 1.;
}
