#include "stage_fight_menu_player.h"
#include "arrow.h"
#include "common.h"
#include "iface.h"
#include "object.h"
#include "raylib.h"
#include "raymath.h"
#include "stage_fight.h"
#include "teleport.h"
#include "routine.h"

#include <assert.h>
#include <stdio.h>

const float arrow_time = 2.;

void menu_player_find_nearest_teleport(Control *ctrl) {
    printf("menu_player_find_nearest_teleport\n");
    Stage_Fight *st = ctrl->data;
    if (st->current_tank) {
        Vector2 pos = from_Vect(st->current_tank->b.p);
        Teleport * t = teleports_find_nearest(st, pos);
        if (t) {
            Vector2 dir = Vector2Subtract(from_Vect(t->b.p), pos);
            arrow_circle_add(
                st->arrow_circle, to_polarv(dir).angle, 
                arrow_time, 
                RED
            );
        }
    }
}

void menu_player_place_mine(Control *ctrl) {
    printf("menu_player_place_mine\n");
}

void menu_player_shahid(Control *ctrl)  {
    printf("menu_player_shahid\n");
}

VContainer *menu_init_player(Stage_Fight *st) {
    VContainer *mnu = vcontainer_new(-1, "player");
    //mnu->mousebtn_click = MOUSE_BUTTON_LEFT;

    Button *btn = NULL;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Найти телепорт рядом",
        .click = (ControlFunc)menu_player_find_nearest_teleport,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Поставить мину",
        .click = (ControlFunc)menu_player_place_mine,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Самоподрыв",
        .click = (ControlFunc)menu_player_shahid,
    }));
    btn->ctrl.data = st;

    // Закончить добавление элементов и построить внутренни структуры 
    // контейнера.
    vcontainer_build(mnu);

    return mnu;
}
