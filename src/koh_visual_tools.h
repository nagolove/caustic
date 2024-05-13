#pragma once

#include "raylib.h"

struct ToolPolyline {
    void    *internal;
    bool    exist;
};

struct ToolRectangleAligned {
    void        *internal;
    bool        exist;
    Rectangle   rect;
};

struct ToolRectangle {
    void        *internal;
    bool        exist;
    float       angle, radius;
    Vector2     center,
                // Вершины для режима VIS_TOOL_RECTANGLE_ORIENTED, по часовой стрелке
                // Последняя вершина дублирует первую.
                points[5];
};

struct ToolSector {
    void    *internal;
    bool    exist;
    float   radius, angle1, angle2;
    Vector2 position;
};

struct ToolCommonOpts {
    // Номер кнопки мыши из Raylib для работы с рамкой выделения
    // При -1 сохряняется текущее значение в Tool****
    int     mouse_button_bind;
    Color   line_color, handle_color, handle_color_alternative,
            handle_color_selected;
    float   line_thick;
    bool    snap;
    int     snap_size;
    float   handle_circle_radius;
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

struct ToolRectangleAlignedDrawOpts {
    bool draw_axises;
};

struct ToolRectangleAlignedOpts {
    struct ToolCommonOpts   common;
    int                     snap_size;  // Размер сетки для прилипания, >= 1
    bool                    snap;       // Включить прилипание сетки
};

struct ToolRectangleOpts {
    struct ToolCommonOpts   common;
};

struct ToolSectorOpts {
    struct ToolCommonOpts   common;
};

enum VisualToolMode {
    VIS_TOOL_RECTANGLE,
    VIS_TOOL_RECTANGLE_ORIENTED,
    VIS_TOOL_POLYLINE,
    VIS_TOOL_SECTOR,
};

struct VisualTool {
    enum VisualToolMode             mode;

    struct ToolRectangle            t_rect;
    struct ToolRectangleOpts        t_rect_opts;
    struct ToolRectangleDrawOpts    t_rect_draw_opts;

    struct ToolRectangleAligned            t_recta;
    struct ToolRectangleAlignedOpts        t_recta_opts;
    struct ToolRectangleAlignedDrawOpts    t_recta_draw_opts;

    struct ToolSector               t_sector;
    struct ToolSectorOpts           t_sector_opts;
    struct ToolSectorDrawOpts       t_sector_draw_opts;

    struct ToolPolyline             t_pl;
    struct ToolPolylineDrawOpts     t_pl_draw_opts;
    struct ToolPolylineOpts         t_pl_opts;
};

/*
void visual_tool_init(
    struct VisualTool *vt,
    struct ToolRectangleOpts *t_rect_opts,
    struct ToolRectangleOpts *t_rect_oriented_opts,
    struct ToolSectorOpts *t_sector_opts,
    struct ToolPolylineDrawOpts *t_pl_draw_opts
);
*/
void visual_tool_shutdown(struct VisualTool *vt);
void visual_tool_update(struct VisualTool *vt, const Camera2D *cam);
void visual_tool_reset_all(struct VisualTool *vt);
void visual_tool_draw(struct VisualTool *vt, const Camera2D *cam);

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

void rectanglea_init(
    struct ToolRectangleAligned *rf,
    const struct ToolRectangleAlignedOpts *opts
);
void rectanglea_update_opts(
    struct ToolRectangleAligned *rf,
    const struct ToolRectangleAlignedOpts *new_opts
);
void rectanglea_shutdown(struct ToolRectangleAligned *rf);
void rectanglea_update(struct ToolRectangleAligned *rf, const Camera2D *cam);
void rectanglea_draw(
    struct ToolRectangleAligned *rf,
    const struct ToolRectangleAlignedDrawOpts *opts
);

void rectangle_init(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *opts
);
void rectangle_update_opts(
    struct ToolRectangle *rf, const struct ToolRectangleOpts *new_opts
);
void rectangle_shutdown(struct ToolRectangle *rf);
void rectangle_update(struct ToolRectangle *rf, const Camera2D *cam);
void rectangle_draw(
    struct ToolRectangle *rf, const struct ToolRectangleDrawOpts *opts
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


extern struct ToolCommonOpts visual_tool_commont_default_opts;
