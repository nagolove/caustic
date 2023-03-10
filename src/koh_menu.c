// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_menu.h"

#include "koh_common.h"
#include "koh_console.h"
#include "koh_logger.h"
#include "koh_timer.h"
#include "raylib.h"
#include "utf8proc.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    return true;
}

void menu_init(Menu *mnu, Font fnt, bool font_owned) {
    assert(mnu);
    memset(mnu, 0, sizeof(*mnu));
    mnu->fnt = fnt;
    mnu->font_owned = font_owned;
    mnu->input_active = true;
    mnu->scroll_rect = (Rectangle) {
        .x = 0, .y = 0,
        .width = GetScreenWidth(), .height = GetScreenHeight(),
    };
    /*mnu->arrows_color = YELLOW;*/
    timerstore_init(&mnu->timers, 1);
    mnu->tmr_arrow_blink = timerstore_new(&mnu->timers, &(Timer_Def) {
        .waitfor  = 0,
        .every    = 0.05,
        .duration = -1,
        .data     = mnu,
        .func     = tmr_timer_blink,
    });
    mnu->items_cap = 10;
    mnu->items = calloc(mnu->items_cap, sizeof(mnu->items[0]));
    mnu->left_bracket = left_bracket;
    mnu->right_bracket = right_bracket;
}

void menu_shutdown(Menu *mnu) {
    assert(mnu);
    free(mnu->items);
    if (mnu->font_owned) {
        UnloadFont(mnu->fnt);
    }
    timerstore_shutdown(&mnu->timers);
    memset(mnu, 0, sizeof(*mnu));
}

static bool has_scroll(Menu *mnu) {
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
    char *caption = mnu->items[item_index].caption;
    char buf[MAX_CAPTION + 32] = {0, };
    snprintf(
        buf, sizeof(buf), "%s%s%s", 
        mnu->left_bracket, caption, mnu->right_bracket
    );
    DrawTextEx(mnu->fnt, buf, pos, mnu->fnt.baseSize, 0, BLACK);
}

void item_draw(Menu *mnu, Vector2 pos, int item_index) {
    char *caption = mnu->items[item_index].caption;
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
    for(int j = 0; j < visible_num; ++j) {
    //for(int i = mnu->i; i < mnu->items_num; ++i) {
        /*printf("render (%f, %f)\n", pos.x, pos.y);*/
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

    timerstore_update(&mnu->timers);
    menu_render(mnu);

    if (mnu->input_active && !console_is_editor_mode()) {
        if (func)
            func(mnu, udata);
    }

}

MenuItem *menu_add(Menu *mnu, const char *caption, MenuAction act) {
    assert(mnu);
    assert(caption);
    assert(act);
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

    char *longest_caption = mnu->items[maxwidth_index].caption;
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

