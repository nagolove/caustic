// vim: set colorcolumn=85
// vim: fdm=marker

/*
Объект для хранения и передвижения по пути, состоящим из точек.
*/

#include "koh_stage_path.h"

#include "koh_path.h"
#include "raylib.h"
#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void stage_path_init(Stage_Path *st, void *data);
void stage_path_update(Stage_Path *st);
void stage_path_shutdown(Stage_Path *st);

Stage *stage_path_new(void) {
    Stage_Path *st = calloc(1, sizeof(Stage_Path));
    st->parent.init = (Stage_data_callback)stage_path_init;
    st->parent.update = (Stage_callback)stage_path_update;
    st->parent.shutdown = (Stage_callback)stage_path_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_path_enter;*/
    return (Stage*)st;
}

void stage_path_init(Stage_Path *st, void *data) {
    printf("stage_path_init\n");
    path_init(&st->path);
}

void stage_path_update(Stage_Path *st) {
    printf("stage_path_update\n");
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = GetMousePosition();
        path_add(&st->path, mp);
    }

    //path_start(&st->path);
    //while (path_
}

void stage_path_shutdown(Stage_Path *st) {
    printf("stage_path_shutdown\n");
    path_shutdown(&st->path);
}

void stage_path_enter(Stage_Path *st, void *data) {
    printf("stage_path_enter\n");
}


