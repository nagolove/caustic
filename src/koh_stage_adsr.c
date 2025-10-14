// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_adsr.h"

#include "koh_adsr.h"
#include "koh_common.h"
#include "koh_hotkey.h"
#include "koh_stages.h"

#include "raylib.h"
#include "raymath.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

static Camera2D cam = { 0 };
static const float LINE_THICK = 3.;
static const float CIRCLE_RADIUS = 40;
//static int height = 0;
static Font fnt;

Stage *stage_adsr_new() {
    Stage_ADSR *st = calloc(1, sizeof(Stage_ADSR));
    if (!st) {
        printf("stage_adsr_new: allocation failed\n");
        koh_fatal();
    } else {
        st->parent.init = (Stage_callback)stage_adsr_init;
        st->parent.update = (Stage_callback)stage_adsr_update;
        st->parent.shutdown = (Stage_callback)stage_adsr_shutdown;
    }
    return (Stage*)st;
}

static bool is_guide_none(struct DragIndex di) {
    return di.guide == -1 && di.circle == -1;
}

static struct DragIndex guide_none() {
    return (struct DragIndex) { -1, -1 };
}

static void guides_drag(Stage_ADSR *st) {
    /*if (st->drag_index != -1)*/
    //printf("down %d\n", IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    //printf("pressed %d\n", IsMouseButtonPressed(MOUSE_LEFT_BUTTON));

    struct DragIndex *drag_index = &st->drag_index;
    Vector2 mp = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < st->envelopes_num; i++) {
            for (int j = 0; j < GUIDES_NUM; j++) {
                bool ok = CheckCollisionPointCircle(
                    mp,
                    st->guides[i].circles[j], 
                    CIRCLE_RADIUS
                );
                if (ok) {
                    *drag_index = (struct DragIndex) { i, j };
                }
            }
        }

        if (!is_guide_none(*drag_index)) {
            st->guides[drag_index->guide].circles[drag_index->circle] = mp;
        }

    } else {
        st->drag_index = guide_none();
    }
}

static void view_init(struct ADSR_view *view) {
    assert(view);
    view->columns_cap = 2048;
    view->columns_num = 0;
    view->colums = calloc(sizeof(view->colums[0]), view->columns_cap);
}

static void view_shutdown(struct ADSR_view *view) {
    assert(view);
    if (view->colums) {
        free(view->colums);
        view->colums = NULL;
    }
}

static void view_clear(struct ADSR_view *view) {
    assert(view);
    view->columns_num = 0;
}

static void view_push(struct ADSR_view *view, double value, Color color) {
    assert(view);
    if (view->columns_num + 1 < view->columns_cap) {
        view->colums[view->columns_num++] = (struct Column) {
            .color = color, 
            .value = value
        };
    }
}

static void view_draw(struct ADSR_view *view, struct ADSR *env) {
    assert(view);
    Vector2 pos = view->pos;
    pos.y += view->height;
    for (int i = 0; i < view->columns_num; i++) {
        DrawLineEx(
            pos, Vector2Add(pos, (Vector2) { 0., -view->colums[i].value}), 
            LINE_THICK,
            view->colums[i].color
        );
        //DrawLineEx(
        pos.x += LINE_THICK;
    }

    const float thick = 5;
    Rectangle rect = {
        .x = view->pos.x,
        .y = view->pos.y,
        .width = adsr_duration(env) * LINE_THICK / GetFrameTime(),
        //.width = view->columns_num * LINE_THICK,
        .height = view->height,
    };
    printf("adsr_duration(env) %f\n", adsr_duration(env));
    DrawRectangleLinesEx(rect, thick, BLACK);
}

static void adsr_draw_params(const ADSR *adsr, Vector2 pos) {
    char buf[256] = {0};
    sprintf(buf, "atack amplitude %f", adsr->amp_attack);
    DrawTextEx(fnt, buf, pos, fnt.baseSize, 0, BLACK);
    pos.y += fnt.baseSize;
    sprintf(buf, "sustain amplitude %f", adsr->amp_sustain);
    DrawTextEx(fnt, buf, pos, fnt.baseSize, 0, BLACK);
    pos.y += fnt.baseSize;
    sprintf(buf, "attack %f", adsr->attack);
    DrawTextEx(fnt, buf, pos, fnt.baseSize, 0, BLACK);
    pos.y += fnt.baseSize;
    sprintf(buf, "decay %f", adsr->decay);
    DrawTextEx(fnt, buf, pos, fnt.baseSize, 0, BLACK);
    pos.y += fnt.baseSize;
    sprintf(buf, "sustain %f", adsr->sustain);
    DrawTextEx(fnt, buf, pos, fnt.baseSize, 0, BLACK);
    pos.y += fnt.baseSize;
    sprintf(buf, "release %f", adsr->release);
    DrawTextEx(fnt, buf, pos, fnt.baseSize, 0, BLACK);
}

