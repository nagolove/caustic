// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_menu.h"

#include "koh_common.h"
//#include "koh_console.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "koh_routine.h"
#include "lua.h"
#include "raylib.h"
#include "utf8proc.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "koh_timerman.h"

#define MENU_MAX_NAME   48

struct Menu {
    StagesStore *ss;
    Color       arrow_down_color, arrow_up_color;
    Font        fnt;
    MenuAction  render_before;
    MenuItem    *items;
    Rectangle   scroll_rect;
    //koh_Timer   *tmr_arrow_blink;
    TimerMan    *timers;
    Vector2     pos;
    bool        font_owned, input_active, is_builded;
    const char  *left_bracket, *right_bracket;
    int         i, visible_num;
    int         items_cap, items_num, active_item;
    int         maxwidth;
    char        name[MENU_MAX_NAME];
    void        *data;
};

static const char *left_bracket = "[[", *right_bracket = "]]";

bool tmr_timer_blink(Timer *t) {
    //trace("timer_blink\n");
    Menu *mnu = t->data;

    //XXX: Не работает смена фазы для стрелки вверх и стрелки вниз
    /*double time = GetTime();*/
    /*float iter = time - M_PI * floor(time / M_PI);*/
    /*trace("iter %f\n", time - M_PI * floor(time / M_PI));*/
    //mnu->arrow_down_color.a = floor((1. + sin(M_PI + iter))) / 2. * 255.;
    
    mnu->arrow_down_color.a = floor((1. + sin(GetTime())) / 2. * 255.);
    mnu->arrow_up_color.a = floor((1. + sin(GetTime())) / 2. * 255.);

    // XXX: или возвращать истину?
    return false;
}

void menu_dump(Menu *mnu) {
    assert(mnu);
    trace("menu_dump: '%s'\n", mnu->name);
    trace("menu_dump: arrow_down_color %s\n", color2str(mnu->arrow_down_color));
    trace("menu_dump: arrow_up_color %s\n", color2str(mnu->arrow_up_color));
    trace("menu_dump: fnt %s\n", font2str(mnu->fnt));
    trace("menu_dump: render_before %p\n", mnu->render_before);
    trace("menu_dump: items %p\n", mnu->items);
    trace("menu_dump: scroll_rect %s\n", rect2str(mnu->scroll_rect));
    //trace("menu_dump: tmr_arrow_blink %s\n", koh_timer2str(mnu->tmr_arrow_blink));
    //trace("menu_dump: timers num %d\n", mnu->timers.timersnum);
    trace("menu_dump: pos %s\n", Vector2_tostr(mnu->pos));
    trace("menu_dump: font_owned %s\n", mnu->font_owned ? "true" : "false");
    trace("menu_dump: input_active %s\n", mnu->input_active ? "true" : "false");
    trace("menu_dump: is_builded %s\n", mnu->is_builded ? "true" : "false");
    trace("menu_dump: left_bracket \"%s\"\n", mnu->left_bracket);
    trace("menu_dump: right_bracket \"%s\"\n", mnu->right_bracket);
    trace("menu_dump: i %d\n", mnu->i );
    trace("menu_dump: visible_num %d\n", mnu->visible_num);
    trace("menu_dump: items_cap %d\n", mnu->items_cap);
    trace("menu_dump: items_num %d\n", mnu->items_num);
    trace("menu_dump: active_item %d\n", mnu->active_item);
    trace("menu_dump: maxwidth %d\n", mnu->maxwidth);
    trace("menu_dump: data %p\n", mnu->data);
}

Menu *menu_new(MenuSetup setup) {
    Menu *mnu = calloc(1, sizeof(Menu));
    assert(mnu);
    mnu->ss = setup.ss;
    mnu->fnt = setup.fnt;
    mnu->font_owned = setup.font_owned;
    mnu->pos = setup.pos;
    mnu->active_item = setup.active_item;
    mnu->render_before = setup.render_before;
    mnu->input_active = setup.input_active;
    mnu->data = setup.data;

    if (setup.name)
        strncpy(mnu->name, setup.name, MENU_MAX_NAME - 1);

    if (setup.scroll_rect.height != 0.)
        mnu->scroll_rect.height = setup.scroll_rect.height;

    //mnu->input_active = true;
    mnu->scroll_rect = (Rectangle) {
        .x = 0, .y = 0,
        .width = GetScreenWidth(), .height = GetScreenHeight(),
    };
    mnu->arrow_up_color = setup.arrow_up_color;
    mnu->arrow_down_color = setup.arrow_down_color;
    /*mnu->arrows_color = YELLOW;*/

    //koh_timerstore_init(&mnu->timers, 1);
    mnu->timers = timerman_new(10, "mnu");

    /*
    mnu->tmr_arrow_blink = koh_timerstore_new(&mnu->timers, &(Timer_Def) {
        .waitfor  = 0,
        .every    = 0.05,
        .duration = -1,
        .data     = mnu,
        .func     = tmr_timer_blink,
    });
    */

    /*
    timerman_add(&mnu->timers, &(TimerDef) {
        .waitfor  = 0,
        .every    = 0.05,
        .duration = -1,
        .data     = mnu,
        .func     = tmr_timer_blink,
    });
*/

    mnu->items_cap = 10;
    mnu->items = calloc(mnu->items_cap, sizeof(mnu->items[0]));
    mnu->left_bracket = left_bracket;
    mnu->right_bracket = right_bracket;
    return mnu;
}

