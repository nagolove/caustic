// vim: set colorcolumn=85
// vim: fdm=marker
#include "koh_input.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "string.h"
#include "koh_logger.h"
#include <stdlib.h>
#include <assert.h>
#include "raylib.h"
#include "koh_routine.h"
#include <stdio.h>
#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_resource.h"
#include "koh_table.h"

typedef struct Btn {
    char *lbl;
         // KEY_ESCAPE и т.д.
    int  keycode,
         // смещения по x и y на размер кнопки
         dx, dy, 
         // сделать кнопку в два раза больше
         sx;
    bool pressed;

    KbBind bind;
} Btn;

struct BtnRow {
    int dx, dy, gap;
    struct Btn *btns;
};

bool koh_verbose_input = false;

static struct Btn row1[] = {
    // {{{
    { "ESC", KEY_ESCAPE, },
    { "F1", KEY_F1, .dx = 1, },
    { "F2", KEY_F2 },
    { "F3", KEY_F3 },
    { "F4", KEY_F4 },
    { "F5", KEY_F5, .dx = 1, },
    { "F6", KEY_F6 },
    { "F7", KEY_F7 },
    { "F8", KEY_F8 },
    { "F9", KEY_F9, .dx = 1, },
    { "F10", KEY_F10 },
    { "F11", KEY_F11 },
    { "F12", KEY_F12 },
    { NULL, 0},
    // }}}
};

static struct Btn row2[] = {
    // {{{
    { "`", KEY_GRAVE },
    { "1", KEY_ONE },
    { "2", KEY_TWO },
    { "3", KEY_THREE },
    { "4", KEY_FOUR },
    { "5", KEY_FIVE },
    { "6", KEY_SIX },
    { "7", KEY_SEVEN },
    { "8", KEY_EIGHT },
    { "9", KEY_NINE },
    { "0", KEY_ZERO },
    { "-", KEY_MINUS },
    { "=", KEY_EQUAL },
    { "BACKSPACE", KEY_BACKSPACE, .sx = 2 },
    { NULL, 0},
    // }}}
};

static struct Btn row3[] = {
    // {{{
    { "TAB", KEY_TAB },
    { "q", KEY_Q },
    { "w", KEY_W },
    { "e", KEY_E },
    { "r", KEY_R },
    { "t", KEY_T },
    { "y", KEY_Y },
    { "u", KEY_U },
    { "i", KEY_I },
    { "o", KEY_O },
    { "p", KEY_P },
    { "[", KEY_LEFT_BRACKET },
    { "]", KEY_RIGHT_BRACKET },
    { "\\", KEY_BACKSLASH },
    { NULL, 0},
    // }}}
};

static struct Btn row4[] = {
    // {{{
    { "CAPS", KEY_CAPS_LOCK },
    { "a", KEY_A },
    { "s", KEY_S },
    { "d", KEY_D },
    { "f", KEY_F },
    { "g", KEY_G },
    { "h", KEY_H },
    { "j", KEY_J },
    { "k", KEY_K },
    { "l", KEY_L },
    { ";", KEY_SEMICOLON },
    { "'", KEY_APOSTROPHE },
    { "ENTER", KEY_ENTER, .sx = 1, },
    { NULL, 0},
    // }}}
};

static struct Btn row5[] = {
    // {{{
    { "SHIFT", KEY_LEFT_SHIFT },
    { "z", KEY_Z },
    { "x", KEY_X },
    { "c", KEY_C },
    { "v", KEY_V },
    { "b", KEY_B },
    { "n", KEY_N },
    { "m", KEY_M },
    { ",", KEY_COMMA },
    { ".", KEY_PERIOD },
    { "/", KEY_SLASH },
    { "SHIFT", KEY_RIGHT_SHIFT },
    //{ "CTRL", KEY_RIGHT_CONTROL },
    { "UP", KEY_UP, .dx = 2, },
    { NULL, 0},
    // }}}
};

