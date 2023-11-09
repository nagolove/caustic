// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_iface.h"

#include "koh_common.h"
#include "koh_console.h"
#include "koh_iface.h"
#include "koh_stages.h"
#include "raylib.h"
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Stage_iface {
    Stage parent;
    VContainer *mnu1, *mnu2;
    //bool is_mnu_open; //, is_first_iter;
    Vector2 mousep;
    Font fnt;
} Stage_iface;

void stage_iface_init(Stage_iface *st, void *data);
void stage_iface_update(Stage_iface *st);
void stage_iface_shutdown(Stage_iface *st);
void stage_iface_enter(Stage_iface *st, void *data);

Stage *stage_iface_new(void) {
    Stage_iface *st = calloc(1, sizeof(Stage_iface));
    st->parent.init = (Stage_data_callback)stage_iface_init;
    st->parent.update = (Stage_callback)stage_iface_update;
    st->parent.shutdown = (Stage_callback)stage_iface_shutdown;
    st->parent.enter = (Stage_data_callback)stage_iface_enter;
    return (Stage*)st;
}

void btn_click(Button *btn) {
    assert(btn);
    printf("button %s clicked\n", btn->caption);
}

Vector2 _GetMousePosition(void *udata) {
    return GetMousePosition();
}

bool _IsMouseButtonPressed(int button, void *udata) {
    return IsMouseButtonPressed(button);
}

static struct IInputSources inp = {
    .mouse_position = _GetMousePosition,
    .mouse_is_button_pressed = _IsMouseButtonPressed,
    .is_down = NULL,
    .is_up = NULL,
    .is_select = NULL,
};

VContainer *menu2_init(Stage_iface *st) {
    VContainer *mnu = vcontainer_new(MOUSE_BUTTON_LEFT, "menu2", inp);
    //Button *btn = NULL;

    //btn = 
    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Ангар",
        .click = (ControlFunc)btn_click,
    }));

    //btn->color_caption = BLUE;
    //btn->color = RED;

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Танк",
        .click = (ControlFunc)btn_click,
    }));

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Турель",
        .click = (ControlFunc)btn_click,
    }));

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Телепорт",
        .click = (ControlFunc)btn_click,
    }));

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Дерево",
        .click = (ControlFunc)btn_click,
    }));

    //vcontainer_add(mnu, btn_new((Button_Def) {
        //.fnt = st->fnt,
        //.caption = "Земляк|Писняк|Икняк",
        //.click = (ControlFunc)btn_click,
    //}));

    // Закончить добавление элементов и построить внутренни структуры 
    // контейнера.
    vcontainer_build(mnu);

    return mnu;
}

VContainer *menu1_init(Stage_iface *st) {
    VContainer *mnu = vcontainer_new(MOUSE_BUTTON_LEFT, "menu1", inp);
    Button *btn = NULL;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Кнопкарь",
        .click = (ControlFunc)btn_click,
    }));

    btn->color_caption = BLUE;
    btn->color = RED;

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Нажимарь",
        .click = (ControlFunc)btn_click,
    }));

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Дохляк",
        .click = (ControlFunc)btn_click,
    }));

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Здоровяк",
        .click = (ControlFunc)btn_click,
    }));

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Пухля|Долбяк|Межняк",
        .click = (ControlFunc)btn_click,
    }));

    vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt,
        .caption = "Земляк|Писняк|Икняк",
        .click = (ControlFunc)btn_click,
    }));


    // Закончить добавление элементов и построить внутренни структуры 
    // контейнера.
    vcontainer_build(mnu);

    return mnu;
}

void stage_iface_init(Stage_iface *st, void *data) {
    printf("stage_iface_init\n");

    st->fnt = load_font_unicode("assets/dejavusansmono.ttf", 30);
    //st->is_mnu_open = false;

    /*printf("sizeof(st->fnt) = %d\n", (int)sizeof(st->fnt));*/

    st->mnu1 = menu1_init(st);
    st->mnu2 = menu2_init(st);
}

void stage_iface_update(Stage_iface *st) {
    /*printf("stage_iface_update\n");*/
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    int radius = 100;
    DrawCircle(cx, cy, radius, RED);

    //Vector2 mousep = GetMousePosition();

    console_is_editor_mode();

    if (vcontainer_update(st->mnu2, NULL)) {
        //printf("vcontainer_update() == false\n");
    }

    /*
       if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) {
           if (vcontainer_update(st->mnu2)) {
            //printf("vcontainer_update() == false\n");
        }
    }
    */

    /*
    (ContainerContext){
        .mousebtn_open = MOUSE_BUTTON_LEFT,
        .mousebtn_click = MOUSE_BUTTON_LEFT,
        .usegamepad = false});
    */

    //if (!st->is_mnu_open && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        //printf("stage_iface_update: mouse pressed\n");
        //st->is_mnu_open = true;
    //}

    //if (st->is_mnu_open)
        //vcontainer_update(st->mnu, st->mousep, st->is_first_iter);

    //if (st->is_first_iter)
        //st->is_first_iter = false;

    //if (st->is_mnu_open && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        //!vcontainer_is_in_area(st->mnu, mousep)) {
        //st->is_mnu_open = false;
    //}

}

void stage_iface_shutdown(Stage_iface *st) {
    assert(st);
    UnloadFont(st->fnt);
    printf("stage_iface_shutdown\n");
    vcontainer_shutdown(st->mnu1);
    vcontainer_shutdown(st->mnu2);
    //free(st->mnu1);
    //free(st->mnu2);
}

void stage_iface_enter(Stage_iface *st, void *data) {
    printf("stage_iface_enter\n");
}


