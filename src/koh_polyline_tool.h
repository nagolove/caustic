#pragma once

#include "raylib.h"

typedef struct PolylineToolInternal PolylineToolInternal;

struct PolylineTool {
    bool                        exist;
    struct PolylineToolInternal *internal;
};

struct PolylineToolOpts {
    // Номер кнопки мыши из Raylib для работы с точками линиии
    // При -1 сохряняется текущее значение в PolylineTool
    int     mouse_button_bind;
    // Размер сетки для прилипания, >= 1
    //int     snap_size;
    //bool    snap;
    Color   line_color, handle_color, handle_selected_color;
    float   line_thick;
};

struct PolylineToolDrawOpts {
    //bool draw_axises;
};

void polyline_init(
    struct PolylineTool *plt, const struct PolylineToolOpts *opts
);
void polyline_update_opts(
    struct PolylineTool *plt, const struct PolylineToolOpts *new_opts
);
void polyline_shutdown(struct PolylineTool *plt);
void polyline_update(struct PolylineTool *plt, const Camera2D *cam);

void polyline_draw(
    struct PolylineTool *plt, struct PolylineToolDrawOpts *opts
);

Vector2 *polyline_points_get(struct PolylineTool *plt, int *points_num);
void polyline_points_set(
    struct PolylineTool *plt, Vector2 *points, int points_num
);
