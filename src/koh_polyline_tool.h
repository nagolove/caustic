#pragma once

#include "raylib.h"

typedef struct PolylineToolInternal PolylineToolInternal;

struct PolylineTool {
    bool                        exist;
    struct PolylineToolInternal *internal;
};

struct PolylineToolOpts {
    // Номер кнопки мыши из Raylib для работы с рамкой выделения
    // При -1 сохряняется текущее значение в PolylineTool
    int     mouse_button_bind;
    // Размер сетки для прилипания, >= 1
    //int     snap_size;
    //bool    snap;
    //Color   line_color, handle_color;
    //float   line_thick;
};

void polyline_init(
    struct PolylineTool *plt, const struct PolylineToolOpts *opts
);
void polyline_update_opts(
    struct PolylineTool *plt, const struct PolylineToolOpts *new_opts
);
void polyline_shutdown(struct PolylineTool *plt);
void polyline_update(struct PolylineTool *plt, const Camera2D *cam);

struct PolylineToolDrawOpts {
    bool draw_axises;
};

// TODO: Рисование перекрестия и центра прямоугольника
void polyline_draw(
    struct PolylineTool *plt, struct PolylineToolDrawOpts *opts
);


