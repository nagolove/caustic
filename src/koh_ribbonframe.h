#pragma once

#include "raylib.h"

typedef struct RibbonFrameInternal RibbonFrameInternal;

struct RibbonFrame {
    bool                        exist;
    Rectangle                   rect;
    struct RibbonFrameInternal  *internal;
};

struct RibbonFrameOpts {
    // Номер кнопки мыши из Raylib для работы с рамкой выделения
    int mouse_button_bind;
};

void ribbonframe_init(
    struct RibbonFrame *rf, const struct RibbonFrameOpts *opts
);
void ribbonframe_shutdown(struct RibbonFrame *rf);
void ribbonframe_update(struct RibbonFrame *rf, const Camera2D *cam);
void ribbonframe_draw(struct RibbonFrame *rf);