static struct Btn row6[] = {
    // {{{
    { "CTRL", KEY_LEFT_CONTROL },
    //{ "WIN",  KEY_W},
    { "ALT", KEY_LEFT_ALT, .dx = 1, },
    { "SPACE", KEY_SPACE, .sx = 6, },
    { "ALT", KEY_RIGHT_ALT,  },
    { "CTRL", KEY_RIGHT_CONTROL },
    { "LEFT", KEY_LEFT, .dx = 1 },
    { "DOWN", KEY_DOWN },
    { "RIGHT", KEY_RIGHT },
    { NULL, 0},
    // }}}
};

const int gap = 4;
static struct BtnRow btn_rows[] = {
    // {{{
    { .dx = 0, .dy = 1, .gap = gap, .btns = row1, },
    { .dx = 0, .dy = 1, .gap = gap, .btns = row2, },
    { .dx = 0, .dy = 1, .gap = gap, .btns = row3, },
    { .dx = 0, .dy = 1, .gap = gap, .btns = row4, },
    { .dx = 0, .dy = 1, .gap = gap, .btns = row5, },
    { .dx = 0, .dy = 1, .gap = gap, .btns = row6, },
    // }}}
};

typedef struct InputKbMouseDrawer {
    f32             scale_mouse;
    ResList         *reslist;
    RenderTexture2D rt;
    Texture2D       tex_mouse, tex_mouse_lb,
                    tex_mouse_rb, tex_mouse_wheel;

    Color           color_text, color_btn_pressed, color_btn_unpressed;
    int             font_size, line_thick, btn_width;
    Vector2         kb_size;

    bool            is_advanched_mode;
                    // i32 -> Btn*
    HTable          *map_keycode2btn;
} InputKbMouseDrawer;

void input_kb_free(InputKbMouseDrawer *kb) {
    assert(kb);
    reslist_free(kb->reslist);
    htable_free(kb->map_keycode2btn);
    //res_unload_all(&kb->reslist, false);
    free(kb);
}

static Vector2 kb_size(int btn_width) {
    // {{{
    int x_zero = 0, y_zero = 0;
    int x = x_zero, y = y_zero;
    int x_max = 0, y_max = 0;

    for (int i = 0; i < sizeof(btn_rows) / sizeof(btn_rows[0]); i++) {
        for (int j = 0; btn_rows[i].btns[j].lbl; j++) {
            const struct Btn *btn = &btn_rows[i].btns[j];

            x += (btn_width + gap) * (btn->dx + 1);
            int w = btn_width + btn_width * btn->sx;

            if (x + w > x_max)
                x_max = x + w;
            if (y + btn_width > y_max)
                y_max = y + btn_width;

            x += (btn_width + gap) * btn->sx;
            
        }
        y += btn_rows[i].dy * btn_width + btn_rows[i].gap;
        x = x_zero;
    }

    return (Vector2) { .x = x_max, .y = y_max };
    // }}}
}

typedef void (*BtnIterCb)(Btn *btn, int x, int y, int w, int h, int btn_width, void *user_data);

static void kb_each(
    int x_zero, int y_zero, int btn_width,
    BtnIterCb iter,
    void *user_data
) {
    // {{{
    assert(iter);
    x_zero -= btn_width;
    int x = x_zero, y = y_zero;

    for (int i = 0; i < sizeof(btn_rows) / sizeof(btn_rows[0]); i++) {
        for (int j = 0; btn_rows[i].btns[j].lbl; j++) {
            struct Btn *btn = &btn_rows[i].btns[j];

            x += (btn_width + gap) * (btn->dx + 1);
            int w = btn_width + btn_width * btn->sx;

            iter(btn, x, y, w, btn_width, btn_width, user_data);
            x += (btn_width + gap) * btn->sx;
            
        }
        y += btn_rows[i].dy * btn_width + btn_rows[i].gap;
        x = x_zero;
    }
    // }}}
}

