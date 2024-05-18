// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_tst_menu.h"

#include "koh_common.h"
#include "koh_console.h"
#include "koh_hotkey.h"
/*#include "koh_input.h"*/
/*#include "koh_input.h"*/
#include "koh_logger.h"
#include "koh_menu.h"
#include "koh_timer.h"
#include "raylib.h"
#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void stage_tstmenu_init(Stage_TestMenu *st, void *data);
void stage_tstmenu_update(Stage_TestMenu *st);
void stage_tstmenu_shutdown(Stage_TestMenu *st);

Stage *stage_tstmenu_new(HotkeyStorage *hk_store) {
    Stage_TestMenu *st = calloc(1, sizeof(Stage_TestMenu));
    st->hk_store = hk_store;
    st->parent.init = (Stage_callback)stage_tstmenu_init;
    st->parent.update = (Stage_callback)stage_tstmenu_update;
    st->parent.shutdown = (Stage_callback)stage_tstmenu_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_tstmenu_enter;*/
    return (Stage*)st;
}

static const char *rules = 
"'1' и '2' для переключения менюшек\n"
"'R' для сброса";

const char *lines[] = {
// {{{ lines
    "Простишь ли мне ревнивые мечты,",
    "Моей любви безумное волненье?",
    "Ты мне верна: зачем же любишь ты",
    "Всегда пугать мое воображенье?",
    "Окружена поклонников толпой,",
    "Зачем для всех казаться хочешь милой,",
    "И всех дарит надеждою пустой",
    "Твой чудный взор, то нежный, то унылый?",
    "Мной овладев, мне разум омрачив,",
    "Уверена в любви моей несчастной,",
    //XXX: Что не так в строке, мочему она определяется очень длинной?
    /*"Не видишь ты, когда, в толпе их страстной,", // <- longest string*/
    "Беседы чужд, один и молчалив,",
    "Терзаюсь я досадой одинокой;",
    "Ни слова мне, ни взгляда... друг жестокой!", // <- longest string
    "Хочу ль бежать: с боязнью и мольбой",
    "Твои глаза не следуют за мной.",
    "Заводит ли красавица другая",
    "Двусмысленный со мною разговор -",
    "Спокойна ты; веселый твой укор",
    "Меня мертвит, любви не выражая.",
    "Скажи еще: соперник вечный мой,",
    "Наедине застав меня с тобой,",
    "Зачем тебя приветствует лукаво?..",
    "Что ж он тебе? Скажи, какое право",
    "Имеет он бледнеть и ревновать?..",
    "В нескромный час меж вечера и света,",
    "Без матери, одна, полуодета,",
    "Зачем его должна ты принимать?..",
    "Но я любим... Наедине со мною",
    "Ты так нежна! Лобзания твои",
    "Так пламенны! Слова твоей любви",
    "Так искренно полны твоей душою!",
    "Тебе смешны мучения мои;",
    "Но я любим, тебя я понимаю.",
    "Мой милый друг, не мучь меня, молю:",
    "Не знаешь ты, как сильно я люблю,",
    "Не знаешь ты, как тяжко я страдаю.",
    "Изыде сеятель сеяти семена своя.",
    "Свободы сеятель пустынный,",
    "Я вышел рано, до звезды;",
    "Рукою чистой и безвинной",
    "В порабощенные бразды",
    "Бросал живительное семя -",
    "Но потерял я только время,",
    "Благие мысли и труды...",
    "Паситесь, мирные народы!",
    "Вас не разбудит чести клич.",
    "К чему стадам дары свободы?",
    "Их должно резать или стричь.",
    "Наследство их из рода в роды",
    "Ярмо с гремушками да бич.",
    NULL,
// }}}
};

bool tmr_space(Timer *t) {
    Stage_TestMenu *st = (Stage_TestMenu*)t->data;
    Vector2 textsize = MeasureTextEx(st->fnt, st->message, st->fnt.baseSize, 0);
    DrawTextEx(
        st->fnt, st->message, 
        (Vector2) {
            .x = (GetScreenWidth() - textsize.x) / 2.,
            .y = (GetScreenHeight() - textsize.y) / 2.,
        },
        st->fnt.baseSize, 0, GREEN
    );
    return true;
}

