// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_hotkey.h"

#include "koh_common.h"
#include "koh_console.h"
#include "koh_logger.h"
#include "koh_lua_tools.h"
#include "koh_script.h"
#include "lua.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HotkeyStorage *last_storage = NULL;

struct Key2Str {
    int key;
    char *kname;
} static key2str[] = {
    // {{{
    { KEY_NULL          , "NULL"},
    { KEY_APOSTROPHE    , "APOSTROPHE" },
    { KEY_COMMA         , "COMMA"},
    { KEY_MINUS         , "MINUS" },
    { KEY_PERIOD        , "PERIOD" },
    { KEY_SLASH         , "SLASH" },
    { KEY_ZERO          , "ZERO" },
    { KEY_ONE           , "ONE" },
    { KEY_TWO           , "TWO" },
    { KEY_THREE         , "THREE" },
    { KEY_FOUR          , "FOUR" },
    { KEY_FIVE          , "FIVE" },
    { KEY_SIX           , "SIX" },
    { KEY_SEVEN         , "SEVEN" },
    { KEY_EIGHT         , "EIGHT" },
    { KEY_NINE          , "NINE" },
    { KEY_SEMICOLON     , "SEMICOLON" },
    { KEY_EQUAL         , "EQUAL" },
    { KEY_A             , "A" },
    { KEY_B             , "B" },
    { KEY_C             , "C" },
    { KEY_D             , "D" },
    { KEY_E             , "E" },
    { KEY_F             , "F" },
    { KEY_G             , "G" },
    { KEY_H             , "H" },
    { KEY_I             , "I" },
    { KEY_J             , "J" },
    { KEY_K             , "K" },
    { KEY_L             , "L" },
    { KEY_M             , "M" },
    { KEY_N             , "N" },
    { KEY_O             , "O" },
    { KEY_P             , "P" },
    { KEY_Q             , "Q" },
    { KEY_R             , "R" },
    { KEY_S             , "S" },
    { KEY_T             , "T" },
    { KEY_U             , "U" },
    { KEY_V             , "V" },
    { KEY_W             , "W" },
    { KEY_X             , "X" },
    { KEY_Y             , "Y" },
    { KEY_Z             , "Z" },
    { KEY_LEFT_BRACKET  , "LBRACKET" },
    { KEY_BACKSLASH     , "BACKSLASH" },
    { KEY_RIGHT_BRACKET , "RBRACKET" },
    { KEY_GRAVE         , "GRAVE" },
    { KEY_SPACE         , "SPACE" },
    { KEY_ESCAPE        , "ESCAPE" },
    { KEY_ENTER         , "ENTER" },
    { KEY_TAB           , "TAB" },
    { KEY_BACKSPACE     , "BACKSPACE" },
    { KEY_INSERT        , "INSERT" },
    { KEY_DELETE        , "DELETE" },
    { KEY_RIGHT         , "RIGHT" },
    { KEY_LEFT          , "LEFT" },
    { KEY_DOWN          , "DOWN" },
    { KEY_UP            , "UP" },
    { KEY_PAGE_UP       , "PAGE_UP" },
    { KEY_PAGE_DOWN     , "PAGE_DOWN" },
    { KEY_HOME          , "HOME" },
    { KEY_END           , "END" },
    { KEY_CAPS_LOCK     , "CAPS_LOCK" },
    { KEY_SCROLL_LOCK   , "SCROLL_LOCK" },
    { KEY_NUM_LOCK      , "NUM_LOCK" },
    { KEY_PRINT_SCREEN  , "PRINT_SCREEN" },
    { KEY_PAUSE         , "PAUSE" },
    { KEY_F1            , "F1" },
    { KEY_F2            , "F2" },
    { KEY_F3            , "F3" },
    { KEY_F4            , "F4" },
    { KEY_F5            , "F5" },
    { KEY_F6            , "F6" },
    { KEY_F7            , "F7" },
    { KEY_F8            , "F8" },
    { KEY_F9            , "F9" },
    { KEY_F10           , "F10" },
    { KEY_F11           , "F11" },
    { KEY_F12           , "F12" },
    { KEY_LEFT_SHIFT    , "LSHIFT" },
    { KEY_LEFT_CONTROL  , "LCONTROL" },
    { KEY_LEFT_ALT      , "LALT" },
    { KEY_LEFT_SUPER    , "LSUPER" },
    { KEY_RIGHT_SHIFT   , "RSHIFT" },
    { KEY_RIGHT_CONTROL , "RCONTROL" },
    { KEY_RIGHT_ALT     , "RALT" },
    { KEY_RIGHT_SUPER   , "RSUPER" },
    { KEY_KB_MENU       , "KB_MENU" },
    { KEY_KP_0          , "KP_0" },
    { KEY_KP_1          , "KP_1" },
    { KEY_KP_2          , "KP_2" },
    { KEY_KP_3          , "KP_3" },
    { KEY_KP_4          , "KP_4" },
    { KEY_KP_5          , "KP_5" },
    { KEY_KP_6          , "KP_6" },
    { KEY_KP_7          , "KP_7" },
    { KEY_KP_8          , "KP_8" },
    { KEY_KP_9          , "KP_9" },
    { KEY_KP_DECIMAL    , "KP_DECIMAL" },
    { KEY_KP_DIVIDE     , "KP_DIVIDE" },
    { KEY_KP_MULTIPLY   , "KP_MULTIPLY" },
    { KEY_KP_SUBTRACT   , "KP_SUBTRACT" },
    { KEY_KP_ADD        , "KP_ADD" },
    { KEY_KP_ENTER      , "KP_ENTER" },
    { KEY_KP_EQUAL      , "KP_EQUAL" },
    { KEY_BACK          , "BACK" },
    { KEY_MENU          , "MENU" },
    { KEY_VOLUME_UP     , "VOLUME_UP" },
    { KEY_VOLUME_DOWN   , "VOLUME_DOWN" },
    { -1                , NULL },
    // }}}
};

