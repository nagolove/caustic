#include "koh_animator.h"

#include "koh_logger.h"
#include <assert.h>
#include <stdlib.h>
#include "lua.h"
#include "lauxlib.h"
#include "stdio.h"

struct koh_Animator {
    Texture2D   texture;
    Rectangle   *frames;
    int         frames_num, current;
};

koh_Animator *koh_animator_new(koh_Animator_Def *def) {
    assert(def);
    koh_Animator *a = calloc(1, sizeof(koh_Animator));
    assert(a);

    if (!def->assets_dir) {
        trace("koh_animator_new: assets_dir in NULL\n");
        exit(EXIT_FAILURE);
    }

    if (!def->lua_file) {
        trace("koh_animator_new: lua_file in NULL\n");
        exit(EXIT_FAILURE);
    }

    lua_State *lua = luaL_newstate();

    const char *image_fname = lua_tostring(lua, -1);

    char fname[512] = {0};
    snprintf(fname, sizeof(fname), "%s/%s", def->assets_dir, image_fname);
    trace("koh_animator_new: fname %s\n", fname);
    a->texture = LoadTexture(fname);
    if (!a->texture.id) {
        trace("koh_animator_new: could not load texture from '%s'\n", fname);
    }

    lua_close(lua);

    return a;
}

void koh_animator_free(koh_Animator *a) {
    assert(a);
    free(a);
}

void koh_animator_update(koh_Animator *a) {
}

void koh_animator_draw(koh_Animator *a, Vector2 pos, float angle) {
}