void mnu_default(Menu *mnu, MenuItem *i) {
    trace("mnu_default:\n");
    Stage_TestMenu *st = menu_data_get(mnu);
    st->message = i->caption;
    st->tmr_select = koh_timerstore_new(&st->timers, &(Timer_Def) {
        .data = st,
        .duration = 1.,
        .every = 0,
        .waitfor = 0,
        .func = tmr_space,
    });
}

void stage_tstmenu_init(Stage_TestMenu *st, void *data) {
    trace("stage_tstmenu_init:\n");

    koh_timerstore_init(&st->timers, 0);

    //st->fnt = load_font_unicode("assets/dejavusansmono.ttf", 80);
    //st->fnt = load_font_unicode("assets/veramono.ttf", 80);
    st->fnt = load_font_unicode("assets/hack-regular.ttf", 50);

    st->mshort = menu_new((struct MenuSetup){
        .fnt = st->fnt,
        .font_owned = false,
        .pos = (Vector2) { 0, 200 },
        .scroll_rect.height = 400,
        .data = st,
        .arrow_up_color = RED,
        .arrow_down_color = RED,
    });

    for (int k = 0; k < 4; ++k) {
        char buf[200] = {0};
        snprintf(buf, sizeof(buf), "%3d %s", k, lines[k]);
        menu_add(st->mshort, buf, mnu_default);
    }
    menu_build(st->mshort);

    st->mlong = menu_new((struct MenuSetup){
        .fnt = st->fnt,
        .font_owned = false,
        .pos = (Vector2) { 50, 200 },
        .scroll_rect.height = 400,
        .data = st,
        .arrow_up_color = RED,
        .arrow_down_color = RED,
    });

    int linesnum = 0;
    while(lines[linesnum++]) { };
    linesnum--;

    trace("linesnum %d\n", linesnum);

    for (int k = 0; k < linesnum; ++k) {
        char buf[200] = {0};
        snprintf(buf, sizeof(buf), "%3d %s", k, lines[k]);
        menu_add(st->mlong, buf, mnu_default);
    }
    menu_build(st->mlong);

    st->active = st->mshort;
}

static void handler(Menu *mnu, void *udata) {
    assert(mnu);
    /*
    // FIXME: Сделать обработчик ввода как для библиотеки, так и для t80
    if (input.is_up()) {
        menu_up(mnu);
    }
    if (input.is_down()) {
        menu_down(mnu);
    }
    if (input.is_select()) {
        menu_select(mnu);
    }
    */
}

void stage_tstmenu_update(Stage_TestMenu *st) {
    trace("stage_tstmenu_update:\n");

    if (IsKeyPressed(KEY_ONE)) {
        st->active = st->mshort;
    } else if (IsKeyPressed(KEY_TWO)) {
        st->active = st->mlong;
    } else if (IsKeyPressed(KEY_R)) {
        stage_tstmenu_shutdown(st);
        stage_tstmenu_init(st, NULL);
    }

    menu_update(st->active, handler, NULL);
    Rectangle mnu_rect = menu_scroll_rect_get(st->active);

    Vector2 active_pos = menu_pos_get(st->active);
    mnu_rect.x = active_pos.x;
    mnu_rect.y = active_pos.y;

    const int thick = 5;
    DrawRectangleLinesEx(mnu_rect, thick, BLUE);

    DrawTextEx(st->fnt, rules, (Vector2) {0, 800}, st->fnt.baseSize, 0, YELLOW);
    koh_timerstore_update(&st->timers);
}

void stage_tstmenu_shutdown(Stage_TestMenu *st) {
    trace("stage_tstmenu_shutdown:\n");
    koh_timerstore_shutdown(&st->timers);
    menu_free(st->mlong);
    menu_free(st->mshort);
}

//XXX: Почему _enter() функции не вызываются?
void stage_tstmenu_enter(Stage_TestMenu *st, void *data) {
    trace("stage_tstmenu_enter:\n");
    hotkey_group_enable(st->hk_store, HOTKEY_GROUP_CONSOLE, true);
}


