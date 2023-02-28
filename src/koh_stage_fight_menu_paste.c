#include "stage_fight_menu_paste.h"

#include <assert.h>
#include <stdio.h>

#include "cammode.h"
#include "chipmunk/chipmunk_types.h"

#include "routine.h"
#include "common.h"
#include "console.h"
#include "flames.h"
#include "hangar.h"
#include "helicopter.h"
#include "mine.h"
#include "object.h"
#include "raylib.h"
#include "stage_fight.h"
#include "stages.h"
#include "tank.h"
#include "teleport.h"
#include "tree.h"
#include "xturret.h"

// Через сколько кадров регистровать нажатие мыши после вызова меню
#define FRAMES_SKIP 3
// Как сильно поворачивать редактируемый объект
#define ROTATION_SPEED 8

// Счетчит кадров
static int framecounter;
Object *current_object;

// Режим удаления объекта
void mode_removing_disable(Stage_Fight *st);
void mode_removing_enable(Stage_Fight *st);

// Режим выделения активного танка
void mode_selecting_tank_disable(Stage_Fight *st);
void mode_selecting_tank_enable(Stage_Fight *st);

// Вставка нового объекта
void mode_new_enable(Stage_Fight *st, Object *obj);
void mode_new_disable(Stage_Fight *st);

void poststep_remove_object(cpSpace *space, void *key, void *data) {
    assert(key);
    assert(data);
    cpShape *shape = key;
    if (shape->body && shape->body->userData) {
        //tank_free(data, shape->body->userData);
        Stage_Fight *st = (Stage_Fight*)data;
        bool removed = object_free(&st->obj_store, shape->body->userData);
        if (!removed) {
            printf("poststep_remove_object: not removed\n");
        }
    }
}

void menu_paste_btn_click_new_mine(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    DrawCircle(pos.x, pos.y, st->defs.tank_radius, VIOLET);
    //bool can_place = check_position(st, pos, OBJ_TANK);
    //console_write("tank_check_position %d", (int)can_place);
    //if (can_place) {
    Mine *tmp = mine_new(st, &(struct Mine_Def){ .pos = pos});
    if (tmp) {
        mine_collision_filter_set(st, tmp, CF_TRANSPARENT);
        mode_new_enable(st, (Object*)tmp);
    }
    //}
}

void menu_paste_btn_click_new_tank(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    DrawCircle(pos.x, pos.y, st->defs.tank_radius, VIOLET);
    //bool can_place = check_position(st, pos, OBJ_TANK);
    //console_write("tank_check_position %d", (int)can_place);
    //if (can_place) {
    Tank *tmp = tank_new(st, pos, 0.);
    if (tmp) {
        tank_collision_filter_set(st, tmp, CF_TRANSPARENT);
        mode_new_enable(st, (Object*)tmp);
    }
    //}
}

void menu_paste_btn_click_new_hangar(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    DrawCircle(pos.x, pos.y, st->defs.tank_radius, VIOLET);
    //bool can_place = check_position(st, pos, OBJ_HANGAR);
    //console_write("tank_check_position %d", (int)can_place);
    //if (can_place) {
    Hangar *tmp = hangar_new_def(st, &(Hangar_Def){
        .pos = pos,
        .fullud = false,
        .angle = 0.,
    });
    if (tmp) {
        hangar_collision_filter_set(st, tmp, CF_TRANSPARENT);
        mode_new_enable(st, (Object*)tmp);
    }
    //}
}

void menu_paste_btn_click_new_xturret(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    DrawCircle(pos.x, pos.y, st->defs.tank_radius, VIOLET);
    //bool can_place = check_position(st, pos, OBJ_XTURRET);
    //console_write("tank_check_position %d", (int)can_place);
    //if (can_place) {
    XTurret_Def def = {
        .pos = pos,
    };
    XTurret *tmp = xturret_new_def(st, &def);
    if (tmp) {
        xturret_collision_filter_set(st, tmp, CF_TRANSPARENT);
        mode_new_enable(st, (Object*)tmp);
    }
    //}
}

void menu_paste_btn_click_new_helicopter(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    DrawCircle(pos.x, pos.y, st->defs.tank_radius, VIOLET);
    //bool can_place = check_position(st, pos, OBJ_TELEPORT);
    //console_write("tank_check_position %d", (int)can_place);
    //if (can_place) {
    Helicopter_Def def = {
        .pos = pos,
    };
    Helicopter *tmp = helicopter_new(st, &def);
    if (tmp) {
        helicopter_collision_filter_set(st, tmp, CF_TRANSPARENT);
        mode_new_enable(st, (Object*)tmp);
    }
    //}
}

