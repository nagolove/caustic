// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "hotkey.h"
#include "stages.h"
#include "menu.h"
#include "timer.h"

typedef struct Stage_TestMenu {
    Stage parent;

    char *message;
    HotkeyStorage *hk_store;
    Font          fnt;
    Menu          mshort, mlong;
    Menu          *active;
    Timer         *tmr_select;
    TimerStore    timers;
} Stage_TestMenu;

Stage *stage_tstmenu_new(HotkeyStorage *hk_store);


