// vim: fdm=marker
#include "koh_input.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include "lua.h"
#include "raylib.h"

#include "koh_script.h"
#include "koh_common.h"

#define LAST_BUTTONS_NUM    20

/*
typedef enum InputAction {
    INP_ACT_is_up              = 1,
    INP_ACT_is_down            = 2,
    INP_ACT_is_select          = 3,
    INP_ACT_is_forward         = 4,
    INP_ACT_is_backward        = 5,
    INP_ACT_is_rot_right       = 6,
    INP_ACT_is_rot_left        = 7,
    INP_ACT_is_accel           = 8,
    INP_ACT_is_fire            = 9,
    INP_ACT_stick_turret_x     = 10,
    INP_ACT_stick_turret_y     = 11,
    INP_ACT_stick_map_x        = 12,
    INP_ACT_stick_map_y        = 13,
    INP_ACT_camera_mode_change = 14,
} InputAction;
*/

struct Action2String {
    int act;
    char *str;
};

struct Action2String input_action_2str[] = {
    {INP_ACT_player_menu        , "player_menu"},
    {INP_ACT_is_up              , "is_up"},
    {INP_ACT_is_down            , "is_down"},
    {INP_ACT_is_select          , "is_select"},
    {INP_ACT_is_forward         , "is_forward"},
    {INP_ACT_is_backward        , "is_backward"},
    {INP_ACT_is_rot_right       , "is_rot_right"},
    {INP_ACT_is_rot_left        , "is_rot_left"},
    {INP_ACT_is_accel           , "is_accel"},
    {INP_ACT_is_fire            , "is_fire"},
    {INP_ACT_stick_turret_x     , "stick_turret_x"},
    {INP_ACT_stick_turret_y     , "stick_turret_y"},
    {INP_ACT_stick_map_x        , "stick_map_x"},
    {INP_ACT_stick_map_y        , "stick_map_y"},
    {INP_ACT_camera_mode_change , "camera_mode_change"},
};

struct {
    InputActionBuf *arr;
    int            i, j, maxlen, len;
} circ_buf;

Input input;

// Gamepad mappings
struct {
    int up, down, right, left, select;
    // a_ axis
    int a_accel, a_fire;
    int a_map_x, a_map_y;
    int a_turret_x, a_turret_y;
    int change_camera_mode;
    int player_menu;
} gp;

// Активный геймпад
int active_gp = 0;

float last_axises[32] = {0, };
int last_buttons[LAST_BUTTONS_NUM] = {0, };

#define push_action_f(act, arg) \
    push_action(act, *((uint32_t*)&arg));

#define push_action_b(act, arg) \
    push_action(act, *((uint32_t*)&arg));

static void push_action(InputAction act, uint32_t arg) {
    circ_buf.arr[circ_buf.i] = (InputActionBuf) {
        .act = act,
        .arg = arg,
    };
    circ_buf.i = (circ_buf.i + 1) % circ_buf.maxlen;
    circ_buf.len++;
    if (circ_buf.len == circ_buf.maxlen) {
        circ_buf.len = circ_buf.maxlen - 1;
    }
}

int l_gamepad_init_mappings(lua_State *lua) {
    if (!lua_istable(lua, 1)) {
        printf("l_gamepad_init_mappings: argument is not a table\n");
        return 0;
    }

    // {{{

    lua_pushstring(lua, "a_map_x");
    lua_gettable(lua, -2);
    gp.a_map_x = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "a_map_y");
    lua_gettable(lua, -2);
    gp.a_map_y = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "a_turret_x");
    lua_gettable(lua, -2);
    gp.a_turret_x = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "a_turret_y");
    lua_gettable(lua, -2);
    gp.a_turret_y = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "select");
    lua_gettable(lua, -2);
    gp.select = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "a_accel");
    lua_gettable(lua, -2);
    gp.a_accel = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "a_fire");
    lua_gettable(lua, -2);
    gp.a_fire = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "left");
    lua_gettable(lua, -2);
    gp.left = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "right");
    lua_gettable(lua, -2);
    gp.right = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "down");
    lua_gettable(lua, -2);
    gp.down = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "up");
    lua_gettable(lua, -2);
    gp.up = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    lua_pushstring(lua, "change_camera_mode");
    lua_gettable(lua, -2);
    gp.change_camera_mode = lua_tonumber(lua, -1);
    lua_remove(lua, -1);

    // }}}
    
    return 0;
}

