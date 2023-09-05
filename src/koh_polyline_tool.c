#include "koh_polyline_tool.h"

#include "raylib.h"
#include "raymath.h"
#include "koh_logger.h"
#include <allegro/fli.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct PolylineToolInternal {
    Vector2 *points;
    int     points_cap, points_num;
    int     mouse_button_bind;
    Color   line_color, handle_color, handle_selected_color;
    float   line_thick;
    float   handle_circle_radius;
    int     selected_point_index;
    bool    drag;
};

void polyline_init(
    struct PolylineTool *plt, const struct PolylineToolOpts *opts
) {
    assert(plt);
    trace("polyline_init:\n");
    memset(plt, 0, sizeof(*plt));
    plt->internal = calloc(1, sizeof(*plt->internal));
    assert(plt->internal);
    polyline_update_opts(plt, opts);
    struct PolylineToolInternal *internal = plt->internal;
    internal->handle_circle_radius = 30.;

    internal->points_cap = 100;
    internal->points = calloc(
        internal->points_cap, sizeof(plt->internal->points[0])
    );
    assert(internal->points);

    internal->selected_point_index = -1;
    internal->drag = false;
}

void polyline_update_opts(
    struct PolylineTool *plt, const struct PolylineToolOpts *new_opts
) {
    assert(plt);
    assert(new_opts);
    struct PolylineToolInternal *internal = plt->internal;

    if (!internal)
        return;

    internal->line_color = new_opts->line_color;
    internal->handle_color = new_opts->handle_color;
    internal->handle_selected_color = new_opts->handle_selected_color;
    internal->line_thick = new_opts->line_thick;
    internal->mouse_button_bind = new_opts->mouse_button_bind;
}

void polyline_shutdown(struct PolylineTool *plt) {
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

static Vector2 mouse_with_cam(const Camera2D *cam) {
    Vector2 cam_offset = cam ? cam->offset : Vector2Zero();
    float scale = cam ? cam->zoom : 1.;
    Vector2 mp = Vector2Subtract(GetMousePosition(), cam_offset);
    return Vector2Scale(mp, 1. / scale);
}

static void point_add(
    struct PolylineTool *plt, Vector2 point, bool close_line
) {
    assert(plt);
    struct PolylineToolInternal *internal = plt->internal;
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

static void polyline_remove(struct PolylineTool *plt, int index) {
    trace("polyline_remove: index %d\n", index);
    assert(plt);
    assert(index >= 0);
    assert(index < plt->internal->points_num);

    struct PolylineToolInternal *internal = plt->internal;
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

void polyline_update(struct PolylineTool *plt, const Camera2D *cam) {
    //trace("polyline_update\n");
    assert(plt);
    struct PolylineToolInternal *internal = plt->internal;
    assert(internal);
    Vector2 mp = mouse_with_cam(cam);
    internal->selected_point_index = -1;
    internal->drag = false;
    bool removed = false;
    for (int j = 0; j < internal->points_num; j++) {
        Vector2 point = internal->points[j];
        float radius = internal->handle_circle_radius;
        if (CheckCollisionPointCircle(mp, point, radius)) {
            //trace("polyline_update: j = %d\n", j);
            internal->selected_point_index = j;
            if (IsKeyDown(KEY_LEFT_SHIFT) && 
                IsMouseButtonPressed(internal->mouse_button_bind)
            ) {
                polyline_remove(plt, j);
                removed = true;
                break;
            } else {
                if (IsMouseButtonDown(internal->mouse_button_bind)) {
                    internal->drag = true;
                    internal->points[j] = mp;
                    break;
                }
            }
        }
    }

    if (!internal->drag && !removed && 
        IsMouseButtonPressed(internal->mouse_button_bind)) {
        point_add(plt, mp, IsMouseButtonDown(KEY_LEFT_CONTROL));
    }
}

void polyline_draw(
    struct PolylineTool *plt, struct PolylineToolDrawOpts *opts
) {
    assert(plt);
    struct PolylineToolInternal *internal = plt->internal;
    assert(internal);
    //trace(
        //"polyline_draw: internal->selected_point_index %d\n",
        //internal->selected_point_index
    //);

    for (int j = 0; j < internal->points_num; j++) {
        Color color = internal->selected_point_index == j ?
            internal->handle_selected_color : internal->handle_color;
        DrawCircleV(
            internal->points[j], internal->handle_circle_radius, color
        );
    }

    Vector2 prev_point = internal->points[0];
    for (int j = 0; j < internal->points_num; j++) {
        Vector2 point = internal->points[j];
        DrawLineEx(
            prev_point, point, internal->line_thick, internal->line_color
        );
        prev_point = point;
    }
}


Vector2 *polyline_points_get(struct PolylineTool *plt, int *points_num) {
    assert(plt);
    return NULL;
}

void polyline_points_set(
    struct PolylineTool *plt, Vector2 *points, int points_num
) {
    assert(plt);
    assert(points);
    assert(points_num);

    struct PolylineToolInternal *internal = plt->internal;
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
