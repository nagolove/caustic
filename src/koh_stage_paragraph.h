#pragma once

#include "stages.h"

typedef struct Stage_paragraph {
    Stage parent;
    Font fnt;
} Stage_paragraph;

Stage *stage_paragraph_new(void);