void gamepad_init_mappings(void) {
    gp.a_map_x = 2;
    gp.a_map_y = 3;
    gp.a_turret_x = 0;
    gp.a_turret_y = 1;
    // back
    //gp.select = 13;
    gp.select = 6;
    gp.a_accel = 5;
    gp.a_fire = 4;
    gp.left = 8;
    gp.right = 6;
    gp.down = 7;
    gp.up = 5;
    gp.change_camera_mode = 11;
}

bool is_forward() {
    if (IsGamepadAvailable(active_gp)) {
        int state = IsGamepadButtonDown(active_gp, gp.up);
        push_action_b(INP_ACT_is_forward, state);
        return state;
    } else {
        /*return IsKeyPressed(KEY_UP);*/
        return false;
    }
}

bool is_backward() {
    if (IsGamepadAvailable(active_gp)) {
        int state = IsGamepadButtonDown(active_gp, gp.down);
        push_action_b(INP_ACT_is_backward, state);
        return state;
    } else {
        /*return IsKeyPressed(KEY_DOWN);*/
        return false;
    }
}

bool is_up() {
    int pressed = false;
    if (IsGamepadAvailable(active_gp)) {
        pressed = IsGamepadButtonPressed(active_gp, gp.up);
        push_action_b(INP_ACT_is_up, pressed);
    }
    return pressed || IsKeyPressed(KEY_UP);
}

bool is_down() {
    int pressed = false;
    if (IsGamepadAvailable(active_gp)) {
        pressed = IsGamepadButtonPressed(active_gp, gp.down);
        push_action_b(INP_ACT_is_down, pressed);
    } 
   return pressed || IsKeyPressed(KEY_DOWN);
}

bool is_rot_right() {
    if (IsGamepadAvailable(active_gp)) {
        int state = IsGamepadButtonDown(active_gp, gp.right);
        push_action_b(INP_ACT_is_rot_right, state);
        return state;
    } else {
        /*return IsKeyPressed(KEY_RIGHT);*/
        return false;
    }
}

bool is_rot_left() {
    if (IsGamepadAvailable(active_gp)) {
        int state = IsGamepadButtonDown(active_gp, gp.left);
        push_action_b(INP_ACT_is_rot_left, state);
        return state;
    } else {
        /*return IsKeyPressed(KEY_LEFT);*/
        return false;
    }
}

float is_accel() {
    if (IsGamepadAvailable(active_gp)) {
        float axis = GetGamepadAxisMovement(active_gp, gp.a_accel);
        push_action_f(INP_ACT_is_accel, axis);
        return axis;
    } else {
        /*return IsKeyPressed(KEY_UP);*/
        return 0.;
    }
}

float is_fire() {
    if (IsGamepadAvailable(active_gp)) {
        float axis = GetGamepadAxisMovement(active_gp, gp.a_fire);
        push_action_f(INP_ACT_is_fire, axis);
        return axis;
    } else {
        // IsKeyPressed(KEY_UP);
    }
    return 0.;
}

