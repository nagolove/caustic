// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_bzr_curve.h"

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

typedef struct Stage_BzrCurve {
    Stage         parent;
    HotkeyStorage *hk_store;
    Vector2       guides[4];
    Vector2       *points;
    int           pointsnum;
    int           drag_index;
    Font          fnt;
} Stage_BzrCurve;

void stage_bzr_curve_init(Stage_BzrCurve *st, void *data);
void stage_bzr_curve_update(Stage_BzrCurve *st);
void stage_bzr_curve_shutdown(Stage_BzrCurve *st);

Stage *stage_bzr_curve_new(HotkeyStorage *hk_store) {
    Stage_BzrCurve *st = calloc(1, sizeof(Stage_BzrCurve));
    st->hk_store = hk_store;
    st->parent.init = (Stage_data_callback)stage_bzr_curve_init;
    st->parent.update = (Stage_callback)stage_bzr_curve_update;
    st->parent.shutdown = (Stage_callback)stage_bzr_curve_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_bzr_curve_enter;*/
    return (Stage*)st;
}

static const float CIRCLE_RADIUS = 40;

void stage_bzr_curve_init(Stage_BzrCurve *st, void *data) {
    printf("stage_bzr_curve_init\n");
    st->drag_index = -1;

    st->guides[0] = (Vector2) { 20, 20 };
    st->guides[1] = (Vector2) { 20, 120 };
    st->guides[2] = (Vector2) { 1000, 800 };
    st->guides[3] = (Vector2) { 1200, 700 };

    st->pointsnum = 60;
    st->points = calloc(st->pointsnum, sizeof(st->points[0]));

    st->fnt = load_font_unicode("assets/dejavusansmono.ttf", 30);
}

static void guides_draw(Stage_BzrCurve *st) {
    Color color = BLUE;
    Color color_active = RED;
    for (int j = 0; j < sizeof(st->guides) / sizeof(st->guides[0]); j++) {
        if (st->drag_index == j)
            DrawCircleV(st->guides[j], CIRCLE_RADIUS, color_active);
        else
            DrawCircleV(st->guides[j], CIRCLE_RADIUS, color);

        char buf[20] = {0};
        snprintf(buf, sizeof(buf), "%d", j);
        DrawTextEx(st->fnt, buf, st->guides[j], st->fnt.baseSize, 0, BLACK);
    }
}

void guides_drag(Stage_BzrCurve *st) {
    /*if (st->drag_index != -1)*/
    //printf("down %d\n", IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    //printf("pressed %d\n", IsMouseButtonPressed(MOUSE_LEFT_BUTTON));

    Vector2 mp = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        for (int j = 0; j < sizeof(st->guides) / sizeof(st->guides[0]); j++) {
            if (CheckCollisionPointCircle(mp, st->guides[j], CIRCLE_RADIUS)) {
                st->drag_index = j;
            }
        }

        if (st->drag_index != -1) {
            st->guides[st->drag_index] = mp;
        }

    } else {
        st->drag_index = -1;
    }
}

void curve_update(Stage_BzrCurve *st) {
    float t = 0.;
    float dt = 1. / st->pointsnum;
    for (int e = 0; e < st->pointsnum; ++e) {
        st->points[e] =  bzr_cubic(st->guides, t);
        t += dt;
    }
}

void curve_draw(Stage_BzrCurve *st) {
    Vector2 prev = st->points[0];
    const float thick = 4.;
    for (int j = 0; j < st->pointsnum; ++j) {
        DrawLineEx(prev, st->points[j], thick, BROWN);
        prev = st->points[j];
    }
}

void stage_bzr_curve_update(Stage_BzrCurve *st) {
    //printf("stage_bzr_curve_update\n");
    guides_drag(st);
    guides_draw(st);

    curve_update(st);
    curve_draw(st);
}

void stage_bzr_curve_shutdown(Stage_BzrCurve *st) {
    printf("stage_bzr_curve_shutdown\n");
    if (st->points) {
        free(st->points);
        st->points = NULL;
    }
    UnloadFont(st->fnt);
}

void stage_bzr_curve_enter(Stage_BzrCurve *st, void *data) {
    printf("stage_bzr_curve_enter\n");
}


