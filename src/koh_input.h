// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include <stdbool.h>
#include "koh_routine.h"

extern bool koh_verbose_input;

/* 
    NOTE: InputGamepadDrawer и InputKbMouseDrawer требуют следующих ресурсов
    
    "assets/gfx/xbox.png"
    "assets/gfx/mouse/mouse.png"
    "assets/gfx/mouse/rb.png"
    "assets/gfx/mouse/lb.png"
    "assets/gfx/mouse/wheel.png"
*/

typedef struct InputKbMouseDrawer InputKbMouseDrawer;

typedef struct InputKbMouseDrawerSetup {
    int btn_width;
} InputKbMouseDrawerSetup;

InputKbMouseDrawer *input_kb_new(InputKbMouseDrawerSetup *setup);
void input_kb_free(InputKbMouseDrawer *kb);
void input_kb_update(InputKbMouseDrawer *kb);
void input_kb_gui_update(InputKbMouseDrawer *kb);

// XXX: Сделать подсветку клавишы с биндом и подсказку
// XXX: Как быть если должен быть модификатор?

typedef struct KbStroke {
                // KEY_ESCAPE и тд
    i32        keycode;
    bool       mod_shift;
} KbStroke;

typedef struct KbBind {
    KbStroke   s;
    // строка подсказки
    // NOTE: строка должна быть доступна все время работы InputKbMouseDrawer
    const char *msg;
    // возвращает подсказку если msg == NULL
    const char *(*get_msg)(KbStroke s, void *udata);
    void *udata;
} KbBind;

char *kb_stroke2str(KbStroke s);
KbStroke input_kb_bind(InputKbMouseDrawer *kb, KbBind b);
bool input_kb_is_pressed(KbStroke s);

typedef struct InputGamepadDrawer InputGamepadDrawer;

typedef struct InputGamepadDrawerSetup {
    float scale;
} InputGamepadDrawerSetup;

InputGamepadDrawer *input_gp_new(InputGamepadDrawerSetup *setup);
void input_gp_free(InputGamepadDrawer *gp);
void input_gp_update(InputGamepadDrawer *gp);
/*void input_gp_gui_update(InputGamepadDrawer *kb);*/