void iter_draw(
    struct Btn *btn ,int x, int y, int w, int h, int btn_width, void *user_data
) {
    InputKbMouseDrawer *ctx = user_data;
    Color color_btn = btn->pressed ? 
        ctx->color_btn_pressed : ctx->color_btn_unpressed;

    DrawRectangle(x, y, w, btn_width, color_btn);
    DrawRectangleLinesEx((Rectangle) {
        .x = x,
        .y = y,
        .width = w,
        .height = btn_width,
    }, ctx->line_thick, BLACK);

    // XXX: Странное сравнение
    if (btn->bind.keycode == btn->keycode) {
        i32 _w = w,
            _h = btn_width;
        //DrawRectangle(x, y, w, btn_width, color_btn);
        Vector2 v1 = { x, y + _h },
                v2 = { x + _w, y },
                v3 = { x + _w, y + _h };
        DrawTriangle(v1, v2, v3, GREEN);
    }

    char msg[32] = {};
    sprintf(msg, "%s", btn->lbl);
    Vector2 m = MeasureTextEx(GetFontDefault(), msg, ctx->font_size, 0.);
    int dx = (w - m.x) / 2.;
    int dy = (btn_width - m.y) / 2.;
    DrawText(msg, x + dx, y + dy, ctx->font_size, ctx->color_text);
}

void iter_update(
    struct Btn *btn ,int x, int y, int w, int h, int btn_width, void *ud
) {
    btn->pressed = IsKeyDown(btn->keycode);
}

void iter_map(
    struct Btn *btn ,int x, int y, int w, int h, int btn_width, void *ud
) {
    InputKbMouseDrawer *kbm = ud;
    assert(kbm);
    assert(kbm->map_keycode2btn);
    htable_add(
        kbm->map_keycode2btn,
        &btn->keycode, sizeof(i32), 
        btn, sizeof(Btn*)
    );
}

// Инициализация системы связанной с размером кнопок и шрифта
static void input_kb_init_btn_width(InputKbMouseDrawer *kbm, i32 btn_width) {
    kbm->btn_width = btn_width;
    Vector2 size = kb_size(btn_width);
    kbm->kb_size = size;

    /*
    if (btn_width >= 70) {
        //kbm->font_size = 23 + (btn_width - 70) * 1.05;
        kbm->font_size = 23 + (btn_width - 70) * 1.01;
        scale_mouse = 0.10;
    } else {
        kbm->font_size = 20;
        scale_mouse = 0.08;
    }
    */

    const i32 btn_width_min = 49;
    assert(btn_width > btn_width_min);
    // XXX: Магическая формула
    kbm->font_size = 10 + (btn_width - btn_width_min) * 0.5;

    ResList *rl = kbm->reslist;
    kbm->kb_size.x += kbm->tex_mouse.width * kbm->scale_mouse;
    kbm->rt = reslist_load_rt(rl, kbm->kb_size.x, kbm->kb_size.y);
}

InputKbMouseDrawer *input_kb_new(struct InputKbMouseDrawerSetup *setup) {
    assert(setup);
    assert(setup->btn_width > 0);
    InputKbMouseDrawer *kbm = calloc(1, sizeof(*kbm));
    assert(kbm);

    ResList *rl = kbm->reslist = reslist_new();
    kbm->map_keycode2btn = htable_new(NULL);

    // TODO: Как и где хранить текстуры если вынести t80_input_kb.c в
    // отдельный проект? Как собирать ресурсы и не делать этого повторно
    // каждый раз при запуске проекта?

    SetTraceLogLevel(LOG_ERROR);
    kbm->tex_mouse = reslist_load_tex(rl, "assets/gfx/mouse/mouse.png");
    kbm->tex_mouse_rb = reslist_load_tex(rl, "assets/gfx/mouse/rb.png");
    kbm->tex_mouse_lb = reslist_load_tex(rl, "assets/gfx/mouse/lb.png");
    kbm->tex_mouse_wheel = reslist_load_tex(rl, "assets/gfx/mouse/wheel.png");

    input_kb_init_btn_width(kbm, setup->btn_width);
    SetTraceLogLevel(LOG_INFO);

    kbm->color_text = BLACK;
    kbm->color_btn_pressed = RED;
    kbm->color_btn_unpressed = BLUE;

    kbm->line_thick = 3;

    if (koh_verbose_input)
        trace("input_kb_new: btn_width %d\n", setup->btn_width);

    //kbm->scale = 1.f;
    kbm->is_advanched_mode = false;
    kbm->scale_mouse = 0.10;

    /*
    typedef struct MapCtx {
        InputKbMouseDrawer  *kb;
        i32                 keycode;
    } MapCtx;
    */

    kb_each(0, 0, kbm->btn_width, iter_map, kbm);

    return kbm;
}


