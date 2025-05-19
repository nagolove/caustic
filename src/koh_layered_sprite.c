#include "koh_layered_sprite.h"

#include <string.h>
#include "koh_common.h"
#include <assert.h>
#include "koh_lua.h"
#include "koh_logger.h"

void layered_sprite_shutdown(LayeredSprite *ls) {
    assert(ls);

    SetTraceLogLevel(LOG_WARNING);
    for (int i = 0; i < ls->num; ++i) {
        if (ls->textures[i].id)
            UnloadTexture(ls->textures[i]);
    }
    SetTraceLogLevel(LOG_ALL);
}

void layered_sprite_bake(struct LayeredSprite *ls) {
    assert(ls);
    assert(ls->vm);
    lua_State *vm = ls->vm;
    int max_num = sizeof(ls->textures) / sizeof(ls->textures[0]);
    if (ls->num >= max_num) {
        printf("layered_sprite_bake: textures buffer is full\n");
        exit(EXIT_FAILURE);
    }

    int top = lua_gettop(ls->vm);

    assert(ls->ref_layer);
    // XXX: Почему ls->ref_layer одинаковый?
    //lua_rawgeti(vm, LUA_REGISTRYINDEX, ls->ref_layer);
    //trace("layered_sprite_bake: ls->ref_layer %d\n", ls->ref_layer);

    /*
    char *dump = table_dump2allocated_str(vm);
    if (dump) {
        trace(
            "layered_sprite_bake: ls->ref_layer '%s'\n", dump
        );
        free(dump);
    }
    */

    //put_layers_table(vm, ref_ase_exported_tbl, );
    BeginMode2D((Camera2D) {
        .zoom = 1.,
    });
    BeginTextureMode(ls->tex_combined);
    ClearBackground(BLANK);
    for (int i = 0; i < ls->num; i++) {
        lua_rawgeti(vm, LUA_REGISTRYINDEX, ls->ref_layer[i]);
        trace("layered_sprite_bake: ls->ref_layer %d\n", ls->ref_layer[i]);

        trace("layered_sprite_bake: [%s]\n", L_stack_dump(vm));
        lua_pushstring(vm, "visible");
        lua_gettable(vm, -2);
        //trace("layered_sprite_bake: [%s]\n", stack_dump(ls->vm));
        //trace("layered_sprite_bake: ls->version %d\n", ls->version);
        assert(lua_isboolean(vm, -1));
        bool visible = lua_toboolean(vm, -1);
        lua_pop(vm, 1);
        trace("layered_sprite_bake: [%s]\n", L_stack_dump(vm));

        lua_pushstring(vm, "name");
        lua_gettable(vm, -2);
        assert(lua_isstring(vm, -1));
        const char *name = lua_tostring(vm, -1);

        trace(
            "layered_sprite_bake: i %d, name %s, visible %s\n",
            i, name, visible ? "true" : "false"
            );

        lua_pop(vm, 1);

        // [.., {ls->ref_layer[i]}, ]
        lua_pop(vm, 1);
         //[.., ]}, ]

        if (!visible)
            continue;

        const Rectangle dest = {
            0., 0., ls->textures[i].width, ls->textures[i].height,
        };
        const Rectangle source = {
            0., 0., ls->textures[i].width, -ls->textures[i].height,
        };
        DrawTexturePro(
            ls->textures[i], source, dest, (Vector2) {0., 0.}, 0., WHITE
        );
    }
    EndTextureMode();
    EndMode2D();
    lua_settop(ls->vm, top);
}

