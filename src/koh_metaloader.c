#include "koh_metaloader.h"

#include "koh_common.h"
#include "koh_logger.h"
#include <string.h>
#include "koh_lua_tools.h"
#include "lauxlib.h"
#include "lua.h"
#include <assert.h>
#include <stdlib.h>

struct MetaLoader {
    lua_State   *lua;
    int         ref_tbl_root;
};

MetaLoader *metaloader_new() {
    MetaLoader *ml = calloc(1, sizeof(MetaLoader));
    ml->lua = luaL_newstate();
    assert(ml->lua);
    trace("metaloader_new:\n");
    luaL_openlibs(ml->lua);
    lua_createtable(ml->lua, 0, 0);
    ml->ref_tbl_root = luaL_ref(ml->lua, LUA_REGISTRYINDEX);
    return ml;
}

void metaloader_free(MetaLoader *ml) {
    assert(ml);
    trace("metaloader_free:\n");
    lua_close(ml->lua);
    free(ml);
}

bool metaloader_load(MetaLoader *ml, const char *fname) {
    assert(ml);
    trace("metaloader_load:\n");
    trace("metaloader_load: [%s]\n", stack_dump(ml->lua));

    if (!ml->lua) {
        trace("metaloader_load: ml->lua == NULL\n");
        return false;
    }

    int type = lua_rawgeti(ml->lua, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    if (type != LUA_TTABLE)  {
        trace(
            "metaloader_load: could not get ref_tbl_root as table [%s]\n", 
            stack_dump(ml->lua)
        );
        return false;
    }

    print_table(ml->lua, 1);

    const char *fname_noext = extract_filename(fname, ".lua");
    trace(
        "metaloader_load: fname '%s', noext '%s'\n",
        fname,
        fname_noext
    );
    lua_pushstring(ml->lua, fname_noext);
    trace("metaloader_load: [%s]\n", stack_dump(ml->lua));

    if (luaL_loadfile(ml->lua, fname) != LUA_OK) {
        trace(
            "metaloader_load: could not load '%s'\n", 
            lua_tostring(ml->lua, -1)
        );
        trace(
            "metaloader_load: [%s]\n", 
            stack_dump(ml->lua)
        );
        // Скинуть ref_tbl_root таблицу и сообщение об ошибке
        lua_pop(ml->lua, 2);
        trace(
            "metaloader_load: [%s]\n", 
            stack_dump(ml->lua)
        );
        return false;
    }

    trace("metaloader_load: [%s]\n", stack_dump(ml->lua));
    lua_call(ml->lua, 0, 1);
    trace("metaloader_load: [%s]\n", stack_dump(ml->lua));
    print_table(ml->lua, 1);
    lua_rawset(ml->lua, 1);
    trace("metaloader_load: [%s]\n", stack_dump(ml->lua));
    lua_remove(ml->lua, 1);
    trace("metaloader_load: [%s]\n", stack_dump(ml->lua));
    return true;
}

Rectangle *metaloader_get(
    MetaLoader *ml, const char * fname, const char *objname
) {
    trace("metaloader_get:\n");
    return NULL;
}

Rectangle *metaloader_get_fmt(
    MetaLoader *ml, const char * fname, const char *objname_fmt, ...
) {
    trace("metaloader_get_fmt:\n");
    return NULL;
}

void metaloader_write(MetaLoader *ml) {
    trace("metaloader_write:\n");
}

void metaloader_print(MetaLoader *ml) {
    assert(ml);
    assert(ml->lua);
    int type = lua_rawgeti(ml->lua, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    if (type == LUA_TTABLE) {
        int top = lua_gettop(ml->lua);
        trace("metaloader_print: %s\n", table_get_print(ml->lua, top, NULL));
        lua_pop(ml->lua, 1);
    }
}

struct MetaLoaderFilesList metaloader_fileslist(MetaLoader *ml) {
    assert(ml);
    struct MetaLoaderFilesList fl = {};
    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int cap = 10;
    fl.fnames = calloc(cap, sizeof(fl.fnames[0]));

    lua_pushnil(l);
    while (lua_next(l, 1)) {
        if (fl.num + 1 == cap) {
            void *new_mem = realloc(fl.fnames, sizeof(fl.fnames[0]));
            if (!new_mem) {
                trace("metaloader_fileslist: bad realloc\n");
                memset(&fl, 0, sizeof(fl));
                return fl;
            }
            fl.fnames = new_mem;
        }

        const char *fname = lua_tostring(l, -2);
        assert(fname);

        fl.fnames[fl.num++] = strdup(fname);

        lua_pop(l, 1);
    }

    return fl;
}

void metaloader_fileslist_shutdown(struct MetaLoaderFilesList *fl) {
    assert(fl);
    for (int u = 0; u < fl->num; u++) {
        if (fl->fnames[u]) {
            free(fl->fnames[u]);
            fl->fnames[u] = 0;
        }
    }
    free(fl);
}

struct MetaLoaderObjects metaloader_objects_get(
    struct MetaLoader *ml, const char *fname_noext
) {
    assert(ml);
    struct MetaLoaderObjects object = {};

    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);

    trace("metaloader_objects_get: [%s]\n", stack_dump(l));

    lua_pushstring(l, fname_noext);
    if (lua_gettable(l, -2) != LUA_TTABLE) {
        return (struct MetaLoaderObjects){};
    }

    lua_pushnil(l);
    int tbl_index = lua_gettop(l) - 1;
    while (lua_next(l, tbl_index)) {
        if (object.num + 1 == METALOADER_MAX_OBJECTS) {
            lua_settop(l, 0);
            trace("metaloader_objects_get: METALOADER_MAX_OBJECTS reached\n");
            return object;
        }

        const char *field_name = lua_tostring(l, -2);
        printf("field_name '%s'\n", field_name);
        if (!field_name)
            goto _next;
        object.names[object.num] = strdup(field_name);

        trace(
            "metaloader_objects_get: %s - %s\n", 
            lua_tostring(l, -1),
            lua_tostring(l, -2)
        );

        {
            printf("before pushnil [%s]\n", stack_dump(l));
            lua_pushnil(l);

            lua_next(l, lua_gettop(l) - 1);
            printf("%f\n", lua_tonumber(l, -1));
            object.rects[object.num].x = lua_tonumber(l, -1);
            lua_pop(l, 1);

            lua_next(l, lua_gettop(l) - 1);
            printf("%f\n", lua_tonumber(l, -1));
            object.rects[object.num].y = lua_tonumber(l, -1);
            lua_pop(l, 1);

            lua_next(l, lua_gettop(l) - 1);
            printf("%f\n", lua_tonumber(l, -1));
            object.rects[object.num].width = lua_tonumber(l, -1);
            lua_pop(l, 1);

            lua_next(l, lua_gettop(l) - 1);
            printf("%f\n", lua_tonumber(l, -1));
            object.rects[object.num].height = lua_tonumber(l, -1);
            lua_pop(l, 1);

            lua_pop(l, 1);
            printf("HERR [%s]\n", stack_dump(l));
        }

        object.num++;
_next:
        lua_pop(l, 1);
        printf("iter\n");
    }
    trace("metaloader_objects_get: [%s]\n", stack_dump(l));
    lua_settop(l, 0);

    return object;
}

void metaloader_objects_shutdown(struct MetaLoaderObjects *objects) {
    assert(objects);

    for (int j = 0; objects->num; ++j) {
        if (objects->names[j]) {
            free(objects->names[j]);
            objects->names[j] = NULL;
        }
    }
}

void metaloader_file_new(MetaLoader *ml, const char *new_fname_noext) {
    assert(ml);
    assert(new_fname_noext);
}

void metaloader_object_set(
    MetaLoader *ml, const char *fname_noext, 
    const char *objname, Rectangle rect
) {
}