void menu_paste_btn_click_new_teleport(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    DrawCircle(pos.x, pos.y, st->defs.tank_radius, VIOLET);
    //bool can_place = check_position(st, pos, OBJ_TELEPORT);
    //console_write("tank_check_position %d", (int)can_place);
    //if (can_place) {
    Teleport_Def def = {
        .pos = pos,
    };
    Teleport *tmp = teleport_new_def(st, &def);
    if (tmp) {
        teleport_collision_filter_set(st, tmp, CF_TRANSPARENT);
        mode_new_enable(st, (Object*)tmp);
    }
    //}
}

void menu_paste_btn_click_new_tree(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    DrawCircle(pos.x, pos.y, st->defs.tank_radius, VIOLET);
    //bool can_place = check_position(st, pos, OBJ_TREE);
    //console_write("tank_check_position %d", (int)can_place);
    //if (can_place) {
    Tree_Def def = {
        .pos = pos,
    };
    Tree *tmp = tree_new_def(st, &def);
    if (tmp) {
        tree_collision_filter_set(st, tmp, CF_TRANSPARENT);
        mode_new_enable(st, (Object*)tmp);
    }
    //}
}

void query_point_select_tank(
        cpShape *shape,
        cpVect point,
        cpFloat distance,
        cpVect gradient, 
        void *data_st
) {
    Stage_Fight *st = (Stage_Fight*)data_st;
    if (shape->body && 
        shape->body->userData &&
        ((Object*)shape->body->userData)->type == OBJ_TANK) {
        //cpSpaceAddPostStepCallback(
            //((Stage_Fight*)data_st)->space,
            //poststep_remove_object,
            //shape,
            //data_st
        //);
        st->current_tank = (Tank*)shape->body->userData;
        mode_selecting_tank_disable(st);
        printf("query_point_select_tank: added tp post-step callback\n");
        //Object *obj = (Object*)shape->body->userData;
        //object_free(&st->obj_store, obj);
    }
}

void query_point_remove_object(
        cpShape *shape,
        cpVect point,
        cpFloat distance,
        cpVect gradient, 
        void *data_st
) {
    Stage_Fight *st = (Stage_Fight*)data_st;
    if (shape->body && shape->body->userData) {
        cpSpaceAddPostStepCallback(
            ((Stage_Fight*)data_st)->space,
            poststep_remove_object,
            shape,
            data_st
        );
        mode_removing_disable(st);
        printf("query_point_remove_object: added tp post-step callback\n");
        //Object *obj = (Object*)shape->body->userData;
        //object_free(&st->obj_store, obj);
    }
}

void menu_paste_btn_click_remove_object(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    printf("menu_paste_btn_click_remove_object\n");
    mode_removing_enable(st);
}

void menu_paste_btn_click_select_tank(Control *ctrl) {
    printf("menu_paste_btn_click_select_tank\n");
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    mode_selecting_tank_enable(st);
}

void menu_paste_btn_click_deselect_tank(Control *ctrl) {
    Stage_Fight *st = (Stage_Fight*)ctrl->data;
    st->current_tank = NULL;
    printf("menu_paste_btn_click_deselect_tank\n");
}

VContainer *menu_init_paste(Stage_Fight *st) {
    const char *delimeter = "----------";
    VContainer *mnu = vcontainer_new(MOUSE_BUTTON_MIDDLE, "dev_paste");
    mnu->mousebtn_click = MOUSE_BUTTON_LEFT;

    Button *btn = NULL;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Ангар",
        .click = (ControlFunc)menu_paste_btn_click_new_hangar,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Мина",
        .click = (ControlFunc)menu_paste_btn_click_new_mine,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Танк",
        .click = (ControlFunc)menu_paste_btn_click_new_tank,
    }));
    btn->ctrl.data = st;

    btn =(Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Турель",
        .click = (ControlFunc)menu_paste_btn_click_new_xturret,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Телепорт",
        .click = (ControlFunc)menu_paste_btn_click_new_teleport,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Вертолет",
        .click = (ControlFunc)menu_paste_btn_click_new_helicopter,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Дерево",
        .click = (ControlFunc)menu_paste_btn_click_new_tree,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = (char*)delimeter,
        .click = NULL,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Стереть объект",
        .click = (ControlFunc)menu_paste_btn_click_remove_object,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = (char*)delimeter, 
        .click = NULL,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Выделить танк",
        .click = (ControlFunc)menu_paste_btn_click_select_tank,
    }));
    btn->ctrl.data = st;

    btn = (Button*)vcontainer_add(mnu, btn_new((Button_Def) {
        .fnt = st->fnt_second,
        .caption = "Снять выделение",
        .click = (ControlFunc)menu_paste_btn_click_deselect_tank,
    }));
    btn->ctrl.data = st;

    // Закончить добавление элементов и построить внутренни структуры 
    // контейнера.
    vcontainer_build(mnu);

    return mnu;
}

