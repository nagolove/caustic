// vim: set colorcolumn=85
// vim: fdm=marker
#include "t80_stage_$PREFIX$.h"

// includes {{{
#include "cimgui.h"
#include "koh_common.h"
#include "koh_hotkey.h"
#include "koh_stages.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
// }}}

#define MAX_SHAPES_UNDER_POINT  100

typedef struct Stage_$PREFIX$ {
    Stage                   parent;
    Camera2D                cam;
} Stage_$PREFIX$;

static void stage_$PREFIX$_init(struct Stage_$PREFIX$ *st) {
    trace("stage_$PREFIX$_init:\n");
    st->cam.zoom = 1.;
}

static void stage_$PREFIX$_update(struct Stage_$PREFIX$ *st) {
    koh_camera_process_mouse_drag(&(struct CameraProcessDrag) {
            .mouse_btn = MOUSE_BUTTON_RIGHT,
            .cam = &st->cam
    });
    koh_camera_process_mouse_scale_wheel(&(struct CameraProcessScale) {
        //.mouse_btn = MOUSE_BUTTON_LEFT,
        .cam = &st->cam,
        .dscale_value = 0.05,
        .modifier_key_down = KEY_LEFT_SHIFT,
    });

}

static void stage_$PREFIX$_gui(struct Stage_$PREFIX$ *st) {
    assert(st);
}

static void stage_$PREFIX$_enter(struct Stage_$PREFIX$ *st) {
    trace("stage_$PREFIX$_enter:\n");
}

static void stage_$PREFIX$_leave(struct Stage_$PREFIX$ *st) {
    trace("stage_$PREFIX$_leave:\n");
}

static void stage_$PREFIX$_draw(struct Stage_$PREFIX$ *st) {
}

static void stage_$PREFIX$_shutdown(struct Stage_$PREFIX$ *st) {
    trace("stage_$PREFIX$_shutdown:\n");
}

Stage *stage_$PREFIX$_new(HotkeyStorage *hk_store) {
    assert(hk_store);
    struct Stage_$PREFIX$ *st = calloc(1, sizeof(*st));
    st->parent.data = hk_store;

    st->parent.init = (Stage_callback)stage_$PREFIX$_init;
    st->parent.enter = (Stage_callback)stage_$PREFIX$_enter;
    st->parent.leave = (Stage_callback)stage_$PREFIX$_leave;

    st->parent.gui = (Stage_callback)stage_$PREFIX$_gui;
    st->parent.update = (Stage_callback)stage_$PREFIX$_update;
    st->parent.draw = (Stage_callback)stage_$PREFIX$_draw;
    st->parent.shutdown = (Stage_callback)stage_$PREFIX$_shutdown;

    return (Stage*)st;
}
