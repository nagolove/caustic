#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
    Хоткеи должны переключаться в зависимости от активной сцены. Если сцена
    не активна, то ее хоткеи не обрабатываются.
*/

#define MAX_HOTKEYS 64

typedef struct Hotkey Hotkey;
typedef void (*HotkeyFunc)(Hotkey *hk);

typedef enum HotkeyMode {
    HM_MODE_RESERVED_FIRST  = 0,
    HM_MODE_ISKEYDOWN       = 1,
    HM_MODE_ISKEYPRESSED    = 2,
    HM_MODE_RESERVED_LAST   = 3,
} HotkeyMode;

typedef struct HotkeyCombo {
    HotkeyMode mode;
    int        mod, mod2; // Ctrl, Shift, Alt .. or 0
    int        key;
} HotkeyCombo;

typedef struct Hotkey {
    HotkeyCombo combo;
    HotkeyFunc  func;
    char        *description;
    char        *name;
    void        *data;
    bool        enabled;
    uint8_t     groups;    // Маска с группами
} Hotkey;

typedef struct HotkeyStorage {
    Hotkey      keys[MAX_HOTKEYS];
    int         keysnum;
    // Ссылка на Луа табличку связывающую номер клавиши со строкой.
    //int     map_key2str_ref;
    uint64_t    updated_times;
} HotkeyStorage;

void hotkey_init(HotkeyStorage *storage);
void hotkey_shutdown(HotkeyStorage *storage);

void hotkey_group_enable(HotkeyStorage *storage, uint8_t mask, bool enable);
void hotkey_group_unregister(HotkeyStorage *storage, uint8_t mask);

void hotkey_register(HotkeyStorage *storage, Hotkey hk);
void hotkey_process(HotkeyStorage *storage);
void hotkey_enable(HotkeyStorage *storage, const char *name, bool state);
bool hotkey_exists(HotkeyStorage *storage, Hotkey hk, Hotkey *existing);
void hotkeys_enumerate(HotkeyStorage *storage);

void hotkey_state_push(HotkeyStorage *storage);
void hotkey_state_pop(HotkeyStorage *storage);

extern bool hotkey_verbose;
extern const char *koh_key2str[];