void action_remove(Stage_Fight *st) {
    console_write("object removing..");

    if (framecounter > FRAMES_SKIP &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        cpSpacePointQuery(
            st->space, 
            from_Vector2(GetScreenToWorld2D(GetMousePosition(), *cam_get())),
            0, 
            ALL_FILTER, 
            query_point_remove_object,
            st
        );
    }
    if (IsKeyDown(KEY_ESCAPE)) { 
        mode_removing_disable(st);
    }
}

void action_selecting_tank(Stage_Fight *st) {
    console_write("tank selecting..");

    if (framecounter > FRAMES_SKIP &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        cpSpacePointQuery(
            st->space, 
            from_Vector2(GetScreenToWorld2D(GetMousePosition(), *cam_get())),
            0, 
            ALL_FILTER, 
            query_point_select_tank,
            st
        );
    }
    if (IsKeyDown(KEY_ESCAPE)) { 
        mode_selecting_tank_disable(st);
    }
}

void action_new(Stage_Fight *st) {
    console_write("object creation&placing..");

    Vector2 pos = GetScreenToWorld2D(GetMousePosition(), *cam_get());
    float delta_angle = GetMouseWheelMove() * GetFrameTime() * ROTATION_SPEED;

    if (current_object) {
        switch (current_object->type) {
            case OBJ_TANK:
                tank_edit_move(st, (Tank*)current_object, pos);
                tank_edit_rotate(st, (Tank*)current_object, delta_angle);
                break;
            case OBJ_HANGAR:
                hangar_edit_move(st, (Hangar*)current_object, pos);
                hangar_edit_rotate(st, (Hangar*)current_object, delta_angle);
                break;
            case OBJ_TELEPORT:
                teleport_edit_move(st, (Teleport*)current_object, pos);
                teleport_edit_rotate(st, (Teleport*)current_object, delta_angle);
                break;
            case OBJ_TREE:
                tree_edit_move(st, (Tree*)current_object, pos);
                tree_edit_rotate(st, (Tree*)current_object, delta_angle);
                break;
            case OBJ_XTURRET:
                xturret_edit_move(st, (XTurret*)current_object, pos);
                xturret_edit_rotate(st, (XTurret*)current_object, delta_angle);
                break;
         default:
                break;
        }
    }
    
    if (framecounter > FRAMES_SKIP &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

        switch (current_object->type) {
            case OBJ_TANK: {
                Tank *tmp = (Tank*)current_object;
                tank_collision_filter_set(st, tmp, CF_NORMAL);
                break;
            }
            case OBJ_HANGAR: {
                Hangar *tmp = (Hangar*)current_object;
                hangar_collision_filter_set(st, tmp, CF_NORMAL);
                break;
            }
            case OBJ_TELEPORT: {
                Teleport *tmp = (Teleport*)current_object;
                teleport_collision_filter_set(st, tmp, CF_NORMAL);
                break;
            }
            case OBJ_TREE: {
                Tree *tmp = (Tree*)current_object;
                tree_collision_filter_set(st, tmp, CF_NORMAL);
                break;
            }
            case OBJ_XTURRET: {
                XTurret *tmp = (XTurret*)current_object;
                xturret_collision_filter_set(st, tmp, CF_NORMAL);
                break;
            }
         default:
                break;
        }

        mode_new_disable(st);

        /*
        cpSpacePointQuery(
            st->space, 
            from_Vector2(GetScreenToWorld2D(GetMousePosition(), *cmn.cam)),
            0, 
            ALL_FILTER, 
            query_point_select_tank,
            st
        );
        */

    }
    if (IsKeyDown(KEY_ESCAPE)) { 
        mode_new_disable(st);
    }
}

void menu_paste_update_actions(Stage_Fight *st) {
    if (st->is_action_remove) {
        action_remove(st);
    } else if (st->is_action_select_tank) {
        action_selecting_tank(st);
    } else if (st->is_action_new) {
        action_new(st);
    }

    framecounter++;
}

void mode_removing_disable(Stage_Fight *st) {
    st->is_action_remove = false;
    framecounter = 0;
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

void mode_removing_enable(Stage_Fight *st) {
    st->is_action_remove = true;
    framecounter = 0;
    SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
}

void mode_selecting_tank_disable(Stage_Fight *st) {
    st->is_action_select_tank = false;
    framecounter = 0;
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

//TODO: отключать режим выделения после первого клика не по танку
//Или отключать по нажатию на escape
void mode_selecting_tank_enable(Stage_Fight *st) {
    st->is_action_select_tank = true;
    framecounter = 0;
    SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
}

void mode_new_enable(Stage_Fight *st, Object *obj) {
    current_object = obj;
    st->is_action_new = true;
    framecounter = 0;
}

void mode_new_disable(Stage_Fight *st) {
    current_object = NULL;
    st->is_action_new = false;
    framecounter = 0;
}
