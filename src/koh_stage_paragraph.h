#pragma once

#include "koh_stages.h"

typedef struct Stage_paragraph {
    Stage parent;
    Font fnt;
} Stage_paragraph;

Stage *stage_paragraph_new(void);
