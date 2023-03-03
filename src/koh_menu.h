#pragma once

#include "raylib.h"

#include "koh_stages.h"
#include "koh_timer.h"

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

typedef struct Menu {
    Color      arrow_down_color, arrow_up_color;
    Font       fnt;
    MenuAction render_before;
    MenuItem   *items;
    Rectangle  scroll_rect;
    Timer      *tmr_arrow_blink;
    TimerStore timers;
    Vector2    pos;
    bool       font_owned, input_active, is_builded;
    const char *left_bracket, *right_bracket;
    int        i, visible_num;
    int        items_cap, items_num, active_item;
    int        maxwidth;
    void       *data;
} Menu;

typedef void MenuUpdateHandler(Menu *menu, void *udata);
typedef bool (*MenuIterFunc)(Menu *mnu, MenuItem *item, void *data);

MenuItem *menu_add(Menu *mnu, const char *caption, MenuAction act);
void menu_build(Menu *mnu);
void menu_down(Menu *mnu);
bool menu_each(Menu *mnu, MenuIterFunc func, void *data);
void menu_init(Menu *mnu, Font fnt, bool font_owned);
void menu_print_items(Menu *mnu);
void menu_render(Menu *mnu);
void menu_select(Menu *mnu);
void menu_shutdown(Menu *mnu);
void menu_up(Menu* mnu);
void menu_update(Menu *mnu, MenuUpdateHandler func, void *udata);

