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
    cp->move_ease = EaseSineOut;
    cp->zoom_time_total = 0.5f;

    cp->zoom_elapsed = cp->zoom_time_total;
    cp->zoom_start = cp->cam->zoom;
    cp->zoom_target = cp->cam->zoom;

    cp->is_dragging = false;
    cp->drag_velocity = Vector2Zero();
    cp->move_start = cp->cam->offset;
    cp->move_target = cp->cam->offset;
    tmr_null(&cp->glide_tmr);
    cp->glide_tmr.period = 0.005f;
    cp->glide_tmr.time_loop = CAMP_GLIDE_DURATION;
    cp->glide_tmr.once = true;
    tmr_init(&cp->glide_tmr);
    // Таймер сразу_EXPIRED, чтобы не было ложного glide
    cp->glide_tmr.expired = true;

    cp->move_elapsed = cp->move_time_total = 0.5f;
    cp->offset_start = cp->cam->offset;
    cp->offset_target = cp->cam->offset;
}

void camp_shutdown(CameraProcessor *cp) {
}

// TODO: Сделать обработку мыши отлючаемой
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

        cp->zoom_start = cam->zoom;
        cp->zoom_target = Clamp(cam->zoom + d, zoom_min, zoom_max);
        cp->zoom_elapsed = 0;

        Vector2 mouse_pos = GetMousePosition();
        Vector2 before = Vector2Subtract(mouse_pos, cam->offset);
        float new_zoom = cp->zoom_target;
        Vector2 after = Vector2Scale(before, new_zoom / cam->zoom);
        Vector2 delta = Vector2Subtract(after, before);

        cp->offset_start = cam->offset;
        cp->offset_target = Vector2Subtract(cam->offset, delta);
        cp->move_elapsed = 0;
    }

    // Обработка перемещения мышью: прямой drag + накопление скорости
    if (IsMouseButtonDown(cp->mouse_btn_move)) {
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1.0f / cam->zoom);
        cam->offset = Vector2Add(cam->offset, delta);
        cp->drag_velocity = delta;
        cp->is_dragging = true;
    }

    // Отпускание мыши — запуск glide-анимации
    else if (cp->is_dragging) {
        cp->is_dragging = false;
        cp->move_start = cam->offset;
        cp->move_target = Vector2Add(
            cam->offset,
            Vector2Scale(cp->drag_velocity, CAMP_INERTIA_FACTOR)
        );
        tmr_null(&cp->glide_tmr);
        cp->glide_tmr.period = 0.005f;
        cp->glide_tmr.time_loop = CAMP_GLIDE_DURATION;
        cp->glide_tmr.once = true;
        cp->glide_tmr.use_period_range = false;
        tmr_init(&cp->glide_tmr);
    }

    // Glide-анимация через Tmr
    if (!cp->is_dragging && !cp->glide_tmr.expired) {
        tmr_begin(&cp->glide_tmr);
        float t = cp->glide_tmr.time_amount;
        float dx = cp->move_target.x - cp->move_start.x;
        float dy = cp->move_target.y - cp->move_start.y;
        cam->offset.x = cp->move_ease(t, cp->move_start.x, dx, 1.f);
        cam->offset.y = cp->move_ease(t, cp->move_start.y, dy, 1.f);
    }

    // Применение easing для zoom
    if (cp->zoom_elapsed < cp->zoom_time_total) {
        cp->zoom_elapsed += dt;
        float t = fminf(cp->zoom_elapsed, cp->zoom_time_total);
        cam->zoom = cp->zoom_ease(
            t, cp->zoom_start,
            cp->zoom_target - cp->zoom_start,
            cp->zoom_time_total);
        if (cp->zoom_elapsed >= cp->zoom_time_total)
            cam->zoom = cp->zoom_target;
    }

    // Easing для offset при zoom
    if (cp->move_elapsed < cp->move_time_total) {
        cp->move_elapsed += dt;
        float t = fminf(cp->move_elapsed, cp->move_time_total);
        float dx = cp->offset_target.x - cp->offset_start.x;
        float dy = cp->offset_target.y - cp->offset_start.y;
        cam->offset.x = cp->move_ease(t, cp->offset_start.x, dx, cp->move_time_total);
        cam->offset.y = cp->move_ease(t, cp->offset_start.y, dy, cp->move_time_total);
        if (cp->move_elapsed >= cp->move_time_total)
            cam->offset = cp->offset_target;
    }
}

void camp_reset(CameraProcessor *cp) {
    assert(cp);
    assert(cp->cam);
    cp->cam->zoom = 1.f;
    cp->cam->offset = Vector2Zero();
}
