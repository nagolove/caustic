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
    trace("metaloader_load: fname_noext '%s'\n", fname_noext);
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
    return fl;
}

void metaloader_fileslist_shutdown(struct MetaLoaderFilesList *fl) {
    assert(fl);
}

struct MetaLoaderObjects metaloader_objects_get(
    struct MetaLoader *ml, const char *fname_noext
) {
    assert(ml);
    struct MetaLoaderObjects object = {};

    lua_settop(ml->lua, 0);

    int type = lua_rawgeti(ml->lua, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);

    trace("metaloader_objects_get: [%s]\n", stack_dump(ml->lua));

    lua_pushstring(ml->lua, fname_noext);
    if (lua_gettable(ml->lua, -2) == LUA_TTABLE) {
    }

    trace("metaloader_objects_get: [%s]\n", stack_dump(ml->lua));

    return object;
}

void metaloader_objects_shutdown(struct MetaLoaderObjects *objects) {
    assert(objects);
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
