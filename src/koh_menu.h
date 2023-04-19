#pragma once

#include "koh_stages.h"
#include "koh_timer.h"
#include "raylib.h"

#define MAX_MENU_ITEMS  20
#define MAX_CAPTION     128

typedef struct MenuItem MenuItem;
typedef struct Menu Menu;

typedef void (*MenuAction)(Menu *mnu, MenuItem *item);

typedef struct MenuItem {
    char caption[MAX_CAPTION];
    MenuAction action;
    void *data;
} MenuItem;

typedef struct MenuSetup {
    Vector2     pos;
    Rectangle   scroll_rect;
    void        *data;
    Font        fnt;
    bool        font_owned;
    Color       arrow_up_color, arrow_down_color;
    MenuAction  render_before;
    bool        input_active;
    int         active_item;
    char        *name;
} MenuSetup;

typedef struct Menu Menu;
typedef void MenuUpdateHandler(Menu *menu, void *udata);
typedef bool (*MenuIterFunc)(Menu *mnu, MenuItem *item, void *data);

MenuItem *menu_add(Menu *mnu, const char *caption, MenuAction act);
void menu_build(Menu *mnu);
void menu_down(Menu *mnu);
bool menu_each(Menu *mnu, MenuIterFunc func, void *data);
Menu *menu_new(MenuSetup setup);
void menu_print_items(Menu *mnu);
void menu_render(Menu *mnu);
void menu_select(Menu *mnu);
void menu_free(Menu *mnu);
void menu_up(Menu* mnu);
void menu_update(Menu *mnu, MenuUpdateHandler func, void *udata);

void menu_data_set(Menu *mnu, void *data);
void *menu_data_get(Menu *mnu);
Vector2 menu_pos_get(Menu *mnu);
void menu_pos_set(Menu *mnu, Vector2 pos);
Rectangle menu_scroll_rect_get(Menu *mnu);
void menu_input_active_set(Menu *mnu, bool state);
bool menu_input_active_get(Menu *mnu);
void menu_active_reset(Menu *mnu);
void menu_dump(Menu *mnu);