const char *get_key2str(int key) {
    int i = 0;
    while(key2str[i].key != -1) {
        if (key == key2str[i].key) {
            return key2str[i].kname;
        }
        i++;
    }
    return NULL;
}

void hotkeys_enumerate(HotkeyStorage *storage) {
    assert(storage);

    int maxgroupnum = 0;
    for(int i = 0; i < storage->keysnum; i++) {
        Hotkey *hk = &storage->keys[i];
        if (hk->groups > maxgroupnum) {
            maxgroupnum = hk->groups;
        }
    }

    console_buf_write(
            "|modkey+modkey+key|groups bits|name|bool enabled|description|"
    );

    for(int i = 0; i < storage->keysnum; i++) {
        Hotkey *hk = &storage->keys[i];
        char name[82] = {0};
        sprintf(name, "\"%s\"", hk->name ? hk->name : "NULL");

        /*
        console_buf_write("[%16s] %8s %20s %6s - %s", 
                get_key2str(hk->combo.key),
                uint8t_to_bitstr(hk->groups),
                name,
                hk->enabled ? "true" : "false",
                hk->description ? hk->description : "NULL"
        );
        */

        int lead_zeros = (8 - maxgroupnum);
        char buf[128] = {0};
        snprintf(
            buf, sizeof(buf), 
            "[%%8s+%%8s+%%10s] %%%ds %%20s %%1s - %%s", maxgroupnum
        );
        //console_buf_write("[%8s+%8s+%10s] %8s %20s %1s - %s", 
        console_buf_write(buf, 
                get_key2str(hk->combo.mod),
                get_key2str(hk->combo.mod2),
                get_key2str(hk->combo.key),
                /*skip_lead_zeros(uint8t_to_bitstr(hk->groups)),*/
                uint8t_to_bitstr(hk->groups) + lead_zeros,
                name,
                hk->enabled ? "t" : "f",
                hk->description ? hk->description : "NULL"
        );

    }
}

int l_hotkeys_enumerate(lua_State *lua) {
    if (last_storage)
        hotkeys_enumerate(last_storage);
    return 0;
}

void hotkey_init(HotkeyStorage *storage) {
    assert(storage);
    lua_State *lua = sc_get_state();
    assert(lua);

    last_storage = storage;

    register_function(l_hotkeys_enumerate, "keys", "Клавиши управления");
}