void menu_free(Menu *mnu) {
    assert(mnu);
    free(mnu->items);
    if (mnu->font_owned) {
        UnloadFont(mnu->fnt);
    }

    //koh_timerstore_shutdown(&mnu->timers);
    timerman_free(mnu->timers);

    memset(mnu, 0, sizeof(*mnu));
    free(mnu);
}

static bool has_scroll(const Menu *mnu) {
    return mnu->i + mnu->visible_num < mnu->items_num;
}

static void draw_arrows_down(Menu *mnu) {
    const char *arrow = "↓";
    float charw = MeasureTextEx(mnu->fnt, "w", mnu->fnt.baseSize, 0).x;
    Vector2 pos = { 
        .x = mnu->scroll_rect.x + charw * mnu->maxwidth / 2.,
        .y = mnu->scroll_rect.y + mnu->scroll_rect.height - mnu->fnt.baseSize,
    };
    DrawTextEx(
        mnu->fnt, arrow, pos,
        mnu->fnt.baseSize, 0, mnu->arrow_down_color
    );
}

void draw_arrows_up(Menu *mnu) {
    const char *arrow = "↑";
    float charw = MeasureTextEx(mnu->fnt, "w", mnu->fnt.baseSize, 0).x;
    Vector2 pos = { 
        .x = mnu->scroll_rect.x + charw * mnu->maxwidth / 2.,
        //.y = mnu->scroll_rect.y + mnu->fnt.baseSize,
        .y = mnu->scroll_rect.y,
    };
    DrawTextEx(
        mnu->fnt, arrow, pos,
        mnu->fnt.baseSize, 0, mnu->arrow_down_color
    );
}

void item_draw_active(Menu *mnu, Vector2 pos, int item_index) {
    const char *caption = mnu->items[item_index].caption;
    char buf[MAX_CAPTION + 32] = {0, };
    snprintf(
        buf, sizeof(buf), "%s%s%s", 
        mnu->left_bracket, caption, mnu->right_bracket
    );

    /*printf("item_draw_active: '%s'\n", buf);*/

    DrawTextEx(mnu->fnt, buf, pos, mnu->fnt.baseSize, 0, BLACK);
}

void item_draw(Menu *mnu, Vector2 pos, int item_index) {
    char *caption = mnu->items[item_index].caption;

    /*printf("item_draw: '%s'\n", caption);*/

    DrawTextEx(mnu->fnt, caption, pos, mnu->fnt.baseSize, 0, BLACK);
}

void menu_render(Menu *mnu) {
    assert(mnu);

    if (mnu->render_before)
        mnu->render_before(mnu, NULL);

    Vector2 pos = mnu->pos;
    int visible_num = mnu->scroll_rect.height / mnu->fnt.baseSize;
    
    if (mnu->i > 0)
        draw_arrows_up(mnu);

    //for(int i = mnu->i; i < visible_num; ++i) {

    /*
    printf(
        "menu_render: mnu %p, mnu->name %s, mnu->visible_num %d\n",
        mnu, mnu->name, visible_num
    );
    */

    int num = visible_num > mnu->items_num ? 
              mnu->items_num : visible_num;
    for(int j = 0; j < num; ++j) {
        int item_index = mnu->i + j;
        if (item_index == mnu->active_item) {
            item_draw_active(mnu, pos, item_index);
        } else {
            item_draw(mnu, pos, item_index);
        }
        pos.y += mnu->fnt.baseSize;
    }

    if (has_scroll(mnu)) {
        draw_arrows_down(mnu);
    }

}

void menu_up(Menu* mnu) {
    if (mnu->active_item <= mnu->i) {
        mnu->i--;
    }
    mnu->active_item--;
    if (mnu->active_item < 0) {
        mnu->active_item = mnu->items_num - 1;

        mnu->i = mnu->active_item - mnu->visible_num + 1;
        if (mnu->i < 0) mnu->i = 0;
    }
}

