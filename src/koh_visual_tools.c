#include "koh_visual_tools.h"

#include "chipmunk/chipmunk_types.h"
#include "koh.h"
#include "koh_common.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum State {
    S_NONE,
    S_RESIZE,
    S_CAN_RESIZE,
    S_ROTATE,
    S_CAN_ROTATE,
};

static bool visual_tool_verbose = false;

static const char *state2str(enum State s) {
    switch (s) {
        case S_NONE: return "S_NONE";
        case S_RESIZE: return "S_RESIZE";
        case S_CAN_RESIZE: return "S_HANDLE_CAN_RESIZE";
        case S_ROTATE: return "S_ROTATE";
        case S_CAN_ROTATE: return "S_CAN_ROTATE";
    }
    return NULL;
}
// */

struct ToolPolylineInternal {
    struct ToolCommonOpts       cmn;
    Vector2                     *points;
    int                         points_cap, points_num;
    int                         selected_point_index;
    bool                        drag;
};

struct ToolRectangleAlignedInternal {
    struct ToolCommonOpts       cmn;
    int                         corner_index,
                                points_num;
    enum State                  state;
    Vector2                     corner_point, last_points[4];
};

struct ToolRectangleInternal {
    struct ToolCommonOpts       cmn;
    int                         corner_index,
                                points_num;
    enum State                  state;
    Vector2                     corner_point, last_points[4];
};

struct ToolSectorInternal {
    struct ToolCommonOpts   cmn;
};

static void common_trace(const char *prefix, struct ToolCommonOpts *cmn);
static void build_points_from_rect(
    struct ToolRectangle *r, const Rectangle rect
);
static void rectangle_selection_start(
    struct ToolRectangle *rf, const Camera2D *cam
);

void rectanglea_init(
    struct ToolRectangleAligned *rf,
    const struct ToolRectangleAlignedOpts *opts
) {
    assert(rf);
    trace("rectanglea_init:\n");

    memset(rf, 0, sizeof(*rf));
    rf->internal = malloc(sizeof(struct ToolRectangleAlignedInternal));
    assert(rf->internal);

    struct ToolRectangleAlignedInternal *internal = rf->internal;

    internal->cmn.handle_circle_radius = 30.;
    internal->state = S_NONE;
    internal->cmn.line_thick = 3.;
    internal->cmn.line_color = YELLOW;
    internal->cmn.handle_color = BLUE;
    internal->corner_index = -1;
    internal->points_num = sizeof(internal->last_points) / 
                           sizeof(internal->last_points[0]);
    internal->cmn.snap_size = 1;
    internal->cmn.mouse_button_bind = MOUSE_BUTTON_LEFT;
    internal->cmn.snap = false;
    rectanglea_update_opts(rf, opts);
}

void rectanglea_shutdown(struct ToolRectangleAligned *rf) {
    assert(rf);
    if (rf->internal) {
        free(rf->internal);
        rf->internal = NULL;
    }
}

static Vector2 mouse_with_cam(const Camera2D *cam) {
    assert(cam);
    Vector2 cam_offset = cam ? cam->offset : Vector2Zero();
    float scale = cam ? cam->zoom : 1.;
    Vector2 mp = Vector2Subtract(GetMousePosition(), cam_offset);
    return Vector2Scale(mp, 1. / scale);
}

/*
static Rectangle screen_with_cam(const Camera2D *cam) {
    Rectangle r = {0., 0., GetScreenWidth(), GetScreenHeight()};
    if (cam) {
        Vector2 offset = Vector2Scale(cam->offset, 1. / cam->zoom);
        r.x -= offset.x;
        r.y -= offset.y;
        r.width /= cam->zoom;
        r.height /= cam->zoom;
    }
    return r;
}
*/

static void rotation_start(struct ToolRectangle *rf, const Camera2D *cam) {
    assert(rf);
    assert(rf->internal);

    /*
    struct ToolRectangleInternal *internal = rf->internal;
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
    */
    struct ToolRectangleInternal *internal = rf->internal;
    internal->state = S_ROTATE;
}

