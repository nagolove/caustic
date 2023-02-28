// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "stages.h"

typedef struct Stage_immediate {
    Stage parent;
} Stage_immediate;

Stage *stage_immediate_new(void);