void menu_down(Menu *mnu) {
    mnu->active_item = (mnu->active_item + 1) % mnu->items_num;
    if (mnu->active_item < mnu->i) {
        mnu->i = 0;
    } else if (mnu->active_item == mnu->i + mnu->visible_num) {
        mnu->i++;
    }
}

void menu_select(Menu *mnu) {
    MenuItem *item = &mnu->items[mnu->active_item];
    assert(item);
    if (item->action) {
        trace("menu_select:\n");
        mnu->items[mnu->active_item].action(mnu, item);
    }
}

void menu_update(Menu *mnu, MenuUpdateHandler func, void *udata) {
    assert(mnu);

    if (!mnu->is_builded) {
        perror("use menu_build() before menu_update()\n");
        abort();
    }

    //koh_timerstore_update(&mnu->timers);
    menu_render(mnu);

    /*
    if (mnu->input_active && !console_is_editor_mode()) {
        if (func)
            func(mnu, udata);
    }
    */

}

MenuItem *menu_add(Menu *mnu, const char *caption, MenuAction act) {
    assert(mnu);
    assert(caption);
    assert(act);
    trace("menu_add: %s\n", caption);
    //assert(mnu->items_num < MAX_MENU_ITEMS);

    if (mnu->items_num == mnu->items_cap) {
        mnu->items_cap += 10;
        mnu->items_cap *= 1.5;
        mnu->items = realloc(
            mnu->items, sizeof(mnu->items[0]) * mnu->items_cap
        );
    }

    int i = mnu->items_num;
    mnu->items[i].action = act;
    assert(strlen(caption) < MAX_CAPTION);
    strcpy(mnu->items[i].caption, caption);
    mnu->items_num++;
    mnu->is_builded = false;
    return &mnu->items[i];
}

void menu_build(Menu *mnu) {
    int maxwidth = 0, maxwidth_index = 0;
    for (int i = 0; i < mnu->items_num; i++) {
        int width = u8_codeptlen(mnu->items[i].caption);
        if (width > maxwidth) {
            maxwidth = width;
            maxwidth_index = i;
        }
    }
    mnu->maxwidth = maxwidth;
    mnu->visible_num = mnu->scroll_rect.height / mnu->fnt.baseSize;
    mnu->scroll_rect.x = mnu->pos.x;
    mnu->scroll_rect.y = mnu->pos.y;
    mnu->is_builded = true;

    const char *longest_caption = mnu->items[maxwidth_index].caption;
    char full_longest_caption[MAX_CAPTION + 64] = {0, };
    snprintf(
        full_longest_caption, sizeof(full_longest_caption), "%s%s%s", 
        mnu->left_bracket, longest_caption, mnu->right_bracket
    );

    mnu->scroll_rect.width = MeasureTextEx(
        mnu->fnt, 
        full_longest_caption,
        mnu->fnt.baseSize,
        0
    ).x;

    trace("menu_build: full_longest_caption %s\n", full_longest_caption);
    trace("menu_build: menu_build: maxwidth_index %d\n", maxwidth_index);
    trace("menu_build: mnu->visible_num %d\n", mnu->visible_num);
    trace("menu_build: mnu->scroll_rect.width %f\n", mnu->scroll_rect.width);
}

void menu_print_items(Menu *mnu) {
    assert(mnu);
    for(int i = 0; i < mnu->items_num; ++i) {
        MenuItem *item = &mnu->items[i];
        trace("menu_print_items: %s, %p, %p\n", item->caption, item->action, item->data);
    }
}

bool menu_each(Menu *mnu, MenuIterFunc func, void *data) {
    assert(mnu);
    assert(func);
    for(int i = 0; i < mnu->items_num; i++) {
        if (func(mnu, &mnu->items[i], data)) {
            return true;
        }
    }
    return false;
}

void menu_data_set(Menu *mnu, void *data) {
    assert(mnu);
    mnu->data = data;
}

void *menu_data_get(Menu *mnu) {
    assert(mnu);
    return mnu->data;
}

Vector2 menu_pos_get(Menu *mnu) {
    assert(mnu);
    return mnu->pos;
}

void menu_pos_set(Menu *mnu, Vector2 pos) {
    assert(mnu);
    mnu->pos = pos;
}

Rectangle menu_scroll_rect_get(Menu *mnu) {
    assert(mnu);
    return mnu->scroll_rect;
}

void menu_input_active_set(Menu *mnu, bool state) {
    assert(mnu);
    mnu->input_active = state;
}

bool menu_input_active_get(Menu *mnu) {
    assert(mnu);
    return mnu->input_active;
}

void menu_active_reset(Menu *mnu) {
    assert(mnu);
    mnu->active_item = 0;
}

StagesStore *menu_get_stages_store(Menu *mnu) {
    assert(mnu);
    return mnu->ss;
}