bool is_select() {
    int pressed = false;
    if (IsGamepadAvailable(active_gp)) {
        pressed = IsGamepadButtonPressed(0, gp.select);
        push_action_b(INP_ACT_is_select, pressed);
    }
    return pressed || (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
}

float stick_turret_x(void) {
    if (IsGamepadAvailable(active_gp)) {
        float axis = GetGamepadAxisMovement(active_gp, gp.a_turret_x);
        push_action_b(INP_ACT_stick_turret_x, axis);
        return axis;
    } else {
        float v = 0.;
        if (IsKeyPressed(KEY_LEFT)) {
            v = -1.;
        } else if (IsKeyPressed(KEY_RIGHT)) {
            v = 1.;
        }
        return v;
    }
}

float stick_turret_y(void) {
    if (IsGamepadAvailable(active_gp)) {
        float axis = GetGamepadAxisMovement(active_gp, gp.a_turret_y);
        push_action_f(INP_ACT_stick_turret_y, axis);
        return axis;
    } else {
        float v = 0.;
        if (IsKeyPressed(KEY_UP)) {
            v = -1.;
        } else if (IsKeyPressed(KEY_DOWN)) {
            v = 1.;
        }
        return v;
    }
}

float stick_map_x(void) {
    if (IsGamepadAvailable(active_gp)) {
        float axis = GetGamepadAxisMovement(active_gp, gp.a_map_x);
        push_action_f(INP_ACT_stick_map_x, axis);
        return axis;
    } else {
        float v = 0.;
        if (IsKeyPressed(KEY_LEFT)) {
            v = -1.;
        } else if (IsKeyPressed(KEY_RIGHT)) {
            v = 1.;
        }
        return v;
    }
}

float stick_map_y(void) {
    if (IsGamepadAvailable(active_gp)) {
        float axis = GetGamepadAxisMovement(active_gp, gp.a_map_y);
        push_action_f(INP_ACT_stick_map_y, axis);
        return axis;
    } else {
        float v = 0.;
        if (IsKeyPressed(KEY_UP)) {
            v = -1.;
        } else if (IsKeyPressed(KEY_DOWN)) {
            v = 1.;
        }
        return v;
    }
}

void fill_last_axixes(void) {
    for(int i = 0; i < GetGamepadAxisCount(0); i++) {
        last_axises[i] = GetGamepadAxisMovement(0, i);
    }
}

bool player_menu(void) {
    if (IsGamepadAvailable(active_gp)) {
        int state = IsGamepadButtonPressed(active_gp, gp.player_menu);
        push_action_b(INP_ACT_player_menu, state);
        return state;
    } else {
        // TODO: Как проверить, что клавиша уже не используется?
        if (IsKeyPressed(KEY_Q)) {
            return true;
        }
    }
    return false;
}

bool camera_mode_change(void) {
    if (IsGamepadAvailable(active_gp)) {
        int state = IsGamepadButtonPressed(active_gp, gp.change_camera_mode);
        push_action_b(INP_ACT_camera_mode_change, state);
        return state;
    } else {
        if (IsKeyPressed(KEY_C)) {
            return true;
        }
    }
    return false;
}

#if defined(PLATFORM_WEB)
int l_get_browser_name(lua_State *lua) {
    char *browser_name = 
        emscripten_run_script_string("getBrowserName()");
    printf("l_get_browser_name: %s\n", browser_name);
    lua_pushstring(lua, browser_name);
    free(browser_name);
    return 1;
}
#endif

void input_init(void) {
    printf("input_init\n");
    input = (Input) {
        .is_up = is_up,
        .is_down = is_down,
        .is_forward = is_forward,
        .is_backward = is_backward,
        .is_rot_left = is_rot_left,
        .is_rot_right = is_rot_right,
        .is_accel = is_accel,
        .is_fire = is_fire,
        .is_select = is_select,
        .stick_turret_x = stick_turret_x,
        .stick_turret_y = stick_turret_y,
        .stick_map_x = stick_map_x,
        .stick_map_y = stick_map_y,
        .camera_mode_change = camera_mode_change,
        .is_player_menu = player_menu,
    };

    gamepad_init_mappings();

#if defined(PLATFORM_WEB)
    register_function(
        l_get_browser_name,
        "get_browser_name",
        "Получить название текущего веб-браузера"
    );
#endif

    register_function(
        l_gamepad_init_mappings,
        "gamepad_init_mappings",
        "Загрузить модель кнопок геймпада"
    );

    fill_last_axixes();

    memset(&circ_buf, 0, sizeof(circ_buf));
    circ_buf.maxlen = 30;
    circ_buf.arr = calloc(circ_buf.maxlen, sizeof(circ_buf.arr[0]));
}

void input_shutdown() {
    printf("input_shutdown\n");
    free(circ_buf.arr);
}

void input_gp_print_changed_axises(void) {
    for(int i = 0; i < GetGamepadAxisCount(active_gp); i++) {
        float value = GetGamepadAxisMovement(active_gp, i);
        if (last_axises[i] != value) {
            printf("axis %d = %f\n", i, value);
            last_axises[i] = value;
        }
    }
}

void input_gp_print_changed_buttons(void) {
    for(int i = 0; i < LAST_BUTTONS_NUM; i++) {
        int pressed = IsGamepadButtonUp(active_gp, i);
        if (pressed != last_buttons[i]) {
            printf("gamepad button %d pressed\n", i);
            last_buttons[i] = pressed;
        }
    }
}

bool input_buffer_get(InputActionBuf *action, int index) {
    assert(index >= 0);
    assert(action);
    if (index >= circ_buf.maxlen) return true;

    int i_part = circ_buf.maxlen - circ_buf.i;
    if (index < i_part)
        *action = circ_buf.arr[circ_buf.i + index];
    else
        *action = circ_buf.arr[circ_buf.j + (index - i_part)];

    return false;
}

const char *input_action2str(uint32_t action) {
    if (action > INPUT_ACT_MAX_VALUE)
        return NULL;

    int low = 0;
    int high = sizeof(input_action_2str) / sizeof(struct Action2String);

    while (low <= high) {
        int mid = (low + high) / 2;
        int value = input_action_2str[mid].act;

        if (value < action)
            low = mid + 1;
        else if (value > action)
            high = mid - 1;
        else
            return input_action_2str[mid].str;
    }

    return NULL;
}

bool input_gamepad_check() {
    return IsGamepadAvailable(active_gp);
}

