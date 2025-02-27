#pragma once

#include "lua.h"
#include "raylib.h"

#define MAX_SPRITE_LAYERS   16
typedef struct LayeredSprite {
    lua_State       *vm;
    RenderTexture2D tex_combined;
    Texture         textures[MAX_SPRITE_LAYERS];
    int             ref_layer[MAX_SPRITE_LAYERS];
    int             num, version;
} LayeredSprite;

void layered_sprite_shutdown(LayeredSprite *ls);
void layered_sprite_init(
    LayeredSprite *ls, lua_State *vm, int ref_tbl, const char *fname
);
void layered_sprite_bake(LayeredSprite *ls);

