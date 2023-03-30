#pragma once

#include "raylib.h"

typedef struct GraphDrawer GraphDrawer;

GraphDrawer *grdr_new(int cap);
void grdr_free(GraphDrawer *g);

void grdr_draw(GraphDrawer *g, Vector2 pos);
void grdr_push(GraphDrawer *g, double value);
