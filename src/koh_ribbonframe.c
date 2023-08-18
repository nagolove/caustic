#include "koh_ribbonframe.h"

#include <assert.h>
#include <stdlib.h>
#include "koh.h"
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
*/

struct RibbonFrameInternal {
    int         mouse_button_bind, 
                corner_index,
                points_num;
    enum State  state;
    float       line_thick;
    Color       line_color,
                handle_color;
    float       handle_circle_radius;
    Vector2     corner_point, last_points[4];
};

void ribbonframe_init(struct RibbonFrame *rf, const struct RibbonFrameOpts *opts) {
    assert(rf);
    trace("ribbonframe_init:\n");

    rf->internal = malloc(sizeof(*rf->internal));
    assert(rf->internal);

    struct RibbonFrameInternal *internal = rf->internal;

    if (opts)
        internal->mouse_button_bind = opts->mouse_button_bind;
    else
        internal->mouse_button_bind = MOUSE_BUTTON_LEFT;

    internal->handle_circle_radius = 15.;
    internal->state = S_NONE;
    internal->line_thick = 3.;
    internal->line_color = YELLOW;
    internal->handle_color = BLUE;
    internal->corner_index = -1;
    internal->points_num = sizeof(internal->last_points) / 
                           sizeof(internal->last_points[0]);
}

void ribbonframe_shutdown(struct RibbonFrame *rf) {
    assert(rf);
    if (rf->internal) {
        free(rf->internal);
        rf->internal = NULL;
    }
}

static void selection_start(struct RibbonFrame *rf) {
    assert(rf);
    assert(rf->internal);
    struct RibbonFrameInternal *internal = rf->internal;
    Vector2 mp = GetMousePosition();

    if (internal->state == S_NONE) {
        /*trace("ribbonframe_update: S_NONE\n");*/
        rf->rect.x = GetMouseX();
        rf->rect.y = GetMouseY();
    }

    /*trace("ribbonframe_update: S_GRIP_RESIZE\n");*/
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

static void handle_resize(
    struct RibbonFrame *rf, Vector2 handle_diff
) {
    assert(rf);
    assert(rf->internal);
    assert(rf->internal->corner_index != -1);
    trace("handle_resize: corner_index %d\n", rf->internal->corner_index);
    switch (rf->internal->corner_index) {
        case 0:
            rf->rect.x += handle_diff.x;
            rf->rect.y += handle_diff.y;
            rf->rect.width -= handle_diff.x;
            rf->rect.height -= handle_diff.y;
            break;
        case 1:
            rf->rect.y -= handle_diff.y;
            rf->rect.width += handle_diff.x;
            rf->rect.height -= handle_diff.y;
            break;
        case 2:
            rf->rect.width += handle_diff.x;
            rf->rect.height += handle_diff.y;
            break;
        case 3:
            rf->rect.x -= handle_diff.x;
            rf->rect.width -= handle_diff.x;
            rf->rect.height += handle_diff.y;
            break;
    }
}

void ribbonframe_update(struct RibbonFrame *rf, const Camera2D *cam) {
    assert(rf);
    struct RibbonFrameInternal *internal = rf->internal;

    Vector2 mp = GetMousePosition();

    if (internal->state != S_HANDLE_RESIZE && 
        internal->state != S_HANDLE_CAN_RESIZE &&
        IsMouseButtonDown(internal->mouse_button_bind)) {

        selection_start(rf);

        trace("ribbonframe_update: rect %s\n", rect2str(rf->rect));
    } else {
        //internal->state = S_NONE;

    // TODO: Правильный учет камеры
    if (cam) {
        mp.x += cam->offset.x;
        mp.y += cam->offset.y;
    }

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

    if (true) {
        //trace("ribbonframe_update: CheckCollisionPointCircle\n");

        // Проверка радиуса попадания в ручку управления
        handles_check(internal, points, points_num, mp);
        Vector2 handle_diff = {};

        if (IsMouseButtonDown(internal->mouse_button_bind)) {
            if (internal->corner_index != -1) {
                SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);

                internal->state = S_HANDLE_CAN_RESIZE;
                handle_diff = Vector2Subtract(
                    mp, internal->last_points[internal->corner_index]
                );

                internal->state = S_HANDLE_RESIZE;
            }

        } else {
            SetMouseCursor(0);
            //if (internal->state != S_HANDLE_RESIZE)
                internal->state = S_NONE;
        }

            if (internal->state == S_HANDLE_RESIZE)
                handle_resize(rf, handle_diff);
    }

    }

    //trace("ribbonframe_update: state %s\n", state2str(internal->state));

}

void ribbonframe_draw(struct RibbonFrame *rf) {
    //trace("ribbonframe_draw:\n");
    assert(rf);

    DrawRectangleLinesEx(
        rf->rect, rf->internal->line_thick, rf->internal->line_color
    );

    if (rf->internal->corner_index != -1) {
        DrawCircleV(
            rf->internal->corner_point,
            rf->internal->handle_circle_radius,
            rf->internal->handle_color
        );
    }

    /*
    DrawRectangleRec((Rectangle) {
            .x = 30,
            .y = 30,
            .width = 100,
            .height = 100,
        }, line_color
    );
    DrawRectangleLinesEx((Rectangle) {
            .x = 30,
            .y = 30,
            .width = 100,
            .height = 100,
        }, line_thick, line_color
    );
    */

}