static void rectanglea_selection_start(
    struct ToolRectangleAligned *rf, const Camera2D *cam
) {
    assert(rf);
    assert(rf->internal);

    /*trace("selection_start:\n");*/

    struct ToolRectangleAlignedInternal *internal = rf->internal;
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
    struct ToolRectangleAlignedInternal *internal,
    const Vector2 *points, int points_num
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

static void rectanglea_handles_check(
    struct ToolRectangleAlignedInternal *internal, const Vector2 *points, 
    int points_num, Vector2 mp
) {
    assert(internal);
    float radius = internal->cmn.handle_circle_radius;
    //internal->corner_index = -1;
    for (int i = 0; i < internal->points_num; ++i) {
        if (CheckCollisionPointCircle(mp, points[i], radius)) {
            internal->corner_index = i;
            internal->corner_point = points[i];
            break;
        }
    }
}

static void rectangle_handles_check(
    struct ToolRectangleInternal *internal, const Vector2 *points, 
    int points_num, Vector2 mp
) {
    assert(internal);
    float radius = internal->cmn.handle_circle_radius;
    //internal->corner_index = -1;
    for (int i = 0; i < internal->points_num; ++i) {
        if (CheckCollisionPointCircle(mp, points[i], radius)) {
            internal->corner_index = i;
            internal->corner_point = points[i];
            break;
        }
    }
}

static void rectangle_handle_resize(
    struct ToolRectangle *rf, Vector2 handle_diff
) {
    assert(rf);
    assert(rf->internal);
    struct ToolRectangleInternal *internal = rf->internal;
    switch (internal->corner_index) {
        case 0:
            /*
            rf->rect.x += handle_diff.x;
            rf->rect.y += handle_diff.y;
            rf->rect.width -= handle_diff.x;
            rf->rect.height -= handle_diff.y;

            if (internal->cmn.snap) {
                int snap_size = internal->cmn.snap_size;
                int dx = (int)rf->rect.x % snap_size;
                int dy = (int)rf->rect.y % snap_size;
                rf->rect.x = rf->rect.x - dx;
                rf->rect.y = rf->rect.y - dy;
                rf->rect.width = rf->rect.width + dx;
                rf->rect.height = rf->rect.height + dy;
            }
            */
            if (rf->angle == 0.) {
                //rf->points[0] = Vector2Add(rf->points[0], handle_diff);
                //rf->points[1] = Vector2Add(rf->points[1], handle_diff);
                //rf->points[2] = Vector2Add(rf->points[2], handle_diff);
                rf->points[3] = Vector2Add(rf->points[3], handle_diff);
            }
            break;
        case 1:
            // XXX: Тут я не придумал, что написать..
            //rf->rect.y -= handle_diff.y;
            //rf->rect.width += handle_diff.x;
            //rf->rect.height -= handle_diff.y;
            break;
        case 2:
            /*
            rf->rect.width += handle_diff.x;
            rf->rect.height += handle_diff.y;

            if (internal->cmn.snap) {
                int snap_size = internal->cmn.snap_size;
                int dx = (int)rf->rect.width % snap_size;
                int dy = (int)rf->rect.height % snap_size;
                rf->rect.width = rf->rect.width - dx;
                rf->rect.height = rf->rect.height - dy;
            }
            */
            break;
        case 3:
            // XXX: Тут я не придумал, что написать..
            //rf->rect.x -= handle_diff.x;
            //rf->rect.width -= handle_diff.x;
            //rf->rect.height += handle_diff.y;
            break;
    }

    // Ограничение схлопывания и деления на ноль
    //if (rf->rect.width < 0.) rf->rect.width = 0.;
    //if (rf->rect.height < 0.) rf->rect.height = 0.;
    trace(
        "rectangle_handle_resize: points {%s, %s, %s, %s}\n",
        Vector2_tostr(rf->points[0]),
        Vector2_tostr(rf->points[1]),
        Vector2_tostr(rf->points[2]),
        Vector2_tostr(rf->points[3])
    );
}

static void rectanglea_handle_resize(
    struct ToolRectangleAligned *rf, Vector2 handle_diff
) {
    assert(rf);
    assert(rf->internal);
    struct ToolRectangleAlignedInternal *internal = rf->internal;
    switch (internal->corner_index) {
        case 0:
            rf->rect.x += handle_diff.x;
            rf->rect.y += handle_diff.y;
            rf->rect.width -= handle_diff.x;
            rf->rect.height -= handle_diff.y;

            if (internal->cmn.snap) {
                int snap_size = internal->cmn.snap_size;
                int dx = (int)rf->rect.x % snap_size;
                int dy = (int)rf->rect.y % snap_size;
                rf->rect.x = rf->rect.x - dx;
                rf->rect.y = rf->rect.y - dy;
                rf->rect.width = rf->rect.width + dx;
                rf->rect.height = rf->rect.height + dy;
            }
            break;
        case 1:
            // XXX: Тут я не придумал, что написать..
            //rf->rect.y -= handle_diff.y;
            //rf->rect.width += handle_diff.x;
            //rf->rect.height -= handle_diff.y;
            break;
        case 2:
            rf->rect.width += handle_diff.x;
            rf->rect.height += handle_diff.y;

            if (internal->cmn.snap) {
                int snap_size = internal->cmn.snap_size;
                int dx = (int)rf->rect.width % snap_size;
                int dy = (int)rf->rect.height % snap_size;
                rf->rect.width = rf->rect.width - dx;
                rf->rect.height = rf->rect.height - dy;
            }
            break;
        case 3:
            // XXX: Тут я не придумал, что написать..
            //rf->rect.x -= handle_diff.x;
            //rf->rect.width -= handle_diff.x;
            //rf->rect.height += handle_diff.y;
            break;
    }

    // Ограничение схлопывания и деления на ноль
    if (rf->rect.width < 0.) rf->rect.width = 0.;
    if (rf->rect.height < 0.) rf->rect.height = 0.;
}

static void rectangle_update_verts(struct ToolRectangle *rf) {
    if (IsKeyDown(KEY_LEFT)) {
        rf->angle += 0.01;
    }
    if (IsKeyDown(KEY_RIGHT)) {
        rf->angle -= 0.01;
    }

    /*
    rf->radius = sqrt(
        rf->rect.width * rf->rect.width +
        rf->rect.height * rf->rect.height
    ) / 2.;
    rf->center =  (Vector2) {
        .x = rf->rect.x + rf->rect.width / 2.,
        .y = rf->rect.y + rf->rect.height / 2.,
    };
    */

    cpTransform t = cpTransformIdentity;
    //t = cpTransformMult(cpTransformTranslate(from_Vector2(rf->center)), t);
    //t = cpTransformMult(t, cpTransformTranslate(from_Vector2(rf->center)));
    cpTransform rot = cpTransformRotate(rf->angle);

    cpTransform result = cpTransformMult(t, rot);

    /*
    cpTransform result = cpTransformMult(
        t, cpTransformMult(
            //rot, cpTransformTranslate(from_Vector2(Vector2Negate(rf->center)))
            //cpTransformTranslate(from_Vector2(Vector2Negate(rf->center))), rot
            cpTransformTranslate(from_Vector2(Vector2Negate(rf->center))), rot
        )
    );
    */

    Vector2 *points = rf->points;


    points[0] = from_Vect(cpTransformVect(result, from_Vector2(points[0])));
    points[1] = from_Vect(cpTransformVect(result, from_Vector2(points[1])));
    points[2] = from_Vect(cpTransformVect(result, from_Vector2(points[2])));
    points[3] = from_Vect(cpTransformVect(result, from_Vector2(points[3])));
    points[4] = points[0];

    /*
    trace("ribbonframe_update_verts: point[0] %s\n", Vector2_tostr(points[0]));
    trace("ribbonframe_update_verts: point[1] %s\n", Vector2_tostr(points[1]));
    trace("ribbonframe_update_verts: point[2] %s\n", Vector2_tostr(points[2]));
    trace("ribbonframe_update_verts: point[3] %s\n", Vector2_tostr(points[3]));
    */
}

static void rectanglea_set_state_by_corner_index(
    struct ToolRectangleAlignedInternal *internal
) {
    int corner_index = internal->corner_index;
    if (corner_index != -1) {
        if (corner_index == 0 || corner_index == 2) {
            internal->state = S_CAN_RESIZE;
            SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
        } 
    }
}

static void rectangle_set_state_by_corner_index(
    struct ToolRectangleInternal *internal
) {
    int corner_index = internal->corner_index;
    if (corner_index != -1) {
        if (corner_index == 0 || corner_index == 2) {
            internal->state = S_CAN_RESIZE;
            SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
        } else if (corner_index == 1 || corner_index == 3) {
            internal->state = S_CAN_ROTATE;
            SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
        }
    }
}

static void rectanglea_parse_state(
    struct ToolRectangleAligned *rf, const Camera2D *cam, Vector2 mp
) {
    assert(rf);
    assert(cam);
    struct ToolRectangleAlignedInternal *internal = rf->internal;
    switch(internal->state) {
        case S_NONE: 
            rectanglea_selection_start(rf, cam);
            break;
        case S_RESIZE:
            rectanglea_selection_start(rf, cam);
            break;
        case S_CAN_RESIZE: {
            Vector2 handle_diff = Vector2Subtract(
                mp, internal->last_points[internal->corner_index]
            );
            rectanglea_handle_resize(rf, handle_diff);
            break;
        }
        default:
            trace(
                "rectanglea_parse_state: bad value '%s' in switch\n",
                state2str(internal->state)
            );
    };
}

static void rectangle_parse_state(
    struct ToolRectangle *rf, const Camera2D *cam, Vector2 mp
) {
    assert(rf);
    assert(cam);
    struct ToolRectangleInternal *internal = rf->internal;
    switch(internal->state) {
        case S_NONE: 
            rectangle_selection_start(rf, cam);
            break;
        case S_RESIZE:
            rectangle_selection_start(rf, cam);
            break;
        case S_CAN_RESIZE: {
            Vector2 handle_diff = Vector2Subtract(
                mp, internal->last_points[internal->corner_index]
            );
            rectangle_handle_resize(rf, handle_diff);
            break;
        }
        case S_CAN_ROTATE:
            rotation_start(rf, cam);
            break;
        case S_ROTATE: {
            Vector2 point = Vector2Subtract(rf->center, mp);
            trace("rectangle_parse_state: point %s\n", Vector2_tostr(point));
            float angle = atan2(point.y, point.y);
            trace("rectangle_parse_state: angle %f\n", angle);
            rf->angle = angle;
            break;
        }
        default:
            trace(
                "rectangle_parse_state: bad value '%s' in switch\n",
                state2str(internal->state)
            );
    };
}

void rectanglea_update(struct ToolRectangleAligned *rf, const Camera2D *cam) {
    assert(rf);
    struct ToolRectangleAlignedInternal *internal = rf->internal;
    Vector2 mp = mouse_with_cam(cam);

    //rectanglea_update_verts(rf);

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
    rectanglea_handles_check(internal, points, points_num, mp);
   
    /*
    if ((internal->corner_index == 1 || internal->corner_index == 3) &&
        IsMouseButtonDown(internal->cmn.mouse_button_bind)) {
    }
    */

    if (IsMouseButtonDown(internal->cmn.mouse_button_bind)) {

        if (internal->state == S_NONE) {
            rectanglea_set_state_by_corner_index(internal);
        }
        rectanglea_parse_state(rf, cam, mp);

    } else {
        internal->state = S_NONE;
        //rf->exist = false;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    trace("rectanglea_update: state %s\n", state2str(internal->state));
    //trace("rectanglea_update: angle %f\n", rf->angle);
}

static void rectanglea_draw_axises(
    struct ToolRectangleAligned *rf,
    const struct ToolRectangleAlignedDrawOpts *opts
) {
    assert(rf);
    struct ToolRectangleAlignedInternal *internal = rf->internal;
    struct ToolCommonOpts *cmn = &internal->cmn;
    if (opts && opts->draw_axises) {
        DrawLineEx(
            (Vector2) {
                .x = rf->rect.x + rf->rect.width / 2.,
                .y = rf->rect.y,
            },
            (Vector2) {
                .x = rf->rect.x + rf->rect.width / 2.,
                .y = rf->rect.y + rf->rect.height,
            },
            cmn->line_thick, cmn->line_color
        );
        DrawLineEx(
            (Vector2) {
                .x = rf->rect.x,
                .y = rf->rect.y + rf->rect.height / 2.,
            },
            (Vector2) {
                .x = rf->rect.x + rf->rect.width,
                .y = rf->rect.y + rf->rect.height / 2.,
            },
            cmn->line_thick, cmn->line_color
        );
    }
}

void rectanglea_draw(
    struct ToolRectangleAligned *rf,
    const struct ToolRectangleAlignedDrawOpts *opts
) {
    //trace("rectanglea_draw:\n");
    assert(rf);
    assert(rf->internal);
    struct ToolRectangleAlignedInternal *internal = rf->internal;
    struct ToolCommonOpts *cmn = &internal->cmn;

    rectanglea_draw_axises(rf, opts);
    DrawRectangleLinesEx(rf->rect, cmn->line_thick, cmn->line_color);

    if (internal->corner_index == 0 || internal->corner_index == 2) {
        DrawCircleV(
            internal->corner_point,
            cmn->handle_circle_radius,
            cmn->handle_color
        );
    }
}

void rectanglea_update_opts(
    struct ToolRectangleAligned *rf,
    const struct ToolRectangleAlignedOpts *new_opts
) {
    assert(rf);

    if (!new_opts)
        return;

    struct ToolRectangleAlignedInternal *internal = rf->internal;
    struct ToolCommonOpts *cmn = &internal->cmn;
    assert(internal);

    if (new_opts->common.mouse_button_bind != -1)
        cmn->mouse_button_bind = new_opts->common.mouse_button_bind;
    cmn->snap_size = new_opts->snap_size;
    cmn->snap = new_opts->snap;
    cmn->line_color = new_opts->common.line_color;
    cmn->handle_color = new_opts->common.handle_color;
    cmn->line_thick = new_opts->common.line_thick;
    common_trace("rectanglea_update_opts:", cmn);
}

void polyline_init(
    struct ToolPolyline *plt, const struct ToolPolylineOpts *opts
) {
    assert(plt);
    trace("polyline_init:\n");
    memset(plt, 0, sizeof(*plt));
    plt->internal = calloc(1, sizeof(struct ToolPolylineInternal));
    assert(plt->internal);
    polyline_update_opts(plt, opts);
    struct ToolPolylineInternal *internal = plt->internal;
    internal->cmn.handle_circle_radius = 30.;

    internal->points_cap = 100;
    internal->points = calloc(
        internal->points_cap, sizeof(internal->points[0])
    );
    assert(internal->points);

    internal->selected_point_index = -1;
    internal->drag = false;
}

static void common_trace(const char *prefix, struct ToolCommonOpts *cmn) {
    assert(cmn);

    if (!visual_tool_verbose) return;

    const char *_prefix = prefix ? prefix : " ";
    trace("%s mouse_button_bind %d\n", _prefix, cmn->mouse_button_bind);
    trace("%s line_color %s\n", _prefix, color2str(cmn->line_color));
    trace("%s handle_color %s\n", _prefix, color2str(cmn->handle_color));
    trace(
        "%s handle_selected_color %s\n", _prefix,
        color2str(cmn->handle_color_selected)
    );
    trace("%s line_thick %f\n", _prefix, cmn->line_thick);
    trace("%s snap %s\n", _prefix, cmn->snap ? "true" : "false");
    trace("%s snap_size %d\n", _prefix, cmn->snap_size);
    trace("%s handle_circle_radius %f\n", _prefix, cmn->handle_circle_radius);
}

void polyline_update_opts(
    struct ToolPolyline *plt, const struct ToolPolylineOpts *new_opts
) {
    assert(plt);
    assert(new_opts);
    struct ToolPolylineInternal *internal = plt->internal;

    if (!internal) {
        trace("polyline_update_opts: internal == NULL\n");
        return;
    }

    internal->cmn.line_color = new_opts->common.line_color;
    trace(
        "polyline_update_opts: line_color %s\n",
        color2str(new_opts->common.line_color)
    );
    struct ToolCommonOpts *cmn = &internal->cmn;
    cmn->handle_color = new_opts->common.handle_color;
    cmn->handle_color_selected = new_opts->common.handle_color_selected;
    cmn->line_thick = new_opts->common.line_thick;
    cmn->mouse_button_bind = new_opts->common.mouse_button_bind;

    common_trace("polyline_update_opts:", cmn);
}

void polyline_shutdown(struct ToolPolyline *plt) {
    assert(plt);
    trace("polyline_shutdown:\n");
    struct ToolPolylineInternal *internal = plt->internal;
    if (plt->internal && internal->points) {
        free(internal->points);
        internal->points = NULL;
    }
    if (plt->internal) {
        free(plt->internal);
        plt->internal = NULL;
    }
}

static void point_add(
    struct ToolPolyline *plt, Vector2 point, bool close_line
) {
    assert(plt);
    struct ToolPolylineInternal *internal = plt->internal;
    if (internal->points_cap == internal->points_num + 1) {
        internal->points_cap += 1;
        internal->points_cap *= 2;
        trace("point_add: points_cap %d\n", internal->points_cap);
        int arr_size = sizeof(internal->points[0]) * internal->points_cap;
        void *new_ptr = realloc(internal->points, arr_size);
        if (!new_ptr) {
            trace(
                "point_add: could not realloc to %d capacity\n",
                internal->points_cap
            );
            exit(EXIT_FAILURE);
        }
        internal->points = new_ptr;
    }

    trace("point_add: close_line %s\n", close_line ? "true" : "false");
    if (close_line && internal->points_num >= 3) {
        internal->points[internal->points_num++] = internal->points[0];
    } else
        internal->points[internal->points_num++] = point;
}

static void polyline_remove(struct ToolPolyline *plt, int index) {
    trace("polyline_remove: index %d\n", index);
    struct ToolPolylineInternal *internal = plt->internal;
    assert(plt);
    assert(internal);
    assert(index >= 0);
    assert(index < internal->points_num);

    if (!internal->points_num)
        return;

    // FIXME: Вычитать еденицу или нет?
    //int rest_num = internal->points_num - index - 1;
    int rest_num = internal->points_num - index - 0;

    memmove(
        &internal->points[index], &internal->points[index + 1],
        sizeof(internal->points[0]) * rest_num
    );
    internal->points_num--;
    trace(
        "polyline_remove: points_num %d, rest_num %d\n",
        internal->points_num, rest_num
    );
}

void polyline_update(struct ToolPolyline *plt, const Camera2D *cam) {
    //trace("polyline_update\n");
    assert(plt);
    struct ToolPolylineInternal *internal = plt->internal;
    assert(internal);
    Vector2 mp = mouse_with_cam(cam);
    internal->selected_point_index = -1;
    internal->drag = false;
    bool removed = false;
    for (int j = 0; j < internal->points_num; j++) {
        Vector2 point = internal->points[j];
        float radius = internal->cmn.handle_circle_radius;
        if (CheckCollisionPointCircle(mp, point, radius)) {
            //trace("polyline_update: j = %d\n", j);
            internal->selected_point_index = j;
            if (IsKeyDown(KEY_LEFT_SHIFT) && 
                IsMouseButtonPressed(internal->cmn.mouse_button_bind)
            ) {
                polyline_remove(plt, j);
                removed = true;
                break;
            } else {
                if (IsMouseButtonDown(internal->cmn.mouse_button_bind)) {
                    internal->drag = true;
                    internal->points[j] = mp;
                    break;
                }
            }
        }
    }

    if (!internal->drag && !removed && 
        IsMouseButtonPressed(internal->cmn.mouse_button_bind)) {
        point_add(plt, mp, IsMouseButtonDown(KEY_LEFT_CONTROL));
    }
}

void polyline_draw(
    struct ToolPolyline *plt, struct ToolPolylineDrawOpts *opts, 
    const Camera2D *cam
) {
    assert(plt);
    struct ToolPolylineInternal *internal = plt->internal;
    assert(internal);

    // Получить видимые координаты экрана после преобразования камеры
    //const Rectangle screen = screen_with_cam(cam);

    for (int j = 0; j < internal->points_num; j++) {
        Color color = internal->selected_point_index == j ?
            internal->cmn.handle_color_selected : internal->cmn.handle_color;
        DrawCircleV(
            internal->points[j], internal->cmn.handle_circle_radius, color
        );
    }

    // TODO: Проверить линию на вхождение в камеру и выполнить отсечение
    Vector2 prev_point = internal->points[0];
    for (int j = 0; j < internal->points_num; j++) {
        Vector2 point = internal->points[j];
        DrawLineEx(
            prev_point, point, internal->cmn.line_thick, 
            internal->cmn.line_color
        );
        prev_point = point;
    }
}

// XXX: Кто ответственнен за возвращенную память?
Vector2 *polyline_points_get(struct ToolPolyline *plt, int *points_num) {
    assert(plt);
    return NULL;
}

void polyline_points_set(
    struct ToolPolyline *plt, Vector2 *points, int points_num
) {
    assert(plt);
    assert(points);
    assert(points_num);

    struct ToolPolylineInternal *internal = plt->internal;
    assert(internal);

    if (!internal->points) {
        trace("polyline_points_set: intenal->points == NULL\n");
        exit(EXIT_FAILURE);
    }

    int points_arr_sz = sizeof(internal->points[0]) * points_num;

    if (internal->points_cap <= points_num) {
        internal->points_cap = points_num;
        int size = sizeof(internal->points[0]) * internal->points_cap;
        void *new_ptr = realloc(internal->points, size);
        if (!new_ptr) {
            trace(
                "polyline_points_get: could not realloc to %d capaticy\n",
                internal->points_cap
            );
            exit(EXIT_FAILURE);
        }
        internal->points = new_ptr;
    }

    memmove(internal->points, points, points_arr_sz);
    internal->points_num = points_num;
}

void sector_init(
    struct ToolSector *sec, const struct ToolSectorOpts *opts
) {
    assert(sec);
    memset(sec, 0, sizeof(*sec));
    sec->internal = malloc(sizeof(struct ToolSectorInternal));
    assert(sec->internal);
    sector_update_opts(sec, opts);
}

void sector_update_opts(
    struct ToolSector *sec, const struct ToolSectorOpts *new_opts
) {
    struct ToolSectorInternal *internal = sec->internal;
    common_trace("sector_update_opts:", &internal->cmn);
}

void sector_shutdown(struct ToolSector *sec) {
    assert(sec);
    if (sec->internal) {
        free(sec->internal);
        sec->internal = NULL;
    }
}

void sector_update(struct ToolSector *sec, const Camera2D *cam) {
    assert(sec);
    //struct ToolSectorInternal *internal = sec->internal;
    //Vector2 mp = mouse_with_cam(cam);
}

void sector_draw(
    struct ToolSector *sec, struct ToolSectorDrawOpts *opts,
    const Camera2D *cam
) {
    assert(sec);
    assert(opts);
    int segments_num = sec->radius * 2;
    struct ToolSectorInternal *internal = sec->internal;
    DrawCircleSector(
        sec->position, sec->radius,
        sec->angle1, sec->angle2,
        segments_num, internal->cmn.line_color
    );
}

/*
void visual_tool_init(
    struct VisualTool *vt,
    struct ToolRectangleOpts *t_rect_opts,
    struct ToolRectangleOpts *t_rect_oriented_opts,
    struct ToolSectorOpts *t_sector_opts,
    struct ToolPolylineDrawOpts *t_pl_draw_opts
) {
    assert(vt);
}
*/

void visual_tool_shutdown(struct VisualTool *vt) {
    assert(vt);
    rectanglea_shutdown(&vt->t_recta);
    rectangle_shutdown(&vt->t_rect);
    polyline_shutdown(&vt->t_pl);
    sector_shutdown(&vt->t_sector);
}

/*
static const char *mode2str(enum VisualToolMode mode) {
    switch (mode) {
        case VIS_TOOL_RECTANGLE: return "RECTANGLE";  
        case VIS_TOOL_RECTANGLE_ORIENTED: return "RECTANGLE_ORIENTED"; 
        case VIS_TOOL_POLYLINE: return "POLYLINE"; 
        case VIS_TOOL_SECTOR: return "SECTOR"; 
    }
    return NULL;
}
// */

void visual_tool_update(struct VisualTool *vt, const Camera2D *cam) {
    assert(vt);
    //trace("visual_tool_update: mode %s\n", mode2str(vt->mode));
    switch (vt->mode) {
        case VIS_TOOL_POLYLINE:
            //if (vt->t_pl.exist)
                polyline_update(&vt->t_pl, cam);
            break;
        case VIS_TOOL_RECTANGLE:
            //if (vt->t_rect.exist)
                rectanglea_update(&vt->t_recta, cam);
            break;
        case VIS_TOOL_RECTANGLE_ORIENTED:
            //if (vt->t_rect_oriented.exist)
                rectangle_update(&vt->t_rect, cam);
            break;
        case VIS_TOOL_SECTOR:
            //if (vt->t_sector.exist)
                sector_update(&vt->t_sector, cam);
            break;
    }
}

void visual_tool_draw(struct VisualTool *vt, const Camera2D *cam) {
    assert(vt);
    //bool trace_nonexist = true;
    switch (vt->mode) {
        case MLT_POLYLINE:
            //if (vt->t_pl.exist)
                polyline_draw(&vt->t_pl, &vt->t_pl_draw_opts, cam);
            //else if (trace_nonexist)
                //trace("visual_tool_draw: polyline is not exists\n");
            break;
        case MLT_RECTANGLE:
            //if (vt->t_rect.exist)
                rectanglea_draw(&vt->t_recta, &vt->t_recta_draw_opts);
            //else if (trace_nonexist)
                //trace("visual_tool_draw: rectangle is not exists\n");
            break;
        case MLT_RECTANGLE_ORIENTED:
            //if (vt->t_rect_oriented.exist)
                rectangle_draw(&vt->t_rect, &vt->t_rect_draw_opts);
            //else if (trace_nonexist)
                //trace("visual_tool_draw: rectangle oriented is not exists\n");
            break;
        case MLT_SECTOR:
            //if (vt->t_sector.exist)
                sector_draw(&vt->t_sector, &vt->t_sector_draw_opts, cam);
            //else if (trace_nonexist)
                //trace("visual_tool_draw: sector is not exists\n");
            break;
        default:
            trace("visual_tool_draw: unknown value in switch\n");
    }
}

void visual_tool_reset_all(struct VisualTool *vt) {
    assert(vt);

    if (vt->t_pl.internal) {
        polyline_shutdown(&vt->t_pl);
        polyline_init(&vt->t_pl, &vt->t_pl_opts);
    }

    if (vt->t_recta.internal) {
        rectanglea_shutdown(&vt->t_recta);
        rectanglea_init(&vt->t_recta, &vt->t_recta_opts);
    }

    if (vt->t_rect.internal) {
        rectangle_shutdown(&vt->t_rect);
        rectangle_init(&vt->t_rect, &vt->t_rect_opts);
    }

    if (vt->t_sector.internal) {
        sector_shutdown(&vt->t_sector);
        sector_init(&vt->t_sector, &vt->t_sector_opts);
    }

}

void rectangle_init(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *opts
) {
    assert(rf);
    trace("rectangle_init:\n");

    memset(rf, 0, sizeof(*rf));
    rf->internal = malloc(sizeof(struct ToolRectangleInternal));
    assert(rf->internal);

    struct ToolRectangleInternal *internal = rf->internal;

    internal->cmn.handle_circle_radius = 30.;
    internal->state = S_NONE;
    internal->cmn.line_thick = 3.;
    internal->cmn.line_color = YELLOW;
    internal->cmn.handle_color = BLUE;
    internal->corner_index = -1;
    internal->points_num = sizeof(internal->last_points) / 
                           sizeof(internal->last_points[0]);
    internal->cmn.snap_size = 1;
    internal->cmn.mouse_button_bind = MOUSE_BUTTON_LEFT;
    internal->cmn.snap = false;
    rf->angle = 0.;
    rf->exist = false;
    rectangle_update_opts(rf, opts);
}

static void build_points_from_rect(
    struct ToolRectangle *r, const Rectangle rect
) {
    assert(r);
    r->angle = 0.;
    Vector2 *points = r->points;

    points[0].x = rect.x;
    points[0].y = rect.y;
    points[1].x = rect.x + rect.width;
    points[1].y = rect.y;
    points[2].x = rect.x + rect.width;
    points[2].y = rect.y + rect.height;
    points[3].x = rect.x;
    points[3].y = rect.y + rect.height;
    points[4].x = points[0].x;
    points[4].y = points[0].y;
}

void rectangle_update_opts(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *new_opts
) {
    assert(rf);

    if (!new_opts)
        return;

    struct ToolRectangleInternal *internal = rf->internal;
    struct ToolCommonOpts *cmn = &internal->cmn;
    assert(internal);

    if (new_opts->common.mouse_button_bind != -1)
        cmn->mouse_button_bind = new_opts->common.mouse_button_bind;
    //cmn->snap_size = new_opts->snap_size;
    //cmn->snap = new_opts->snap;
    cmn->line_color = new_opts->common.line_color;
    cmn->handle_color = new_opts->common.handle_color;
    cmn->line_thick = new_opts->common.line_thick;
    common_trace("rectangle_update_opts:", cmn);
}

void rectangle_shutdown(struct ToolRectangle *rf) {
    assert(rf);
    if (rf->internal) {
        free(rf->internal);
        rf->internal = NULL;
    }
}

static void rectangle_selection_start(
    struct ToolRectangle *rf, const Camera2D *cam
) {
    assert(rf);
    assert(rf->internal);

    /*trace("selection_start:\n");*/

    struct ToolRectangleInternal *internal = rf->internal;
    Vector2 mp = mouse_with_cam(cam);

    if (internal->state == S_NONE) {
        //rf->rect.x = mp.x;
        //rf->rect.y = mp.y;

        build_points_from_rect(rf, (Rectangle){
            .x = mp.x,
            .y = mp.y,
            .width = 30.,
            .height = 30.,
        });
        // */
        
        //rf->points[0].x = mp.x;
        //rf->points[0].y = mp.y;
    }

    internal->state = S_RESIZE;

    // XXX: приращение слишком большое
    Vector2 handle_diff = Vector2Subtract(mp, rf->points[0]);
    trace(
        "rectangle_selection_start: handle_diff %s\n",
        Vector2_tostr(handle_diff)
    );
    rf->points[1] = Vector2Add(rf->points[1], handle_diff);
    rf->points[2] = Vector2Add(rf->points[2], handle_diff);
    rf->points[3] = Vector2Add(rf->points[3], handle_diff);
    rf->points[4] = rf->points[0];

    /*
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
    */

    rf->exist = true;
}


void rectangle_update(struct ToolRectangle *rf, const Camera2D *cam) {
    assert(rf);
    struct ToolRectangleInternal *internal = rf->internal;
    Vector2 mp = mouse_with_cam(cam);

    rectangle_update_verts(rf);

    internal->corner_index = -1;

    // Проверка радиуса попадания в ручку управления
    rectangle_handles_check(internal, rf->points, 4, mp);
   
    /*
    if ((internal->corner_index == 1 || internal->corner_index == 3) &&
        IsMouseButtonDown(internal->cmn.mouse_button_bind)) {
    }
    */

    if (IsMouseButtonDown(internal->cmn.mouse_button_bind)) {

        if (internal->state == S_NONE) {
            rectangle_set_state_by_corner_index(internal);
        }
        rectangle_parse_state(rf, cam, mp);

    } else {
        internal->state = S_NONE;
        //rf->exist = false;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    trace("rectangle_update: state %s\n", state2str(internal->state));
    trace("rectangle_update: angle %f\n", rf->angle);
}

void rectangle_draw(
    struct ToolRectangle *rf, const struct ToolRectangleDrawOpts *opts
) {
    assert(rf);
    struct ToolRectangleInternal *internal = rf->internal;
    
    //trace("rectanglea_draw: is_oriented == true\n");
    Vector2 center = rf->center;
    //Color color = cmn->line_color;
    Color color = YELLOW;
    DrawCircleLines(center.x, center.y, rf->radius, color);
    DrawLineStrip(rf->points, 5, color);
    DrawCircleV(rf->center, 15, BLACK);

    // TODO: Кружки вращения, как добавить?
    if (internal->corner_index == 0 || internal->corner_index == 2) {
        DrawCircleV(
            internal->corner_point,
            internal->cmn.handle_circle_radius,
            internal->cmn.handle_color
        );
    } else if (internal->corner_index == 1 || internal->corner_index == 3) {
        DrawCircleV(
            internal->corner_point,
            internal->cmn.handle_circle_radius,
            internal->cmn.handle_color_alternative
        );
    }

}

