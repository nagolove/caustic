#pragma once

#include "raylib.h"

typedef struct koh_Animator koh_Animator;

typedef struct koh_Animator_Def {
    char *lua_file, *assets_dir;
} koh_Animator_Def;

koh_Animator *koh_animator_new(koh_Animator_Def *def);
void koh_animator_free(koh_Animator *a);
void koh_animator_update(koh_Animator *a);
void koh_animator_draw(koh_Animator *a, Vector2 pos, float angle);
