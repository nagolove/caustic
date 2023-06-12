// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "koh_stages.h"
#include "koh_timer.h"

typedef struct Stage_Timers {
    Stage      parent;
    koh_TimerStore ts;
} Stage_Timers;

Stage *stage_timers_new(void);
