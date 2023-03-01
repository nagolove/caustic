#pragma once

#include "koh_stages.h"

typedef struct Stage_HeightColor {
    Stage parent;
    //cpSpace *space;
    //DAS_Context das;
} Stage_HeightColor;

Stage *stage_heightcolor_new(void);
void stage_heightcolor_init(Stage_HeightColor *st, void *data);
void stage_heightcolor_update(Stage_HeightColor *st);
void stage_heightcolor_shutdown(Stage_HeightColor *st);



