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

// Опережающее объявление — общий контейнер биндов (см. ниже).
typedef struct InputBinder InputBinder;

typedef struct InputKbMouseDrawerSetup {
    int btn_width;
    // опциональный общий контейнер биндов для отрисовки подсказок
    InputBinder *binder;
} InputKbMouseDrawerSetup;

InputKbMouseDrawer *input_kb_new(InputKbMouseDrawerSetup *setup);
void input_kb_free(InputKbMouseDrawer *kb);
void input_kb_update(InputKbMouseDrawer *kb);
void input_kb_gui_update(InputKbMouseDrawer *kb);

// Модификатор клавиши. На одну кнопку можно повесить
// до KB_MOD_LAST биндов — по одному на каждый модификатор.
typedef enum KbMod {
    KB_MOD_NONE,
    KB_MOD_SHIFT,
    KB_MOD_CTRL,
    KB_MOD_ALT,
    KB_MOD_LAST,
} KbMod;

typedef struct KbStroke {
                // KEY_ESCAPE и тд
    i32        keycode;
                // KbMod не сочетаются через |
    KbMod      mod;
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
    // опциональный общий контейнер биндов для отрисовки подсказок
    InputBinder *binder;
    // опциональный TTF с кириллицей для подписей/подсказок;
    // NULL → встроенный шрифт raylib (только ASCII, кириллица как ???)
    const char  *font_ttf;
    // размер загрузки шрифта при font_ttf != NULL (0 → значение по умолчанию)
    int         font_size;
} InputGamepadDrawerSetup;

InputGamepadDrawer *input_gp_new(InputGamepadDrawerSetup *setup);
void input_gp_free(InputGamepadDrawer *gp);
void input_gp_update(InputGamepadDrawer *gp);
/*void input_gp_gui_update(InputGamepadDrawer *kb);*/

void input_kb_pass_next_frame(InputKbMouseDrawer *kb);

/* РЕШЕНО (InputBinder): бинды вынесены в отдельный объект InputBinder,
общий для клавиатуры и геймпада. Сами Drawer'ы не сливаются — каждый
принимает InputBinder как опциональный параметр (в Setup или через
сеттер) и использует его только для отрисовки подсказок. Логика опроса
действия (input_binder_is_pressed) независима от устройства: одно
действие может быть привязано и к клавише, и к кнопке геймпада.

Совместимость: старый путь input_kb_bind / input_kb_is_pressed /
Btn.binds сохранён и работает параллельно. */

// Нет привязки к кнопке геймпада.
enum { GP_BUTTON_NONE = -1, };

// Привязка к кнопке геймпада.
typedef struct GpStroke {
    int button;   // GAMEPAD_BUTTON_* либо GP_BUTTON_NONE
    int gamepad;  // индекс геймпада (обычно 0)
} GpStroke;

// Абстрактное действие: подсказка + опциональные привязки к устройствам.
typedef struct InputAction {
    // подсказка; если NULL — берётся из get_msg
    const char *msg;
    const char *(*get_msg)(void *udata);
    void       *udata;
    // клавиатурная привязка (keycode == KEY_NULL → нет)
    KbStroke    kb;
    // геймпадная привязка (button == GP_BUTTON_NONE → нет)
    GpStroke    gp;
    // зеркалировать активацию действия в оба drawer'а (kb и gp)
    bool        is_synced;
} InputAction;

// Дескриптор действия внутри InputBinder; -1 — ошибка.
typedef int InputActionId;

InputBinder  *input_binder_new(void);
void          input_binder_free(InputBinder *b);
InputActionId input_binder_add(InputBinder *b, InputAction a);
// true, если сработала любая из привязок действия (клавиатура ИЛИ геймпад)
bool          input_binder_is_pressed(InputBinder *b, InputActionId id);

// Доступ для Drawer'ов при отрисовке подсказок.
int                input_binder_count(InputBinder *b);
const InputAction *input_binder_get(InputBinder *b, InputActionId id);

// Привязать общий контейнер биндов к Drawer'у после создания.
void input_kb_set_binder(InputKbMouseDrawer *kb, InputBinder *b);
void input_gp_set_binder(InputGamepadDrawer *gp, InputBinder *b);
