#include "koh_visual_tools.h"

#include "koh.h"
#include "koh_common.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

struct ToolCommonInternal {
    int     mouse_button_bind;
    Color   line_color, handle_color, handle_selected_color;
    float   line_thick;
    float   handle_circle_radius;
    bool    snap;
    int     snap_size;
};

struct ToolPolylineInternal {
    struct ToolCommonInternal   cmn;
    Vector2                     *points;
    int                         points_cap, points_num;
    int                         selected_point_index;
    bool                        drag;
};

struct ToolRectangleInternal {
    struct ToolCommonInternal   cmn;
    int                         corner_index,
                                points_num;
    enum State                  state;
    Vector2                     corner_point, last_points[4];
};

struct ToolSectorInternal {
    struct ToolCommonInternal   cmn;
};

void ribbonframe_init(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *opts
) {
    assert(rf);
    trace("ribbonframe_init:\n");

    memset(rf, 0, sizeof(*rf));
    rf->internal = malloc(sizeof(*rf->internal));
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
    ribbonframe_update_opts(rf, opts);
}

void ribbonframe_shutdown(struct ToolRectangle *rf) {
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

static void selection_start(struct ToolRectangle *rf, const Camera2D *cam) {
    assert(rf);
    assert(rf->internal);

    /*trace("selection_start:\n");*/

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
}

static void last_points_copy(
    struct ToolRectangleInternal *internal, const Vector2 *points, int points_num
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

//__attribute__((unused))
static void handle_resize(
    struct ToolRectangle *rf, Vector2 handle_diff
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

            if (rf->internal->cmn.snap) {
                int snap_size = rf->internal->cmn.snap_size;
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

            if (rf->internal->cmn.snap) {
                int snap_size = rf->internal->cmn.snap_size;
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

void ribbonframe_update(struct ToolRectangle *rf, const Camera2D *cam) {
    assert(rf);
    struct ToolRectangleInternal *internal = rf->internal;
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
    
    if (IsMouseButtonDown(internal->cmn.mouse_button_bind)) {

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
    struct ToolRectangle *rf, struct ToolRectangleDrawOpts *opts
) {
    //trace("ribbonframe_draw:\n");
    assert(rf);
    assert(rf->internal);
    struct ToolRectangleInternal *internal = rf->internal;

    DrawRectangleLinesEx(
        rf->rect, rf->internal->cmn.line_thick, rf->internal->cmn.line_color
    );

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
            rf->internal->cmn.line_thick, rf->internal->cmn.line_color
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
            rf->internal->cmn.line_thick, rf->internal->cmn.line_color
        );
    }

    if (internal->corner_index == 0 || internal->corner_index == 2) {
        DrawCircleV(
            rf->internal->corner_point,
            rf->internal->cmn.handle_circle_radius,
            rf->internal->cmn.handle_color
        );
    }

}

void ribbonframe_update_opts(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *new_opts
) {
    assert(rf);

    if (!new_opts)
        return;

    struct ToolRectangleInternal *internal = rf->internal;
    assert(internal);

    if (new_opts->common.mouse_button_bind != -1)
        internal->cmn.mouse_button_bind = new_opts->common.mouse_button_bind;
    internal->cmn.snap_size = new_opts->snap_size;
    internal->cmn.snap = new_opts->snap;

    /*if (!color_eq(new_opts->line_color, internal->line_color))*/
        internal->cmn.line_color = new_opts->common.line_color;
    /*if (!color_eq(new_opts->handle_color, internal->handle_color))*/
        internal->cmn.handle_color = new_opts->common.handle_color;
    /*if (new_opts->line_thick != internal->line_thick)*/
        internal->cmn.line_thick = new_opts->common.line_thick;
}

void polyline_init(
    struct ToolPolyline *plt, const struct ToolPolylineOpts *opts
) {
    assert(plt);
    trace("polyline_init:\n");
    memset(plt, 0, sizeof(*plt));
    plt->internal = calloc(1, sizeof(*plt->internal));
    assert(plt->internal);
    polyline_update_opts(plt, opts);
    struct ToolPolylineInternal *internal = plt->internal;
    internal->cmn.handle_circle_radius = 30.;

    internal->points_cap = 100;
    internal->points = calloc(
        internal->points_cap, sizeof(plt->internal->points[0])
    );
    assert(internal->points);

    internal->selected_point_index = -1;
    internal->drag = false;
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
    internal->cmn.handle_color = new_opts->common.handle_color;
    internal->cmn.handle_selected_color = new_opts->common.handle_color_selected;
    internal->cmn.line_thick = new_opts->common.line_thick;
    internal->cmn.mouse_button_bind = new_opts->common.mouse_button_bind;
}

void polyline_shutdown(struct ToolPolyline *plt) {
    assert(plt);
    trace("polyline_shutdown:\n");
    if (plt->internal && plt->internal->points) {
        free(plt->internal->points);
        plt->internal->points = NULL;
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
    assert(plt);
    assert(index >= 0);
    assert(index < plt->internal->points_num);

    struct ToolPolylineInternal *internal = plt->internal;
    assert(internal);

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
            internal->cmn.handle_selected_color : internal->cmn.handle_color;
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
    sec->internal = malloc(sizeof(*sec->internal));
    assert(sec->internal);
    sector_update_opts(sec, opts);
}

void sector_update_opts(
    struct ToolSector *sec, const struct ToolSectorOpts *new_opts
) {
}

void sector_shutdown(struct ToolSector *sec) {
    assert(sec);
    if (sec->internal) {
        free(sec->internal);
        sec->internal = NULL;
    }
}

void sector_update(struct ToolSector *sec, const Camera2D *cam) {
}

void sector_draw(
    struct ToolSector *sec, struct ToolSectorDrawOpts *opts,
    const Camera2D *cam
) {
    assert(sec);
    assert(opts);
    int segments_num = sec->radius * 2;
    DrawCircleSector(
        sec->position, sec->radius,
        sec->angle1, sec->angle2,
        segments_num, sec->internal->cmn.line_color
    );
}