void input_kb_update(InputKbMouseDrawer *kb) {
    assert(kb);
    kb_each(0, 0, kb->btn_width, iter_update, kb);
}

void input_kb_gui_update(InputKbMouseDrawer *kb) {
    assert(kb);
    BeginTextureMode(kb->rt);
    ClearBackground(GRAY);
    kb_each(0, 0, kb->btn_width, iter_draw, kb);

    const Color color = WHITE;

    // Вручную подобранное смещение для scale_mouse = 0.95
    float manual_shift = 0;

    if (kb->btn_width >= 70) {
        manual_shift = 200.f;
    } else {
        manual_shift = 150.f;
    }

    const f32 scale_mouse = kb->scale_mouse;
    const Vector2 pos = {
        kb->kb_size.x - manual_shift,
        (kb->kb_size.y - kb->tex_mouse.height * scale_mouse) / 2.,
    };
    DrawTextureEx(kb->tex_mouse, pos, 0., scale_mouse, color);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        DrawTextureEx(kb->tex_mouse_lb, pos, 0., scale_mouse, color);
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        DrawTextureEx(kb->tex_mouse_rb, pos, 0., scale_mouse, color);
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        DrawTextureEx(kb->tex_mouse_wheel, pos, 0., scale_mouse, color);

    EndTextureMode();

    bool wnd_open = true;
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;
    igBegin("input - keyboard & mouse", &wnd_open, wnd_flags);
    rlImGuiImageRenderTexture(&kb->rt);

    if (igIsItemClicked(ImGuiMouseButton_Right))
        kb->is_advanched_mode = !kb->is_advanched_mode;

    if (kb->is_advanched_mode) {
        igPushItemWidth(100);
        static i32 btn_width = 0;
        btn_width = kb->btn_width;
        if (igSliderInt("scale", &btn_width, 50, 200, "%d", 0)) {
            input_kb_init_btn_width(kb, btn_width);
        }
        igPopItemWidth();
    }

    igEnd();
}

struct InputGamepadDrawer {
    ResList                 *reslist;
    Texture2D               tex_xbox;
    RenderTexture2D         rt;    
    int                     active_gp;
    float                   scale; // масштаб рисования картинки геймпада
    Font                    fnt; // подсказки с названиями кнопок
    bool                    // писать названия активных клавиш
                            is_draw_labels, 
                            // показывать опции
                            is_advanched_mode; 
};

static void gp_load_tex(InputGamepadDrawer *gp) {
    assert(gp);
    ResList *rl = gp->reslist;
    assert(rl);
    gp->tex_xbox = reslist_load_tex(rl, "assets/gfx/xbox.png");
    float ws = gp->tex_xbox.width * gp->scale,
          hs = gp->tex_xbox.height * gp->scale;
    gp->rt = reslist_load_rt(rl, ws, hs);
}

InputGamepadDrawer *input_gp_new(InputGamepadDrawerSetup *setup) {
    assert(setup);
    assert(setup->scale > 0.);
    assert(setup->scale < 10.);

    struct InputGamepadDrawer *gp = calloc(1, sizeof(*gp));
    assert(gp);

    gp->scale = setup->scale;
    gp->is_advanched_mode = false;
    gp->is_draw_labels = true;

    ResList *rl = gp->reslist = reslist_new();
    SetTraceLogLevel(LOG_ERROR);

    gp_load_tex(gp);

    //gp->fnt = GetFontDefault();
    gp->fnt = reslist_load_font_dlft(rl);

    SetTraceLogLevel(LOG_INFO);

    if (koh_verbose_input) {
        int i = 10;
        while (i >= 0) {
            trace(
                "input_gp_new: gamepad %d was detected %s, name '%s' \n",
                i,
                IsGamepadAvailable(i) ? "true" : "false", GetGamepadName(i)
            );
            const char *gp_name = GetGamepadName(i);
            if (strstr(gp_name, "X-Box")) {
                gp->active_gp = i;
            }
            i--;
        }
    }
    trace("input_gp_new: active_gp %d\n", gp->active_gp);

    return gp;
}

