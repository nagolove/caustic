#pragma once

#include "raylib.h"

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

