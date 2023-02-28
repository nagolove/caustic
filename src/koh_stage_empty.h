// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "stages.h"

typedef struct Stage_Empty {
    Stage parent;
} Stage_Empty;

Stage *stage_empty_new(void);