void input_gp_free(InputGamepadDrawer *gp) {
    assert(gp);
    /*res_unload_all(&gp->reslist, false);*/
    reslist_free(gp->reslist);
    free(gp);
}

static const char *gp_button2name[] = {
    [GAMEPAD_BUTTON_UNKNOWN] = "UNKNOWN",
    [GAMEPAD_BUTTON_LEFT_FACE_UP] = "LEFT_FACE_UP",
    [GAMEPAD_BUTTON_LEFT_FACE_RIGHT] = "LEFT_FACE_RIGHT",
    [GAMEPAD_BUTTON_LEFT_FACE_DOWN] = "LEFT_FACE_DOWN",
    [GAMEPAD_BUTTON_LEFT_FACE_LEFT] = "LEFT_FACE_LEFT",
    [GAMEPAD_BUTTON_RIGHT_FACE_UP] = "RIGHT_FACE_UP",
    [GAMEPAD_BUTTON_RIGHT_FACE_RIGHT] = "RIGHT_FACE_RIGHT",
    [GAMEPAD_BUTTON_RIGHT_FACE_DOWN] = "RIGHT_FACE_DOWN",
    [GAMEPAD_BUTTON_RIGHT_FACE_LEFT] = "RIGHT_FACE_LEFT",
    [GAMEPAD_BUTTON_LEFT_TRIGGER_1] = "LEFT_TRIGGER_1",
    [GAMEPAD_BUTTON_LEFT_TRIGGER_2] = "LEFT_TRIGGER_2",
    [GAMEPAD_BUTTON_RIGHT_TRIGGER_1] = "RIGHT_TRIGGER_1",
    [GAMEPAD_BUTTON_RIGHT_TRIGGER_2] = "RIGHT_TRIGGER_2",
    [GAMEPAD_BUTTON_MIDDLE_LEFT] = "MIDDLE_LEFT",
    [GAMEPAD_BUTTON_MIDDLE] = "MIDDLE",
    [GAMEPAD_BUTTON_MIDDLE_RIGHT] = "MIDDLE_RIGHT",
    [GAMEPAD_BUTTON_LEFT_THUMB] = "LEFT_THUMB",
    [GAMEPAD_BUTTON_RIGHT_THUMB] = "RIGHT_THUMB",
};

static const char *gp_axis2name[] = {
    [GAMEPAD_AXIS_LEFT_X] = "AXIS_LEFT_X",
    [GAMEPAD_AXIS_LEFT_Y] = "AXIS_LEFT_Y",
    [GAMEPAD_AXIS_RIGHT_X] = "AXIS_RIGHT_X",
    [GAMEPAD_AXIS_RIGHT_Y] = "AXIS_RIGHT_Y",
    [GAMEPAD_AXIS_LEFT_TRIGGER] = "AXIS_LEFT_TRIGGER",
    [GAMEPAD_AXIS_RIGHT_TRIGGER] = "AXIS_RIGHT_TRIGGER",
};

static float (*axis_move)(int gamepad, int axis) = GetGamepadAxisMovement;
static const f32 fnt_size = 20;

