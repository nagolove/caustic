#include "koh_camera.h"

#include <assert.h>
#include "raymath.h"

void camp_init(CameraProcessor *cp, CameraProcessorOpts opts) {
    assert(cp);
    assert(opts.cam);

    cp->cam = opts.cam;
    cp->mod_key_down_scale = opts.mod_key_down_scale;
    cp->mouse_btn_move = opts.mouse_btn_move;
    cp->dscale_value = 0.1;

    cp->zoom_ease = EaseCubicInOut;
    cp->move_ease = EaseSineInOut;
    cp->zoom_time_total = 0.5f;
    cp->move_time_total = 0.5f;

    cp->zoom_elapsed = cp->zoom_time_total;
    cp->move_elapsed = cp->move_time_total;

    cp->zoom_start = cp->cam->zoom;
    cp->zoom_target = cp->cam->zoom;
    cp->offset_start = cp->cam->offset;
    cp->offset_target = cp->cam->offset;
}

void camp_shutdown(CameraProcessor *cp) {
}

/*
void camp_update(CameraProcessor *cp) {
    const float zoom_min = 0.01f;
    const float zoom_max = 100.0f;

    assert(cp);
    assert(cp->cam);

    float mouse_wheel = GetMouseWheelMove();
    Camera2D *cam = cp->cam;
    bool modpressed = IsKeyDown(cp->mod_key_down_scale);
    bool wheel_in_eps = mouse_wheel > EPSILON || mouse_wheel < -EPSILON;
    float dscale_value = cp->dscale_value;

    if (!isfinite(cam->zoom)) {
        cam->zoom = zoom_min;
    }

    if (cam && modpressed && wheel_in_eps) {
        const float d = copysignf(dscale_value, mouse_wheel);

        cam->zoom = fminf(fmaxf(cam->zoom + d, zoom_min), zoom_max);
        //cam->zoom = cam->zoom + d;
        //
        if (cam->zoom > EPSILON) {
            Vector2 delta = Vector2Scale(GetMouseDelta(), -1. / cam->zoom);
            //cam->target = Vector2Add(cam->target, delta);
            cam->offset = Vector2Add(cam->offset, Vector2Negate(delta));
        }
    }

    if (cp->cam && IsMouseButtonDown(cp->mouse_btn_move)) {
        float inv_zoom = cp->cam->zoom;
        float dzoom = inv_zoom == 1. ? 
            -(1. / cp->cam->zoom) : 
            -log(1. / cp->cam->zoom);
        Vector2 delta = Vector2Scale(GetMouseDelta(), dzoom);
        cp->cam->offset = Vector2Add(cp->cam->offset, Vector2Negate(delta));
    }
}
*/

void camp_update(CameraProcessor *cp) {
    const float zoom_min = 0.01f;
    const float zoom_max = 100.0f;

    assert(cp);
    assert(cp->cam);

    Camera2D *cam = cp->cam;
    float mouse_wheel = GetMouseWheelMove();
    bool modpressed = IsKeyDown(cp->mod_key_down_scale);
    bool wheel_in_eps = mouse_wheel > EPSILON || mouse_wheel < -EPSILON;
    float dscale_value = cp->dscale_value;
    float dt = GetFrameTime();

    // Масштабирование с easing вокруг мыши
    if (modpressed && wheel_in_eps) {
        float d = copysignf(dscale_value, mouse_wheel);

        // Сохраняем текущее состояние
        cp->zoom_start = cam->zoom;
        cp->zoom_target = Clamp(cam->zoom + d, zoom_min, zoom_max);
        cp->zoom_elapsed = 0;

        // Масштабирование вокруг мыши
        Vector2 mouse_pos = GetMousePosition();
        Vector2 before = Vector2Subtract(mouse_pos, cam->offset);

        // Прогнозируем offset после зума
        float new_zoom = cp->zoom_target;
        Vector2 after = Vector2Scale(before, new_zoom / cam->zoom);
        Vector2 delta = Vector2Subtract(after, before);

        cp->offset_start = cam->offset;
        cp->offset_target = Vector2Subtract(cam->offset, delta);
        cp->move_elapsed = 0;
    }

    // Обработка перемещения мышью
    if (IsMouseButtonDown(cp->mouse_btn_move)) {
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1.0f / cam->zoom);

        cp->offset_start = cam->offset;
        cp->offset_target = Vector2Add(cam->offset, delta);
        cp->move_elapsed = 0;
    }

    // Применение easing для zoom
    if (cp->zoom_elapsed < cp->zoom_time_total) {
        cp->zoom_elapsed += dt;
        float t = fminf(cp->zoom_elapsed, cp->zoom_time_total);
        cam->zoom = cp->zoom_ease(t, cp->zoom_start, cp->zoom_target - cp->zoom_start, cp->zoom_time_total);
    } else {
        cam->zoom = cp->zoom_target;
    }

    // Применение easing для offset
    if (cp->move_elapsed < cp->move_time_total) {
        cp->move_elapsed += dt;
        float t = fminf(cp->move_elapsed, cp->move_time_total);
        float dx = cp->offset_target.x - cp->offset_start.x;
        float dy = cp->offset_target.y - cp->offset_start.y;
        cam->offset.x = cp->move_ease(t, cp->offset_start.x, dx, cp->move_time_total);
        cam->offset.y = cp->move_ease(t, cp->offset_start.y, dy, cp->move_time_total);
    } else {
        cam->offset = cp->offset_target;
    }
}
