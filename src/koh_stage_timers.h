// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "stages.h"
#include "timer.h"

typedef struct Stage_Timers {
    Stage parent;
    TimerStore ts;
} Stage_Timers;

Stage *stage_timers_new(void);