static void draw_triggers(InputGamepadDrawer *gp) {
    DrawRectangle(170, 30, 15, 70, LIGHTGRAY);
    DrawRectangle(604, 30, 15, 70, LIGHTGRAY);

    f32 l = ((1 + axis_move(gp->active_gp, GAMEPAD_AXIS_LEFT_TRIGGER))/2)*70,
        r = ((1 + axis_move(gp->active_gp, GAMEPAD_AXIS_RIGHT_TRIGGER))/2)*70;

    Vector2 p1 = {170, 30},
            p2 = {604, 30};
    DrawRectangleV(p1, (Vector2) {15, l}, RED);
    DrawRectangleV(p2, (Vector2) {15, r}, RED);

    if (l != 0.)
        DrawTextEx(
            gp->fnt, "AXIS_LEFT_TRIGGER", 
            p1, fnt_size, fnt_size / 5., GREEN
    );
    if (r != 0.)
        DrawTextEx(
            gp->fnt, "AXIS_RIGHT_TRIGGER", 
            p2, fnt_size, fnt_size / 5., GREEN
        );
}

static void draw_text(
    InputGamepadDrawer *gp, const char *text, Vector2 position, 
    float fontSize, Color tint
) {

    if (gp->is_draw_labels) 
        DrawTextEx(gp->fnt, text, position, fontSize, fontSize / 5., tint);
}

static void draw_stick(
    InputGamepadDrawer *gp, Vector2 p, i32 axis_base, i32 thumb
) {
    const i32 gamepad = gp->active_gp;

    Color color = BLACK;
    if (IsGamepadButtonDown(gamepad, thumb)) {
        color = RED;
    }

    f32 l = axis_move(gamepad, axis_base),
        r = axis_move(gamepad, axis_base + 1);

    DrawCircle(p.x + l * 20, p.y + r * 20, 25, color);

    if (fabs(l) >= 0.01) {
        const char *lbl = gp_axis2name[axis_base];
        draw_text(gp, lbl, p, fnt_size, GREEN);
    }
    if (fabs(r) >= 0.01) {
        const char *lbl = gp_axis2name[axis_base + 1];
        Vector2 newp = Vector2Add(p, (Vector2){ 0, fnt_size});
        draw_text(gp, lbl, newp, fnt_size, GREEN);
    }
}

