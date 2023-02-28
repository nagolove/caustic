#include "stage_rand.h"
#include "rand.h"
#include "raylib.h"
#include "stages.h"

#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct Stage_rand {
    Stage parent;
    RenderTexture2D tex_rand, tex_xorshift;
} Stage_rand;

void stage_rand_init(Stage_rand *st, void *data);
void stage_rand_update(Stage_rand *st);
void stage_rand_shutdown(Stage_rand *st);

const int size = 4096 * 3;

Stage *stage_rand_new(void) {
    Stage_rand *st = calloc(1, sizeof(*st));
    st->parent.init = (Stage_data_callback)stage_rand_init;
    st->parent.update = (Stage_callback)stage_rand_update;
    st->parent.shutdown = (Stage_callback)stage_rand_shutdown;
    return (Stage*)st;
}

void stage_rand_init(Stage_rand *st, void *data) {
    st->tex_rand = LoadRenderTexture(size, size);
    st->tex_xorshift = LoadRenderTexture(size, size);

    clock_t start, end;
    double elapsed;

    srand(time(NULL));
    BeginDrawing();
    BeginTextureMode(st->tex_rand);
    ClearBackground(BLANK);
    start = clock();
    for(uint32_t i = 0; i < size * size; ++i) {
        int x = rand() % size;
        int y = rand() % size;
        DrawPixel(x, y, RED);
    }
    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("time for rand() - %f\n", elapsed);
    EndTextureMode();
    EndDrawing();

    xorshift32_state rstate = xorshift32_init();
    BeginDrawing();
    BeginTextureMode(st->tex_xorshift);
    ClearBackground(BLANK);
    start = clock();
    for(uint32_t i = 0; i < size * size; ++i) {
        int x = xorshift32_rand(&rstate) % size;
        int y = xorshift32_rand(&rstate) % size;
        DrawPixel(x, y, GREEN);
    }
    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("time for xorshift32_rand() - %f\n", elapsed);
    EndTextureMode();
    EndDrawing();
}

static Camera2D cam = { .zoom = 1. };

void stage_rand_update(Stage_rand *st) {
    Texture *tex = IsKeyDown(KEY_SPACE) ? 
        &st->tex_rand.texture : &st->tex_xorshift.texture;

    BeginMode2D(cam);
    DrawTexture(*tex, 10, 10, WHITE);
    EndMode2D();

    if (IsKeyDown(KEY_LEFT)) {
        cam.offset.x += 10.f;
    }
    if (IsKeyDown(KEY_RIGHT)) {
        cam.offset.x += -10.f;
    }
    if (IsKeyDown(KEY_UP)) {
        cam.offset.y += 10.f;
    }
    if (IsKeyDown(KEY_DOWN)) {
        cam.offset.y += -10.f;
    }
    if (IsKeyDown(KEY_Z)) {
        cam.zoom += 0.1;
    }
    if (IsKeyDown(KEY_X)) {
        if (cam.zoom > 0.05)
            cam.zoom -= 0.05;
    }

}

void stage_rand_shutdown(Stage_rand *st) {
    UnloadRenderTexture(st->tex_rand);
    UnloadRenderTexture(st->tex_xorshift);
}