void hotkey_register(HotkeyStorage *storage, Hotkey hk) {
    assert(storage);

    trace_push_group("hotkey_register");

    if (storage->keysnum + 1 == MAX_HOTKEYS) {
        traceg("hotkeys limit are reached\n");
        exit(EXIT_FAILURE);
    }

    if (!hk.name) {
        traceg("unnamed combination\n");
        exit(EXIT_FAILURE);
    }

    if (!hk.data)
        traceg("combination '%s' without custom data\n", hk.name);

    if (!hk.groups) {
        traceg("combination '%s' without group\n", hk.name);
    }

    if (!hk.func) {
        traceg("combination without func pointer\n");
        exit(EXIT_FAILURE);
    }
    if (!hk.description) {
        traceg("combination without description\n");
        exit(EXIT_FAILURE);
    }
    if (hk.combo.mode < HM_MODE_RESERVED_FIRST || 
        hk.combo.mode > HM_MODE_RESERVED_LAST) {
        traceg("combination without proper mode\n");
        exit(EXIT_FAILURE);
    }

    Hotkey existing = {0};
    if (hotkey_exists(storage, hk, &existing)) {
        traceg(
            "combination duplicated:"
            "'%s'[%s] overlapped existing '%s'[%s]\n",
            hk.name, hk.description, 
            existing.name, existing.description
        );
        exit(EXIT_FAILURE);
    }

    Hotkey with_strings = hk;
    with_strings.description = strdup(hk.description);
    with_strings.name = strdup(hk.name);
    storage->keys[storage->keysnum++] = with_strings;

    trace_pop_group();
}

void hotkey_shutdown(HotkeyStorage *storage) {
    assert(storage);
    for(int i = 0; i < storage->keysnum; i++) {
        Hotkey *hk = &storage->keys[i];
        free(hk->description);
        hk->description = NULL;
        free(hk->name);
        hk->name = NULL;
    }
}

//XXX: Если установлена пара хоткеев на 'a' и ctrl + 'a', то 'a' вызывается
//при нажатии ctrl + 'a'
void hotkey_process(HotkeyStorage *storage) {
    for(int i = 0; i < storage->keysnum; i++) {
        Hotkey *hk = &storage->keys[i];

        if (!hk->enabled) continue;

        bool (*handler)(int key) = NULL;
        int mode = hk->combo.mode;

        if (mode == HM_MODE_ISKEYDOWN) {
            handler = IsKeyDown;
        } else if (mode == HM_MODE_ISKEYPRESSED) {
            handler = IsKeyPressed;
        } else {
            trace("hotkey_process: unknowd mode\n");
        }

        /*
        bool mod = hk->combo.mod ? IsKeyDown(hk->combo.mod) : true; 
        bool mod2 = hk->combo.mod2 ? IsKeyDown(hk->combo.mod2) : true;

        if (mod && mod2) {
            if (handler && handler(hk->combo.key) && hk->func)
                hk->func(hk);
        }
        */

        bool mod_down = IsKeyDown(hk->combo.mod);
        bool mod2_down = IsKeyDown(hk->combo.mod2);
        bool is_pressed = handler && handler(hk->combo.key) && hk->func;
        int mod = hk->combo.mod, mod2 = hk->combo.mod2;

        if (mod) {
            if (mod_down && is_pressed) hk->func(hk);
        } else if (mod2) {
            if (mod_down && mod2_down && is_pressed) hk->func(hk);
        } else {
            if (!mod_down && !mod2_down && is_pressed) hk->func(hk);
        }
    }
}

void hotkey_enable(HotkeyStorage *storage, const char *name, bool state) {
    assert(storage);
    assert(name);
    for(int i = 0; i < storage->keysnum; ++i) {
        Hotkey *hk = &storage->keys[i];
        if (strcmp(name, hk->name) == 0) {
            hk->enabled = state;
            break;
        }
    }
}

bool hotkey_exists(HotkeyStorage *storage, Hotkey hk, Hotkey *existing) {
    assert(storage);
    for(int i = 0; i < storage->keysnum; i++) {
        Hotkey cur = storage->keys[i];
        if (cur.combo.key == hk.combo.key &&
            cur.combo.mode == hk.combo.mode &&
            cur.combo.mod == hk.combo.mod &&
            cur.combo.mod2 == hk.combo.mod2 &&
            cur.groups == hk.groups) {
                if (existing)
                    *existing = cur;
                return true;
            }
    }
    return false;
}

void hotkey_group_enable(HotkeyStorage *storage, uint8_t mask, bool enable) {
    assert(storage);
    for(int i = 0; i < storage->keysnum; ++i) {
        Hotkey *hk = &storage->keys[i];
        hk->enabled = (hk->groups & mask) ? enable : hk->enabled;
    }
}

void hotkey_group_unregister(HotkeyStorage *storage, uint8_t mask) {
    Hotkey new_keys[MAX_HOTKEYS] = {0,};
    int num = 0;
    assert(storage);
    for(int i = 0; i < storage->keysnum; ++i) {
        Hotkey *hk = &storage->keys[i];
        if (hk->groups & mask) {
            free(hk->description);
            free(hk->name);
        } else {
            new_keys[num++] = *hk;
        }
    }
    memcpy(storage->keys, new_keys, num * sizeof(Hotkey));
    storage->keysnum = num;
}
