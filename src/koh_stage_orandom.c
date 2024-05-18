// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_orandom.h"

#include "koh_common.h"
#include "koh_rand.h"
#include "koh_routine.h"
#include "raylib.h"
#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *rules = "'R' - перезапустить";

void stage_orandom_init(Stage_OuterRandom *st);
void stage_orandom_update(Stage_OuterRandom *st);
void stage_orandom_shutdown(Stage_OuterRandom *st);

Stage *stage_orandom_new(void) {
    Stage_OuterRandom *st = calloc(1, sizeof(Stage_OuterRandom));
    st->parent.init = (Stage_callback)stage_orandom_init;
    st->parent.update = (Stage_callback)stage_orandom_update;
    st->parent.shutdown = (Stage_callback)stage_orandom_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_orandom_enter;*/
    return (Stage*)st;
}

void stage_orandom_init(Stage_OuterRandom *st) {
    printf("stage_orandom_init\n");
    st->fnt = load_font_unicode("assets/dejavusansmono.ttf", 40);
    st->rng = xorshift32_init();

    float w = xorshift32_rand1(&st->rng) * 200. + 200,
        h = xorshift32_rand1(&st->rng) * 200. + 200;
    Vector2 start = {
        .x = xorshift32_rand1(&st->rng) * 20 + 60,
        .y = xorshift32_rand1(&st->rng)  + 60,
    };
    int border = xorshift32_rand1(&st->rng) * 100 + 100;

    printf(
        "start %s, border %d, w %f, h %f\n", Vector2_tostr(start), border, w, h
    );
    
    st->rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    int num = 20000;
    //BeginDrawing();
    BeginTextureMode(st->rt);
    Camera2D cam = {
        .offset.y = -GetScreenHeight() / 3.,
        .offset.x = GetScreenWidth() / 2.,
    };
    cam.zoom = 1.;
    BeginMode2D(cam);
    ClearBackground(GREEN);

    for (int k = 0; k < num; k++) {
        Vector2 point = random_outrect_quad(&st->rng, start, w, border);
        /*
        Vector2 point = random_inrect(&st->rng, (Rectangle) {
            .x = 100,
            .y = 100,
            .width = 300,
            .height = 600,
        });
        */
        float radius = 2.;
        //printf("point %s\n", Vector2_tostr(point));
        point.y = GetScreenHeight() -point.y;
        DrawCircleV(point, radius, MAROON);
    }
    DrawCircle(0, 0, 30, RED);

    EndMode2D();
    EndTextureMode();
    //EndDrawing();
}

void stage_orandom_update(Stage_OuterRandom *st) {
    DrawTexture(st->rt.texture, 0, 0, WHITE);

    if (IsKeyPressed(KEY_R)) {
        stage_orandom_shutdown(st);
        stage_orandom_init(st);
    }

    DrawCircle(0, 0, 20, BLACK);
    DrawTextEx(
        st->fnt,
        rules,
        (Vector2) { 0., 50. },
        st->fnt.baseSize,
        0., YELLOW
    );
}

void stage_orandom_shutdown(Stage_OuterRandom *st) {
    printf("stage_orandom_shutdown\n");
    UnloadFont(st->fnt);
    UnloadRenderTexture(st->rt);
}

void stage_orandom_enter(Stage_OuterRandom *st, void *data) {
    printf("stage_orandom_enter\n");
}


