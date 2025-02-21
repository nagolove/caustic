#include "koh_fpsmeter.h"

#include "koh_logger.h"
#include "koh_lua.h"
#include "raylib.h"
#include "raylib.h"
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct Meter {
    int fps;
    bool has_mark;
};

struct Context {
    struct Meter *meters;
    int i, cap, len;
};

static struct Context ctx = {0};
static const float dx = 3;
static const Color color_grid = BLACK;
static const Color color_graph1 = ORANGE;
static const Color color_graph2 = RED;
static const float font_sz = 17;
static bool is_drawing = false;

bool koh_fpsmeter_stat_get() {
    return is_drawing;
}

void koh_fpsmeter_stat_set(bool state) {
    is_drawing = state;
}

static int l_fpsmeter_stat(lua_State *lua) {
    /*koh_fpsmeter_draw((Vector2){ 0, 0 });*/
    trace("l_fpsmeter_stat:\n");
    is_drawing = !is_drawing;
    return 0;
}

static void fpsmeter_register() {
    if (!sc_get_state()) {
        trace("fpsmeter_register: there is no Lua state\n");
        return;
    }

    sc_register_function(
        l_fpsmeter_stat,
        "fpsmeter_stat",
        "Показать статистику по кадрам"
    );
}

void koh_fpsmeter_init() {
    ctx.cap = GetScreenWidth() / dx - dx * 4; // Чуть меньше ширины экрана
    ctx.len = 0;
    ctx.i = 0;
    ctx.meters = calloc(ctx.cap, sizeof(struct Meter));

    fpsmeter_register();
}

void koh_fpsmeter_shutdown() {
    if (ctx.meters) {
        free(ctx.meters);
        ctx.meters = NULL;
    }
    memset(&ctx, 0, sizeof(ctx));
}

static void push(struct Meter m) {
    assert(ctx.meters);
    ctx.meters[ctx.i] = m;
    ctx.i = (ctx.i + 1) % ctx.cap;
    ctx.len++;
    if (ctx.len >= ctx.cap)
        ctx.len = ctx.cap;
}

static bool in_frame = false;
static bool has_mark = false;

void koh_fpsmeter_frame_begin() {
    in_frame = true;
    has_mark = false;
}

void koh_fpsmeter_mark() {
    if (!is_drawing) return;

    if (!in_frame) {
        trace("koh_fpsmeter_mark: no in begin/end bounds\n");
        exit(EXIT_FAILURE);
    }
    has_mark = true;
}

void koh_fpsmeter_frame_end() {
    push((struct Meter) {
        .fps = GetFPS(),
        .has_mark = has_mark,
    });
    in_frame = false;
}

void draw_grid(Vector2 pos, float scale) {
    char txt_buf[128] = {0};
    DrawLineEx(
            (Vector2) {0, pos.y},
            (Vector2){ GetScreenWidth(), pos.y},
            4.,
            color_grid
    );
    DrawTextEx(
        GetFontDefault(), "0 frames", (Vector2) { 0, pos.y}, 
        font_sz, 0., color_grid
    );

    int d_fps = 50;
    //int d_fps = 50;
    int y = pos.y;
    while (y < GetScreenHeight()) {
        DrawLineEx(
                (Vector2) { 0, y}, (Vector2) { GetScreenWidth(), y},
                1., color_grid
        );
        sprintf(txt_buf, "%d", (int)(pos.y - y));
        DrawTextEx(
            GetFontDefault(), txt_buf, (Vector2) { 0, y}, 
            font_sz, 0., color_grid
        );
        y += d_fps;
    }

    y = pos.y;
    while (y > 0) {
        DrawLineEx(
                (Vector2) { 0, y}, (Vector2) { GetScreenWidth(), y},
                1., color_grid
        );
        sprintf(txt_buf, "%d", (int)(pos.y - y));
        DrawTextEx(
            GetFontDefault(), txt_buf, (Vector2) { 0, y}, 
            font_sz, 0., color_grid
        );
        y -= d_fps;
    }

}

static int find_max() {
    int max = 0;
    for (int k = ctx.i; k < ctx.len; k++) {
        if (ctx.meters[k].fps > max)
            max = ctx.meters[k].fps;
    }
    for (int k = 0; k < ctx.len; k++) {
        if (ctx.meters[k].fps > max)
            max = ctx.meters[k].fps;
    }
    return max;
}

static inline void draw_segment(
    Vector2 *pos, Color *color, int *prev_fps, float linethick, 
    const int *k, float scale
) {
    struct Meter *meter = &ctx.meters[*k];
    int cur_fps = meter->fps;
    float newx = pos->x + dx;

    DrawLineEx(
            (Vector2) { pos->x, pos->y - *prev_fps * scale},
            (Vector2) { newx, pos->y - cur_fps * scale},
            linethick, *color
    );

    if (meter->has_mark) {
        DrawLineEx(
                (Vector2) { pos->x, pos->y - *prev_fps * scale},
                (Vector2) { newx,   pos->y - cur_fps * scale},
                linethick * 5, BLACK
        );
    }

    *prev_fps = cur_fps;
    pos->x = newx;

    *color = (*k % 2 == 0) ? color_graph1 : color_graph2;
}

void koh_fpsmeter_draw() {
    if (!is_drawing) return;
    Vector2 pos = { 0, GetScreenHeight() / 2. };

    int max = find_max();
    float scale = pos.y / (max + 1.);
    draw_grid(pos, scale);

    Color color = color_graph1;
    int prev_fps = ctx.meters[ctx.i].fps;
    const float linethick = 4.;
    for (int k = ctx.i; k < ctx.len; k++) {
        draw_segment(&pos, &color, &prev_fps, linethick, &k, scale);
    }
    for (int k = 0; k < ctx.i; k++) {
        draw_segment(&pos, &color, &prev_fps, linethick, &k, scale);
    }
}

