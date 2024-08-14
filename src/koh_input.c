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

struct Btn {
    char *lbl;
    int  keycode, dx, dy, sx;
    bool pressed;
};

struct BtnRow {
    int dx, dy, gap;
    struct Btn *btns;
};

bool koh_verbose_input = false;
static float scale_mouse = 0.10;

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
    Resource        reslist;
    RenderTexture2D rt;
    Texture2D       tex_mouse, tex_mouse_lb,
                    tex_mouse_rb, tex_mouse_wheel;

    Color           color_text, color_btn_pressed, color_btn_unpressed;
    int             font_size, line_thick, btn_width;
    Vector2         kb_size;
} InputKbMouseDrawer;

void input_kb_free(InputKbMouseDrawer *kb) {
    assert(kb);
    res_unload_all(&kb->reslist);
    free(kb);
}

static Vector2 kb_size(int btn_width) {
    // {{{
    int x_zero = 0, y_zero = 0;
    int x = x_zero, y = y_zero;
    int x_max = 0, y_max = 0;

    for (int i = 0; i < sizeof(btn_rows) / sizeof(btn_rows[0]); i++) {
        for (int j = 0; btn_rows[i].btns[j].lbl; j++) {
            struct Btn *btn = &btn_rows[i].btns[j];

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

typedef void (*BtnIterCb)(
            struct Btn *btn, 
            int x, int y, 
            int w, int h, 
            int btn_width, void *user_data
        );

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

InputKbMouseDrawer *input_kb_new(struct InputKbMouseDrawerSetup *setup) {
    assert(setup);
    assert(setup->btn_width > 0);
    InputKbMouseDrawer *kbm = calloc(1, sizeof(*kbm));
    assert(kbm);

    Resource *rl = &kbm->reslist;

    // TODO: Как и где хранить текстуры если вынести t80_input_kb.c в
    // отдельный проект? Как собирать ресурсы и не делать этого повторно
    // каждый раз при запуске проекта?

    SetTraceLogLevel(LOG_ERROR);
    kbm->tex_mouse = res_tex_load(rl, "assets/gfx/mouse/mouse.png");
    kbm->tex_mouse_rb = res_tex_load(rl, "assets/gfx/mouse/rb.png");
    kbm->tex_mouse_lb = res_tex_load(rl, "assets/gfx/mouse/lb.png");
    kbm->tex_mouse_wheel = res_tex_load(rl, "assets/gfx/mouse/wheel.png");

    kbm->btn_width = setup->btn_width;
    Vector2 size = kb_size(setup->btn_width);
    kbm->kb_size = size;
    size.x += kbm->tex_mouse.width * scale_mouse;

    kbm->rt = res_tex_load_rt(rl, size.x, size.y);
    SetTraceLogLevel(LOG_INFO);

    kbm->color_text = BLACK;
    kbm->color_btn_pressed = RED;
    kbm->color_btn_unpressed = BLUE;

    if (setup->btn_width >= 70) {
        kbm->font_size = 23;
        scale_mouse = 0.10;
    } else {
        kbm->font_size = 20;
        scale_mouse = 0.08;
    }

    kbm->line_thick = 3;

    if (koh_verbose_input)
        trace("input_kb_new: btn_width %d\n", setup->btn_width);

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
    /*
    trace(
        "input_kb_gui_update: kb->kb_size.y %f, kb->tex_mouse.height %d\n",
        kb->kb_size.y, kb->tex_mouse.height
    );
    */

    // Вручную подобранное смещение для scale_mouse = 0.95
    float manual_shift = 0;

    // /*
    /*if (kb->btn_width == 70 && scale_mouse - 0.95 < FLT_EPSILON) {*/
    /*if (kb->btn_width == 70 && scale_mouse - 0.95 < FLT_EPSILON) {*/
    if (kb->btn_width >= 70) {

        /*
        if (koh_verbose_input) {
            trace(
                "input_kb_gui_update: manual_shift %f\n",
                manual_shift
            );
        }
        */

        manual_shift = 200.f;
    } else {
        manual_shift = 150.f;
    }
    // */

    /*manual_shift = 200.f;*/

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
    /*ImGuiWindowFlags wnd_flags = 0;*/
    igBegin("input - keyboard & mouse", &wnd_open, wnd_flags);
    rlImGuiImageRenderTexture(&kb->rt);
    igEnd();
}

struct InputGamepadDrawer {
    Resource                reslist;
    Texture2D               tex_xbox;
    RenderTexture2D         rt;    
    int                     active_gp;
    float                   scale; // масштаб рисования картинки геймпада
};

InputGamepadDrawer *input_gp_new(InputGamepadDrawerSetup *setup) {
    assert(setup);
    assert(setup->scale > 0.);
    assert(setup->scale < 10.);

    struct InputGamepadDrawer *gp = calloc(1, sizeof(*gp));
    assert(gp);

    gp->scale = setup->scale;

    Resource *rl = &gp->reslist;
    SetTraceLogLevel(LOG_ERROR);
    gp->tex_xbox = res_tex_load(rl, "assets/gfx/xbox.png");
    gp->rt = res_tex_load_rt(
        rl, gp->tex_xbox.width * gp->scale, gp->tex_xbox.height * gp->scale
    );
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
    res_unload_all(&gp->reslist);
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

    int gamepad = gp->active_gp;

    // Draw buttons: xbox home
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE)) {
        DrawCircle(394, 89, 19, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_MIDDLE\n");
    }

    // Draw buttons: basic
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
        DrawCircle(436, 150, 9, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_MIDDLE_RIGHT\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT)) {
        DrawCircle(352, 150, 9, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_MIDDLE_LEFT\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) {
        DrawCircle(501, 151, 15, BLUE);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_RIGHT_FACE_LEFT\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
        DrawCircle(536, 187, 15, LIME);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_RIGHT_FACE_DOWN\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
        DrawCircle(572, 151, 15, MAROON);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_RIGHT_FACE_RIGHT\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP)) {
        DrawCircle(536, 115, 15, GOLD);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_RIGHT_FACE_UP\n");
    }

    // Draw buttons: d-pad
    DrawRectangle(317, 202, 19, 71, BLACK);
    DrawRectangle(293, 228, 69, 19, BLACK);

    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
        DrawRectangle(317, 202, 19, 26, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_LEFT_FACE_UP\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
        DrawRectangle(317, 202 + 45, 19, 26, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_LEFT_FACE_DOWN\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
        DrawRectangle(292, 228, 25, 19, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_LEFT_FACE_LEFT\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
        DrawRectangle(292 + 44, 228, 26, 19, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_LEFT_FACE_RIGHT\n");
    }

    // Draw buttons: left-right back
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) {
        DrawCircle(259, 61, 20, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_LEFT_TRIGGER_1\n");
    }
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
        DrawCircle(536, 61, 20, RED);
        if (koh_verbose_input)
            trace("input_gp_update: GAMEPAD_BUTTON_RIGHT_TRIGGER_1\n");
    }

    // Draw axis: left joystick

    Color leftGamepadColor = BLACK;
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_THUMB)) leftGamepadColor = RED;
    DrawCircle(259, 152, 39, BLACK);
    DrawCircle(259, 152, 34, LIGHTGRAY);
    DrawCircle(259 + (int)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X)*20),
               152 + (int)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y)*20), 25, leftGamepadColor);

    // Draw axis: right joystick
    Color rightGamepadColor = BLACK;
    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_THUMB)) rightGamepadColor = RED;
    DrawCircle(461, 237, 38, BLACK);
    DrawCircle(461, 237, 33, LIGHTGRAY);
    DrawCircle(461 + (int)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X)*20),
               237 + (int)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y)*20), 25, rightGamepadColor);

    // Draw axis: left-right triggers
    DrawRectangle(170, 30, 15, 70, GRAY);
    DrawRectangle(604, 30, 15, 70, GRAY);

    float (*axis_movement)(int gamepad, int axis) = GetGamepadAxisMovement;

    if (koh_verbose_input && axis_movement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER) != 0.)
        trace("input_gp_update: GAMEPAD_AXIS_LEFT_TRIGGER\n");
    DrawRectangle(
        170, 30, 15, 
        (int)(((1 + axis_movement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER))/2)*70),
        RED
    );

    DrawRectangle(
        604, 30, 15, 
        (int)(((1 + axis_movement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER))/2)*70),
        RED
    );
    if (koh_verbose_input && axis_movement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER))
        trace("input_gp_update: GAMEPAD_AXIS_RIGHT_TRIGGER\n");

    EndMode2D();
    EndTextureMode();

    bool wnd = true;
    int flags = ImGuiWindowFlags_AlwaysAutoResize;
    //int flags = ImGuiWindowFlags_NoResize;
    igBegin("input - gamepad", &wnd, flags);

    rlImGuiImageRenderTexture(&gp->rt);

    igEnd();
    // }}}
}


