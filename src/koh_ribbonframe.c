#include "koh_ribbonframe.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "koh.h"
#include "koh_common.h"
#include "koh_routine.h"
#include "raylib.h"
#include "raymath.h"

enum State {
    S_NONE,
    S_RESIZE,
    S_HANDLE_RESIZE,
    S_HANDLE_CAN_RESIZE,
};

/*
static const char *state2str(enum State s) {
    switch (s) {
        case S_NONE: return "S_NONE";
        case S_RESIZE: return "S_RESIZE";
        case S_HANDLE_RESIZE: return "S_HANDLE_RESIZE";
        case S_HANDLE_CAN_RESIZE: return "S_HANDLE_CAN_RESIZE";
    }
    return NULL;
}
// */

struct RibbonFrameInternal {
    int         mouse_button_bind, 
                corner_index,
                points_num,
                snap_size;
    bool        snap;
    enum State  state;
    float       line_thick;
    Color       line_color,
                handle_color;
    float       handle_circle_radius;
    Vector2     corner_point, last_points[4];
};

void ribbonframe_init(
    struct RibbonFrame *rf, const struct RibbonFrameOpts *opts
) {
    assert(rf);
    trace("ribbonframe_init:\n");

    memset(rf, 0, sizeof(*rf));
    rf->internal = malloc(sizeof(*rf->internal));
    assert(rf->internal);

    struct RibbonFrameInternal *internal = rf->internal;

    internal->handle_circle_radius = 30.;
    internal->state = S_NONE;
    internal->line_thick = 3.;
    internal->line_color = YELLOW;
    internal->handle_color = BLUE;
    internal->corner_index = -1;
    internal->points_num = sizeof(internal->last_points) / 
                           sizeof(internal->last_points[0]);
    internal->snap_size = 1;
    internal->mouse_button_bind = MOUSE_BUTTON_LEFT;
    internal->snap = false;
    ribbonframe_update_opts(rf, opts);
}

void ribbonframe_shutdown(struct RibbonFrame *rf) {
    assert(rf);
    if (rf->internal) {
        free(rf->internal);
        rf->internal = NULL;
    }
}

static Vector2 mouse_with_cam(const Camera2D *cam) {
    Vector2 cam_offset = cam ? cam->offset : Vector2Zero();
    float scale = cam ? cam->zoom : 1.;
    Vector2 mp = Vector2Subtract(GetMousePosition(), cam_offset);
    return Vector2Scale(mp, 1. / scale);
}

static void selection_start(struct RibbonFrame *rf, const Camera2D *cam) {
    assert(rf);
    assert(rf->internal);

    /*trace("selection_start:\n");*/

    struct RibbonFrameInternal *internal = rf->internal;
    Vector2 mp = mouse_with_cam(cam);

    if (internal->state == S_NONE) {
        rf->rect.x = mp.x;
        rf->rect.y = mp.y;
    }

    internal->state = S_RESIZE;

    rf->rect.width = mp.x - rf->rect.x;
    rf->rect.height = mp.y - rf->rect.y;

    if (rf->rect.width < 0.) {
        rf->rect.width = -rf->rect.width;
        rf->rect.x -= rf->rect.width;
    }
    if (rf->rect.height < 0.) {
        rf->rect.height = -rf->rect.height;
        rf->rect.y -= rf->rect.height;
    }
    rf->exist = true;
}

static void last_points_copy(
    struct RibbonFrameInternal *internal, const Vector2 *points, int points_num
) {
    assert(internal);
    assert(points);
    assert(points_num >= 0);

    const int last_points_num = sizeof(internal->last_points) /
                                sizeof(internal->last_points[0]);
    assert(points_num == last_points_num);
    for (int u = 0; u < points_num; u++)
        internal->last_points[u] = points[u];
}

static void handles_check(
    struct RibbonFrameInternal *internal, const Vector2 *points, 
    int points_num, Vector2 mp
) {
    assert(internal);
    float radius = internal->handle_circle_radius;
    //internal->corner_index = -1;
    for (int i = 0; i < internal->points_num; ++i) {
        if (CheckCollisionPointCircle(mp, points[i], radius)) {
            internal->corner_index = i;
            internal->corner_point = points[i];
            break;
        }
    }
}