static void _layered_sprite_init_v2(
    struct LayeredSprite *ls, int ref_tbl, const char *fname, int top
) {
    assert(ls->vm);
    lua_State *vm = ls->vm;
    lua_pushstring(vm, "layers");
    lua_gettable(vm, -2);
    assert(lua_istable(vm, -1));

    ls->version = 2;

    /*
    dump_str = table_dump2allocated_str(vm);
    if (dump_str) {
        trace("layered_sprite_init: dump_str after layers %s\n", dump_str);
        free(dump_str);
    }
    // */


    //int layers_top = lua_gettop(vm);
    //trace(
        //"layered_sprite_init: table -1 '%s'\n",
        //table_get_print(vm, layers_top, NULL)
    //);

    const char *path = koh_extract_path(fname);
    assert(strlen(path) - 1 != '/');

    lua_pushnil(vm);
    int tex_index = 0;
    const int max_tex_index = sizeof(ls->textures) / sizeof(ls->textures[0]);
    while (lua_next(vm, -2)) {
        if (tex_index == max_tex_index) {
            trace(
                "layered_sprite_init: tex_index reached max value(%d)\n",
                max_tex_index
            );
            goto _cleanup;
        }
        //const char *tex_fname = lua_tostring(vm, -2);
        assert(lua_type(vm, -1) == LUA_TTABLE);

        lua_pushvalue(vm, -1);
        ls->ref_layer[tex_index] = luaL_ref(vm, LUA_REGISTRYINDEX);

        trace(
            "_layered_sprite_init_v2: ls->ref_layer[%d] %d\n",
            tex_index, ls->ref_layer[tex_index]
        );

        lua_pushstring(vm, "file");
        lua_gettable(vm, -2);
        assert(lua_isstring(vm, -1));
        const char *tex_fname = lua_tostring(vm, -1);
        lua_pop(vm, 1);

        assert(lua_type(vm, -1) == LUA_TTABLE);

        char buf[512] = {};
        snprintf(buf, sizeof(buf), "%s/%s", path, tex_fname);

        trace("layered_sprite_init: texture filename '%s'\n", buf);
        ls->textures[tex_index++] = LoadTexture(buf);
        lua_pop(vm, 1);
    }
    ls->num = tex_index;

    trace("layered_sprite_init: ls->num %d\n", ls->num);

    struct Texture *l0 = &ls->textures[0];
    ls->tex_combined = LoadRenderTexture(l0->width, l0->height);

    BeginMode2D((Camera2D) {
        .zoom = 1.,
    });
    BeginTextureMode(ls->tex_combined);
    ClearBackground(BLANK);
    for (int i = 0; i < ls->num; i++) {
        const Rectangle dest = {
            0., 0., ls->textures[i].width, ls->textures[i].height,
        };
        const Rectangle source = {
            0., 0., ls->textures[i].width, -ls->textures[i].height,
        };
        DrawTexturePro(
            ls->textures[i], source, dest, (Vector2) {0., 0.}, 0., WHITE
        );
    }
    EndTextureMode();
    EndMode2D();

#if 0
    char img_fname[512] = {};
    strcat(img_fname, extract_filename(fname, ".aseprite.lua"));
    strcat(img_fname, ".png");
    Image img = LoadImageFromTexture(ls->tex_combined.texture);
    ExportImage(img, img_fname);
    UnloadImage(img);
#endif

_cleanup:
    lua_settop(vm, top);
}

void layered_sprite_init(
    LayeredSprite *ls, lua_State *vm, int ref_tbl, const char *fname
) {
    assert(ls);
    assert(vm);
    assert(ref_tbl);
    assert(fname);
    trace("layered_sprite_init: fname %s, [%s]\n", fname, L_stack_dump(vm));

    ls->vm = vm;
    int top = lua_gettop(vm);

    if (lua_rawgeti(vm, LUA_REGISTRYINDEX, ref_tbl) != LUA_TTABLE) {
        trace("layered_sprite_init: could not get ref_tbl from vm\n");
        exit(EXIT_FAILURE);
    }

    lua_pushstring(vm, fname);
    lua_gettable(vm, -2);

    /*
    char *dump_str;

    dump_str = table_dump2allocated_str(vm);
    if (dump_str) {
        trace("layered_sprite_init: dump_str %s\n", dump_str);
        free(dump_str);
    }
    */

    lua_pushstring(vm, "caustic_aseprite_export_ver");
    lua_gettable(vm, -2);
    int version = lua_tonumber(vm, -1);
    lua_pop(vm, 1);

    switch (version) {
        //case 1: _layered_sprite_init_v1(ls, ref_tbl, fname, top); break;
        case 1: trace("layered_sprite_init: version 1 is not supported\n");
                exit(EXIT_FAILURE);
                break;
        case 2: _layered_sprite_init_v2(ls, ref_tbl, fname, top); break;
        default:
            trace("layered_sprite_init: unsupported ase export version\n");
            exit(EXIT_FAILURE);
    }

}
