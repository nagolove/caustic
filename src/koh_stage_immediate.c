// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_immediate.h"
#include "common.h"
#include "raymath.h"

#include <assert.h>
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

/*#define RAYGUI_IMPLEMENTATION*/
/*#include "raygui.h"*/

void stage_immediate_init(Stage_immediate *st, void *data);
void stage_immediate_update(Stage_immediate *st);
void stage_immediate_shutdown(Stage_immediate *st);

enum UI_LAYOUT {
    UI_LAYOUT_VERTICAL,
};

struct UI {
    Font      font;
    Vector2   last_pos;
    Rectangle bound;
    Color     color_back, color_font, color_selected;
    enum UI_LAYOUT  layout;
};

static struct UI ui = {0};

void ui_begin(struct UI *ui, enum UI_LAYOUT layout, Rectangle bound) {
    assert(ui);
    ui->last_pos = Vector2Zero();
    ui->bound = bound;
}

void ui_end(struct UI *ui) {
}

bool ui_button(struct UI *ui, const char *caption) {
    return false;
}

Stage *stage_immediate_new(void) {
    Stage_immediate *st = calloc(1, sizeof(Stage_immediate));
    st->parent.init = (Stage_data_callback)stage_immediate_init;
    st->parent.update = (Stage_callback)stage_immediate_update;
    st->parent.shutdown = (Stage_callback)stage_immediate_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_immediate_enter;*/
    return (Stage*)st;
}

void stage_immediate_init(Stage_immediate *st, void *data) {
    printf("stage_immediate_init\n");
    ui.font = load_font_unicode("assets/dejavusansmono.ttf", 40);
    ui.color_font = BLACK;
    ui.color_back = WHITE;
    ui.color_selected = BLUE;
}

void stage_immediate_update(Stage_immediate *st) {
    //printf("stage_immediate_update\n");

    ui_begin(&ui, UI_LAYOUT_VERTICAL, (Rectangle){
        .x = GetMouseX(),
        .y = GetMouseY(),
        .width = 200, // как расчитать максимально возможную ширину?
        .height = 0,
    });

    if (ui_button(&ui, "aaaaa")) {
        printf("aaaaa pressed\n");
    }

    if (ui_button(&ui, "bbbbbb")) {
        printf("bbbbbb pressed\n");
    }

    ui_end(&ui);
}

void stage_immediate_shutdown(Stage_immediate *st) {
    printf("stage_immediate_shutdown\n");
}

void stage_immediate_enter(Stage_immediate *st, void *data) {
    printf("stage_immediate_enter\n");
}
