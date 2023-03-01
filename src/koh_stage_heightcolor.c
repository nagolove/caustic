#include "koh_stage_heightcolor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"

#include "koh_common.h"
#include "koh_height_color.h"

const int rectw = 128, recth = 64;
Rectangle *rects = NULL;

Stage *stage_heightcolor_new(void) {
    Stage_HeightColor *st = calloc(1, sizeof(Stage_HeightColor));
    st->parent.init = (Stage_data_callback)stage_heightcolor_init;
    st->parent.update = (Stage_callback)stage_heightcolor_update;
    st->parent.shutdown = (Stage_callback)stage_heightcolor_shutdown;
    // Зачем нужна эта дополнительная инициализация?
    /*st->parent.init((Stage*)st);*/
    return (Stage*)st;
}

void stage_heightcolor_init(Stage_HeightColor *st, void *data) {
    /*setup_colorsnum();*/

    int x = (GetScreenWidth() - rectw) / 2;
    int y = (GetScreenHeight() - recth * get_height_colors_num()) / 2;
    rects = calloc(get_height_colors_num(), sizeof(rects[0]));
    for(int i = 0; i < get_height_colors_num(); i++) {
        Rectangle *rect = &rects[i];
        rect->width = rectw;
        rect->height = recth;
        rect->x = x;
        rect->y = y;
        y += recth;
    }
}

void stage_heightcolor_update(Stage_HeightColor *st) {
    BeginDrawing();

    Color color = height_color(GetMouseY() / (float)GetScreenHeight());
    ClearBackground(color);
    //ClearBackground(GRAY);

    int thick = 4;
    for(int i = 0; i < get_height_colors_num(); i++) {
        DrawRectangleRec(rects[i], height_colors[i]);
        DrawRectangleLinesEx(rects[i], thick, BLACK);
    }
    EndDrawing();
}

void stage_heightcolor_shutdown(Stage_HeightColor *st) {
    free(rects);
}