//__attribute__((unused))
static void handle_resize(
    struct RibbonFrame *rf, Vector2 handle_diff
) {
    assert(rf);
    assert(rf->internal);
    //assert(rf->internal->corner_index != -1);
    //trace("handle_resize: corner_index %d\n", rf->internal->corner_index);
    switch (rf->internal->corner_index) {
        case 0:
            rf->rect.x += handle_diff.x;
            rf->rect.y += handle_diff.y;
            rf->rect.width -= handle_diff.x;
            rf->rect.height -= handle_diff.y;

            if (rf->internal->snap) {
                int snap_size = rf->internal->snap_size;
                int dx = (int)rf->rect.x % snap_size;
                int dy = (int)rf->rect.y % snap_size;
                rf->rect.x = rf->rect.x - dx;
                rf->rect.y = rf->rect.y - dy;
                rf->rect.width = rf->rect.width + dx;
                rf->rect.height = rf->rect.height + dy;
            }
            break;
        case 1:
            //rf->rect.y -= handle_diff.y;
            //rf->rect.width += handle_diff.x;
            //rf->rect.height -= handle_diff.y;
            break;
        case 2:
            rf->rect.width += handle_diff.x;
            rf->rect.height += handle_diff.y;

            if (rf->internal->snap) {
                int snap_size = rf->internal->snap_size;
                int dx = (int)rf->rect.width % snap_size;
                int dy = (int)rf->rect.height % snap_size;
                rf->rect.width = rf->rect.width - dx;
                rf->rect.height = rf->rect.height - dy;
            }
            break;
        case 3:
            //rf->rect.x -= handle_diff.x;
            //rf->rect.width -= handle_diff.x;
            //rf->rect.height += handle_diff.y;
            break;
    }

    if (rf->rect.width < 0.) rf->rect.width = 0.;
    if (rf->rect.height < 0.) rf->rect.height = 0.;
}

void ribbonframe_update(struct RibbonFrame *rf, const Camera2D *cam) {
    assert(rf);
    struct RibbonFrameInternal *internal = rf->internal;
    Vector2 mp = mouse_with_cam(cam);

    // Обход по часовой стрелке
    const Vector2 points[] = {
        { rf->rect.x, rf->rect.y }, 
        { rf->rect.x + rf->rect.width, rf->rect.y }, 
        { rf->rect.x + rf->rect.width, rf->rect.y + rf->rect.height },
        { rf->rect.x, rf->rect.y + rf->rect.height }, 
    };
    const int points_num = sizeof(points) / sizeof(points[0]);

    last_points_copy(internal, points, points_num);
    internal->corner_index = -1;

    // Проверка радиуса попадания в ручку управления
    handles_check(internal, points, points_num, mp);
    
    if (IsMouseButtonDown(internal->mouse_button_bind)) {

        if (internal->state == S_NONE)
            if (internal->corner_index != -1) {
                internal->state = S_HANDLE_CAN_RESIZE;
                SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
            }

        switch(internal->state) {
            case S_NONE: 
                selection_start(rf, cam);
                break;
            case S_RESIZE:
                selection_start(rf, cam);
                break;
            case S_HANDLE_RESIZE:
                break;
            case S_HANDLE_CAN_RESIZE: {
                Vector2 handle_diff = Vector2Subtract(
                    mp, internal->last_points[internal->corner_index]
                );
                handle_resize(rf, handle_diff);
                break;
            }
        };

    } else {
        internal->state = S_NONE;
        //rf->exist = false;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

}

void ribbonframe_draw(
    struct RibbonFrame *rf, struct RibbonFrameDrawOpts *opts
) {
    //trace("ribbonframe_draw:\n");
    assert(rf);
    assert(rf->internal);
    struct RibbonFrameInternal *internal = rf->internal;

    DrawRectangleLinesEx(
        rf->rect, rf->internal->line_thick, rf->internal->line_color
    );

    if (internal->corner_index == 0 || internal->corner_index == 2) {
        DrawCircleV(
            rf->internal->corner_point,
            rf->internal->handle_circle_radius,
            rf->internal->handle_color
        );
    }

}

void ribbonframe_update_opts(
    struct RibbonFrame *rf, const struct RibbonFrameOpts *new_opts
) {
    assert(rf);

    if (!new_opts)
        return;

    struct RibbonFrameInternal *internal = rf->internal;
    assert(internal);

    if (new_opts->mouse_button_bind != -1)
        internal->mouse_button_bind = new_opts->mouse_button_bind;
    internal->snap_size = new_opts->snap_size;
    internal->snap = new_opts->snap;

    /*if (!color_eq(new_opts->line_color, internal->line_color))*/
        internal->line_color = new_opts->line_color;
    /*if (!color_eq(new_opts->handle_color, internal->handle_color))*/
        internal->handle_color = new_opts->handle_color;
    /*if (new_opts->line_thick != internal->line_thick)*/
        internal->line_thick = new_opts->line_thick;
}
