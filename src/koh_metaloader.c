#include "koh_metaloader.h"

#include "koh_common.h"
#include "koh_logger.h"
#include <stdbool.h>
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

// На верху луа стека должна лежать загруженная строка кода
static bool _metaloader_load(
    MetaLoader *ml, const char *fname_noext
) {
    assert(ml);
    lua_State *l = ml->lua;

    /*
    trace(
        "_metaloader_load: fname_noext '%s' [%s]\n",
        fname_noext, stack_dump(l)
    );
    */

    assert(lua_type(l, -1) == LUA_TFUNCTION);
    //trace("_metaloader_load: [%s]\n", stack_dump(l));
    lua_call(l, 0, LUA_MULTRET);
    int tbl_data_index = lua_gettop(l);
    //table_print(l, -1);

    //trace("_metaloader_load: [%s]\n", stack_dump(l));

    if (!l) {
        trace("_metaloader_load: l == NULL\n");
        return false;
    }

    int type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    if (type != LUA_TTABLE)  {
        trace(
            "_metaloader_load: could not get ref_tbl_root as table [%s]\n", 
            stack_dump(l)
        );
        return false;
    }
    int tbl_index = lua_gettop(l);

    lua_pushstring(l, fname_noext);
    //trace("_metaloader_load: before lua_copy [%s]\n", stack_dump(l));
    lua_pushvalue(l, tbl_data_index);
    //trace("_metaloader_load: [%s]\n", stack_dump(l));
    lua_settable(l, tbl_index);
    //trace("_metaloader_load: [%s]\n", stack_dump(l));

    //trace("metaloader_load: [%s]\n", stack_dump(l));
    //lua_call(l, 0, 1);
    //trace("metaloader_load: [%s]\n", stack_dump(l));
    //table_print(l, 1);
    //lua_rawset(l, 1);

    //trace("_metaloader_load: [%s]\n", stack_dump(l));
    
    //lua_pop(l, 1);
    lua_settop(l, 0);

    //trace("_metaloader_load: [%s]\n", stack_dump(l));
    return true;
}

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

bool metaloader_load_f(MetaLoader *ml, const char *fname) {
    assert(ml);
    assert(fname);
    if (luaL_loadfile(ml->lua, fname) != LUA_OK) {
        trace(
            "metaloader_load_f: could not do luaL_loadfile() with %s\n",
            lua_tostring(ml->lua, -1)
        );
        return false;
    }
    const char *fname_noext = extract_filename(fname, ".lua");
    return _metaloader_load(ml, fname_noext);
}

Rectangle *metaloader_get(
    MetaLoader *ml, const char *fname_noext, const char *objname
) {
    trace("metaloader_get:\n");
    return metaloader_get_fmt(ml, fname_noext, "%s", objname);
}

Rectangle *metaloader_get_fmt(
    MetaLoader *ml, const char *fname_noext, const char *objname_fmt, ...
) {
    trace("metaloader_get_fmt:\n");

    assert(ml);
    assert(fname_noext);
    assert(objname_fmt);

    char tbl_name[64] = {};
    va_list args;
    va_start(args, objname_fmt);
    vsnprintf(tbl_name, sizeof(tbl_name) - 1, objname_fmt, args);
    va_end(args);

    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int type;
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);

    trace(
        "metaloader_get_fmt: tbl_name '%s'\n",
        tbl_name
    );
    lua_pushstring(l, tbl_name);
    type = lua_gettable(l, -2);
    //assert(type == LUA_TTABLE);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_get_fmt: no such object name '%s' in table, "
            "instead type is %s\n",
            tbl_name,
            lua_typename(l, lua_type(l, -1))
        );
        lua_settop(l, 0);
        return NULL;
    }

    lua_pushnil(l);
    int top = lua_gettop(l) - 1, i = 0;
    float values[4] = {};
    while (lua_next(l, top)) {
        values[i++] = lua_tonumber(l, -1);
        lua_pop(l, 1);
    }
    assert(i == 4);
    lua_settop(l, 0);
    static Rectangle rect = {};
    rect = rect_from_arr(values);

    return &rect;
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

    int type;
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_objects_get: could not get table '%s' "
            "by ref_tbl_root\n",
            fname_noext
        );
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

        int i = 0;
        if (lua_istable(l, -1)) {
            lua_pushnil(l);

            float values[4] = {};
            while (lua_next(l, lua_gettop(l) - 1)) {
                if (lua_isnumber(l, -1)) 
                    values[i++] = lua_tonumber(l, -1);
                else {
                    lua_pop(l, 2);
                    goto _skip_next;
                }
                lua_pop(l, 1);

                if (i == 4)
                    break;
            }

            if (i > 0) 
                lua_pop(l, 1);
            object.rects[object.num] = rect_from_arr(values);
        } else 
            continue; // встретилось что-то другое - строка, число и т.д.

        const char *field_name = lua_tostring(l, -2);
        if (!field_name) {
            trace(
                "metaloader_objects_get: no field_name [%s]\n",
                stack_dump(l)
            );
            lua_pop(l, 1);
            continue;
        }

        //trace("metaloader_objects_get: field_name '%s'\n", field_name);

        // Добавляем объект только если все значения для массива прямоугольника
        // заполнены
        if (i == 4) {
            object.names[object.num] = strdup(field_name);
            object.num++;
        }

_skip_next:

        lua_pop(l, 1);
    }
    lua_settop(l, 0);

    return object;
}

void metaloader_objects_shutdown(struct MetaLoaderObjects *objects) {
    assert(objects);
    //trace("metaloader_objects_shutdown: objects->num %d\n", objects->num);
    for (int j = 0; j < objects->num; ++j) {
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

bool metaloader_load_s(
    MetaLoader *ml, const char *fname, const char *luacode
) {
    assert(ml);
    assert(fname);
    assert(luacode);
    if (luaL_loadstring(ml->lua, luacode) != LUA_OK) {
        trace(
            "metaloader_load_s: could not do luaL_loadstring() with %s\n",
            lua_tostring(ml->lua, -1)
        );
        return false;
    }
    const char *fname_noext = extract_filename(fname, ".lua");
    return _metaloader_load(ml, fname_noext);
}
