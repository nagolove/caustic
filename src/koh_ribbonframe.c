#include "koh_ribbonframe.h"

#include <assert.h>
#include <stdlib.h>
#include "koh_common.h"
#include "koh_logger.h"
#include "raylib.h"

enum State {
    S_NONE,
    S_GRIP_RESIZE,
};

struct RibbonFrameInternal {
    int         mouse_button_bind;
    enum State  state;
    float       line_thick;
    Color       line_color;
    float       handle_circle_radius;
};

void ribbonframe_init(struct RibbonFrame *rf, struct RibbonFrameOpts *opts) {
    assert(rf);
    trace("ribbonframe_init:\n");

    rf->internal = malloc(sizeof(*rf->internal));
    if (opts)
        rf->internal->mouse_button_bind = opts->mouse_button_bind;
    else
        rf->internal->mouse_button_bind = MOUSE_BUTTON_LEFT;

    rf->internal->handle_circle_radius = 15.;
    rf->internal->state = S_NONE;
    rf->internal->line_thick = 3.;
    rf->internal->line_color = YELLOW;
}

void ribbonframe_shutdown(struct RibbonFrame *rf) {
    assert(rf);
    if (rf->internal) {
        free(rf->internal);
        rf->internal = NULL;
    }
}

void ribbonframe_update(struct RibbonFrame *rf, const Camera2D *cam) {
    assert(rf);

    Vector2 mp = GetMousePosition();
    if (IsMouseButtonDown(rf->internal->mouse_button_bind)) {

        if (rf->internal->state == S_NONE) {
            trace("ribbonframe_update: S_NONE\n");
            rf->rect.x = GetMouseX();
            rf->rect.y = GetMouseY();
        }

        trace("ribbonframe_update: S_GRIP_RESIZE\n");
        rf->internal->state = S_GRIP_RESIZE;

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

        trace("ribbonframe_update: rect %s\n", rect2str(rf->rect));
    } else {
        rf->internal->state = S_NONE;
    }

    // TODO: Правильный учет камеры
    if (cam) {
        mp.x += cam->offset.x;
        mp.y += cam->offset.y;
    }

    if (rf->internal->state == S_NONE) {
        //trace("ribbonframe_update: CheckCollisionPointCircle\n");

        Vector2 points[] = {
             { rf->rect.x, rf->rect.y }, 
             { rf->rect.x + rf->rect.width, rf->rect.y + rf->rect.height },
             { rf->rect.x + rf->rect.width, rf->rect.y }, 
             { rf->rect.x, rf->rect.y + rf->rect.height }, 
        };

        int points_num = sizeof(points) / sizeof(points[0]);
        float radius = rf->internal->handle_circle_radius;
        int corner_index = -1;
        for (int i = 0; i < points_num; ++i) {
            if (CheckCollisionPointCircle(mp, points[i], radius)) {
                corner_index = i;
                break;
            }
        }

        if (corner_index != -1) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
        } else {
            SetMouseCursor(0);
        }

    }

}

void ribbonframe_draw(struct RibbonFrame *rf) {
    //trace("ribbonframe_draw:\n");
    assert(rf);

    DrawRectangleLinesEx(
        rf->rect, rf->internal->line_thick, rf->internal->line_color
    );

    /*
    DrawRectangleRec((Rectangle) {
            .x = 30,
            .y = 30,
            .width = 100,
            .height = 100,
        }, line_color
    );
    DrawRectangleLinesEx((Rectangle) {
            .x = 30,
            .y = 30,
            .width = 100,
            .height = 100,
        }, line_thick, line_color
    );
    */
}
