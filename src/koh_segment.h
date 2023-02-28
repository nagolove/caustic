#pragma once

#include "object.h"
#include "stage_fight.h"

typedef struct Segment {
    Object obj;
} Segment;

void segments_init(Stage_Fight *st);
void segment_add(Stage_Fight *st, Vector2 start, Vector2 end);