void input_gp_update(InputGamepadDrawer *gp) {
    // {{{
    assert(gp);

    BeginTextureMode(gp->rt);
    BeginMode2D((Camera2D) {
        .offset = Vector2Zero(),
        .target = Vector2Zero(),
        .rotation = 0.,
        .zoom = gp->scale,
    });

    ClearBackground(GRAY);
    DrawTexture(gp->tex_xbox, 0, 0, DARKGRAY);

    const i32 gamepad = gp->active_gp;

    // массивы {{{
    static const i32 btns[] = {
        GAMEPAD_BUTTON_MIDDLE,
        GAMEPAD_BUTTON_MIDDLE_RIGHT,
        GAMEPAD_BUTTON_MIDDLE_LEFT,
        GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
        GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
        GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
        GAMEPAD_BUTTON_RIGHT_FACE_UP,

        GAMEPAD_BUTTON_LEFT_FACE_UP,
        GAMEPAD_BUTTON_LEFT_FACE_DOWN,
        GAMEPAD_BUTTON_LEFT_FACE_LEFT,
        GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
        GAMEPAD_BUTTON_LEFT_TRIGGER_1,
        GAMEPAD_BUTTON_RIGHT_TRIGGER_1,

    };
    i32 btns_num = sizeof(btns) / sizeof(btns[0]);

    static const struct {
        Vector2 p, wh;
        f32     radius;
        bool    has_wh;
    } desc[] = {
        [GAMEPAD_BUTTON_MIDDLE] = {
            { 394, 89,},
            .radius = 19,
        },
        [GAMEPAD_BUTTON_MIDDLE_RIGHT] = {
            {436, 150},
            .radius = 19,
        },
        [GAMEPAD_BUTTON_MIDDLE_LEFT] = {
            {352, 150},
            .radius = 19,
        },
        [GAMEPAD_BUTTON_RIGHT_FACE_LEFT] = {
            {501, 151},
            .radius = 19,
        },
        [GAMEPAD_BUTTON_RIGHT_FACE_DOWN] = {
            {536, 187},
            .radius = 19,
        },
        [GAMEPAD_BUTTON_RIGHT_FACE_RIGHT] = {
            {572, 151},
            .radius = 19,
        },
        [GAMEPAD_BUTTON_RIGHT_FACE_UP] = {
            {536, 115},
            .radius = 19,
        },

        [GAMEPAD_BUTTON_LEFT_FACE_UP] = {
            {317, 202},
            .has_wh = 1,
            .wh = {19, 26},
        },
        [GAMEPAD_BUTTON_LEFT_FACE_DOWN] = {
            {317, 202 + 45},
            .has_wh = 1,
            .wh = { 19, 26},
        },
        [GAMEPAD_BUTTON_LEFT_FACE_LEFT] = {
            {292, 228},
            .has_wh = 1,
            .wh = { 26, 19},
        },
        [GAMEPAD_BUTTON_LEFT_FACE_RIGHT] = {
            {292 + 44, 228},
            .has_wh = 1,
            .wh = { 26, 19},
        },
        [GAMEPAD_BUTTON_LEFT_TRIGGER_1] = {
            {259, 61},
            .radius = 20,
        },
        [GAMEPAD_BUTTON_RIGHT_TRIGGER_1]= {
            {536, 61},
            .radius = 20,
        }

    };
    // }}}

    // Draw buttons: d-pad
    DrawRectangle(317, 202, 19, 71, BLACK);
    DrawRectangle(293, 228, 69, 19, BLACK);

    // левый стик
    DrawCircle(259, 152, 39, BLACK);
    DrawCircle(259, 152, 34, LIGHTGRAY);
    // правый стик
    DrawCircle(461, 237, 38, BLACK);
    DrawCircle(461, 237, 33, LIGHTGRAY);

    for (i32 i = 0; i < btns_num; ++i) {
        i32 btn = btns[i];
        if (IsGamepadButtonDown(gamepad, btn)) {
            const Vector2 p = desc[btn].p;
            if (desc[btn].has_wh)
                DrawRectangleV(p, desc[btn].wh, RED);
            else
                DrawCircleV(p, desc[btn].radius, RED);
        }
    }

    draw_stick(
        gp, (Vector2) { .x = 259, .y = 152 },
        GAMEPAD_AXIS_LEFT_X, GAMEPAD_BUTTON_LEFT_THUMB
    );
    draw_stick(
        gp, (Vector2) { .x = 461, .y = 237 },
        GAMEPAD_AXIS_RIGHT_X, GAMEPAD_BUTTON_RIGHT_THUMB
    );

    draw_triggers(gp);

    // Рисовать подписи для активных кнопок
    for (i32 i = 0; i < btns_num; ++i) {
        i32 btn = btns[i];
        if (IsGamepadButtonDown(gamepad, btn)) {
            const char *name = gp_button2name[btn];
            draw_text(gp, name, desc[btn].p, fnt_size, GREEN);
        }
    }

    EndMode2D();
    EndTextureMode();

    bool wnd = true;
    int flags = ImGuiWindowFlags_AlwaysAutoResize;
    //int flags = ImGuiWindowFlags_NoResize;
    igBegin("input - gamepad", &wnd, flags);

    rlImGuiImageRenderTexture(&gp->rt);

    if (igIsItemClicked(ImGuiMouseButton_Right))
        gp->is_advanched_mode = !gp->is_advanched_mode;

    if (gp->is_advanched_mode) {
        igCheckbox("labels", &gp->is_draw_labels);
        igSameLine(0.f, 0.);

        igPushItemWidth(100);
        //igPushItemWidth(gp->tex_xbox.width * .5);
        if (igSliderFloat("scale", &gp->scale, 0.5, 2.5, "%.2f", 0)) {
            gp_load_tex(gp);
        }
        igPopItemWidth();
    }

    igEnd();
    // }}}
}

void input_kb_bind(InputKbMouseDrawer *kb, KbBind b) {
    assert(kb);
    assert(b.msg);

    i32 len = 0;
    Btn *btn = htable_get(kb->map_keycode2btn, &b.keycode, sizeof(i32), &len);
    if (btn) {
        assert(len == sizeof(void*));
        btn->bind = b;
    }
}
