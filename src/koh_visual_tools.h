#pragma once

#include "raylib.h"

typedef struct ToolPolylineInternal ToolPolylineInternal;
typedef struct ToolRectangleInternal ToolRectangleInternal;
typedef struct ToolSectorInternal ToolSectorInternal;

struct ToolPolyline {
    struct ToolPolylineInternal *internal;
    bool                        exist;
};

// TODO: Переименовать в selectiontool | seltool
struct ToolRectangle {
    struct ToolRectangleInternal  *internal;
    bool                        exist;
    Rectangle                   rect;
};

struct ToolSector {
    struct ToolSectorInternal   *internal;
    bool                        exist;
    float                       radius, angle1, angle2;
    Vector2                     position;
};

struct ToolCommonOpts {
    // Номер кнопки мыши из Raylib для работы с рамкой выделения
    // При -1 сохряняется текущее значение в Tool****
    int     mouse_button_bind;
    Color   line_color, handle_color, handle_color_selected;
    float   line_thick;
    bool    snap;
    int     snap_size;
};

struct ToolPolylineOpts {
    struct ToolCommonOpts   common;
    // TODO: Добавить прилипание к сетке
    //bool                    snap;       // Включить прилипание сетки
};

struct ToolSectorDrawOpts {
};

struct ToolPolylineDrawOpts {
    //bool draw_axises;
};

struct ToolRectangleDrawOpts {
    bool draw_axises;
};

struct ToolRectangleOpts {
    struct ToolCommonOpts   common;
    int                     snap_size;  // Размер сетки для прилипания, >= 1
    bool                    snap;       // Включить прилипание сетки
    bool                    is_oriented;// Включить поворот прямоугольника
};

struct ToolSectorOpts {
    struct ToolCommonOpts   common;
};

void polyline_init(
    struct ToolPolyline *plt, const struct ToolPolylineOpts *opts
);
void polyline_update_opts(
    struct ToolPolyline *plt, const struct ToolPolylineOpts *new_opts
);
void polyline_shutdown(struct ToolPolyline *plt);
void polyline_update(struct ToolPolyline *plt, const Camera2D *cam);

void polyline_draw(
    struct ToolPolyline *plt, struct ToolPolylineDrawOpts *opts,
    const Camera2D *cam
);

// XXX: Кто ответственнен за возвращенную память?
Vector2 *polyline_points_get(struct ToolPolyline *plt, int *points_num);
void polyline_points_set(
    struct ToolPolyline *plt, Vector2 *points, int points_num
);

void ribbonframe_init(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *opts
);
void ribbonframe_update_opts(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *new_opts
);
void ribbonframe_shutdown(struct ToolRectangle *rf);
void ribbonframe_update(struct ToolRectangle *rf, const Camera2D *cam);

void ribbonframe_draw(
    struct ToolRectangle *rf, struct ToolRectangleDrawOpts *opts
);

void sector_init(
    struct ToolSector *sec, const struct ToolSectorOpts *opts
);
void sector_update_opts(
    struct ToolSector *sec, const struct ToolSectorOpts *new_opts
);
void sector_shutdown(struct ToolSector *sec);
void sector_update(struct ToolSector *sec, const Camera2D *cam);

void sector_draw(
    struct ToolSector *sec, struct ToolSectorDrawOpts *opts,
    const Camera2D *cam
);