void guides_init(
    Stage_ADSR *st,
    struct ADSR_guides *guide,
    struct ADSR *env,
    struct ADSR_view *view
) {
    assert(guide);
    assert(st);
    assert(env);
    assert(view);
    st->drag_index = guide_none();
    guide->circles[0].x = view->pos.x + env->attack * LINE_THICK;
    guide->circles[0].y = view->pos.y;
    guide->circles[1].x = view->pos.x + (env->attack + env->decay) * LINE_THICK;
    guide->circles[1].y = view->pos.y;
    double tmp = (env->attack + env->decay + env->sustain);
    guide->circles[2].x = view->pos.x + tmp * LINE_THICK;
    guide->circles[2].y = view->pos.y;
}

void views_build(Stage_ADSR *st) {
    //height = GetScreenHeight() / st->envelopes_num;
    int x0 = 0, y0 = 0;
    for (int j = 0; j < st->envelopes_num; ++j) {
        struct ADSR_view *view = &st->views[j];
        view->pos.x = x0;
        view->pos.y = y0;
        view->height = st->envelopes[j].amp_attack + CIRCLE_RADIUS;
        printf("view->height %d\n", view->height);
        y0 += view->height;
    }
}

void push_adsr(
    Stage_ADSR *st,
    double amp_attack, double amp_sustain, 
    double attack, double decay,
    double sustain, double release
) {
    struct ADSR *env = NULL;
    view_init(&st->views[st->envelopes_num]);
    env = &st->envelopes[st->envelopes_num];
    env->amp_attack =amp_attack;
    env->amp_sustain =amp_sustain;
    env->attack =attack;
    env->decay =decay;
    env->sustain =sustain;
    env->release =release;
    guides_init(
        st, &st->guides[st->envelopes_num],
        env, &st->views[st->envelopes_num]
    );
    st->envelopes_num++;
};


void stage_adsr_init(Stage_ADSR *st) {
    printf("stage_adsr_init:\n");

    const char *main_font = "assets/dejavusansmono.ttf";
    const i32 fnt_size = 23;
    if (koh_file_exist(main_font))
        fnt = load_font_unicode(main_font, fnt_size);
    else
        fnt = GetFontDefault();

    cam.zoom = 1.;

    //push_adsr(st, 1., 0.8, 2., 1., 10., 5.);
    //push_adsr(st, 0.5, 0.5, 0.5, 0.5, 1., 0.5);
    push_adsr(st, 320, 220, 3., 1., 4., 2.);
    push_adsr(st, 400, 400, 3., .5, 4., 2.);
    push_adsr(st, 400, 270, 3., 1., 4., 2.);

    views_build(st);

    printf("GetTime() %f\n", GetTime());
}

static void guides_draw(Stage_ADSR *st) {
    assert(st);
    Color color;
    for (int i = 0; i < st->envelopes_num; ++i) {
        for (int j = 0; j < GUIDES_NUM; j++) {
            struct ADSR_view *view = &st->views[i];
            Vector2 circle = Vector2Add(st->guides[i].circles[j], view->pos);
            if (st->drag_index.guide == i &&
                st->drag_index.circle == j)
                color = RED;
            else
                color = BLUE;

            DrawCircleV(circle, CIRCLE_RADIUS, color);
        }
    }
}

void cam_translate() {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        Vector2 delta = GetMouseDelta();
        delta = Vector2Scale(delta, -1.0f / cam.zoom);
        cam.target = Vector2Add(cam.target, delta);
    }
}

void cam_zoom() {
    // zoom based on wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        // get the world point that is under the mouse
        Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), cam);

        // set the offset to where the mouse is
        cam.offset = GetMousePosition();

        // set the target to match, so that the camera maps the world space point under the cursor to the screen space point under the cursor at any zoom
        cam.target = mouseWorldPos;

        // zoom
        cam.zoom += wheel * 0.125f;
        if (cam.zoom < 0.125f)
            cam.zoom = 0.125f;
    }
}

void stage_adsr_update(Stage_ADSR *st) {
    guides_drag(st);

    cam_translate();
    cam_zoom();

    // TODO сделать сброс огибающих
    if (IsKeyPressed(KEY_SPACE)) {
        for (int j = 0; j < st->envelopes_num; ++j) {
            //adsr_restart(&st->envelopes[j]);
            adsr_init(&st->envelopes[j]);
            view_clear(&st->views[j]);
        }
    }

    BeginMode2D(cam);
    for (int j = 0; j < st->envelopes_num; ++j) {
        if (!st->envelopes[j].inited)
            continue;

        struct ADSR_view *view = &st->views[j];
        struct ADSR *adsr = &st->envelopes[j];
        //printf("view->pos %s\n", Vector2_tostr(view->pos));
        double value = 0;
        if (adsr_update(&st->envelopes[j], &value))
            view_push(&st->views[j], value, st->envelopes[j].color);

        view_draw(view, adsr);

        char text_buf[128] = {0};
        sprintf(text_buf, "%d", j);
        Vector2 pos = { .x = view->pos.x + 20, .y = view->pos.y + 20 };
        DrawTextEx(fnt, text_buf, pos, fnt.baseSize, 0, BLACK);
        pos.y += fnt.baseSize;
        adsr_draw_params(&st->envelopes[j], pos);
    }
    guides_draw(st);
    EndMode2D();
}

void stage_adsr_shutdown(Stage_ADSR *st) {
    printf("stage_adsr_shutdown\n");
    UnloadFont(fnt);
    for (int q = 0; q < st->envelopes_num; q++) {
        view_shutdown(&st->views[q]);
    }
}

