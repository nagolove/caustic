// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_empty.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

void stage_empty_init(Stage_Empty *st, void *data);
void stage_empty_update(Stage_Empty *st);
void stage_empty_shutdown(Stage_Empty *st);

Stage *stage_empty_new(void) {
    Stage_Empty *st = calloc(1, sizeof(Stage_Empty));
    st->parent.init = (Stage_data_callback)stage_empty_init;
    st->parent.update = (Stage_callback)stage_empty_update;
    st->parent.shutdown = (Stage_callback)stage_empty_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_empty_enter;*/
    return (Stage*)st;
}

void stage_empty_init(Stage_Empty *st, void *data) {
    printf("stage_empty_init\n");
}

void stage_empty_update(Stage_Empty *st) {
    printf("stage_empty_update\n");
}

void stage_empty_shutdown(Stage_Empty *st) {
    printf("stage_empty_shutdown\n");
}

void stage_empty_enter(Stage_Empty *st, void *data) {
    printf("stage_empty_enter\n");
}


