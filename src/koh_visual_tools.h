#pragma once

#include "raylib.h"

typedef struct ToolPolylineInternal ToolPolylineInternal;

struct PolylineTool {
    bool                        exist;
    struct ToolPolylineInternal *internal;
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


typedef struct RibbonFrameInternal RibbonFrameInternal;

// XXX: Добавлять ли другие ограны управления в ribbonframe?
// Переименовать в selectiontool | seltool
struct RibbonFrame {
    bool                        exist;
    Rectangle                   rect;
    struct RibbonFrameInternal  *internal;
};

struct RibbonFrameOpts {
    // Номер кнопки мыши из Raylib для работы с рамкой выделения
    // При -1 сохряняется текущее значение в RibbonFrame
    int     mouse_button_bind;
    // Размер сетки для прилипания, >= 1
    int     snap_size;
    bool    snap;
    Color   line_color, handle_color;
    float   line_thick;
};

void ribbonframe_init(
    struct RibbonFrame *rf, const struct RibbonFrameOpts *opts
);
void ribbonframe_update_opts(
    struct RibbonFrame *rf, const struct RibbonFrameOpts *new_opts
);
void ribbonframe_shutdown(struct RibbonFrame *rf);
void ribbonframe_update(struct RibbonFrame *rf, const Camera2D *cam);

struct RibbonFrameDrawOpts {
    bool draw_axises;
};

void ribbonframe_draw(
    struct RibbonFrame *rf, struct RibbonFrameDrawOpts *opts
);

