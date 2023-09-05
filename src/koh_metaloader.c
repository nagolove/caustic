#include "koh_metaloader.h"

#include "koh_common.h"
#include "koh_logger.h"
#include <stdbool.h>
#include <string.h>
#include "koh_lua_tools.h"
#include "lauxlib.h"
#include "lua.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>

struct MetaLoader {
    lua_State   *lua;
    int         ref_tbl_root, 
                // Таблица ["имя_файла_без_расширения"] =  "путь+имя_файла"
                ref_tbl_fnames; 
};

// На верху луа стека должна лежать загруженная строка кода
// fname - необязательный агрумент для последующего сохранения файлов через
// metaloader_write()
static bool _metaloader_load(
    MetaLoader *ml, const char *fname_noext, const char *fname
) {
    assert(ml);
    lua_State *l = ml->lua;
    char *str;

    trace(
        "_metaloader_load: fname_noext '%s' [%s]\n",
        fname_noext, stack_dump(l)
    );

    //////////// DEBUG
    str = table_dump2allocated_str(l);
    trace("_metaloader_load: dump '%s'\n", str);
    if (str)
        free(str);
    //////////// DEBUG

    assert(lua_type(l, -1) == LUA_TFUNCTION);
    //trace("_metaloader_load: [%s]\n", stack_dump(l));
    lua_call(l, 0, LUA_MULTRET);
    int tbl_data_index = lua_gettop(l);

    //table_print(l, -1);
    //table_print(l, lua_gettop(l) - 1);
    //trace("_metaloader_load: [%s]\n", stack_dump(l));

    //////////// DEBUG
    str = table_dump2allocated_str(l);
    trace("_metaloader_load: '%s'\n", str);
    if (str)
        free(str);
    //////////// DEBUG

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

    trace("_metaloader_load: done [%s]\n", stack_dump(l));
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
    return _metaloader_load(ml, fname_noext, fname);
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

    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int type;
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);
    
    char obj_name[64] = {};
    va_list args;
    va_start(args, objname_fmt);
    vsnprintf(obj_name, sizeof(obj_name) - 1, objname_fmt, args);
    va_end(args);

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    //assert(type == LUA_TTABLE);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_get_fmt: no such fname name '%s' in table, "
            "instead type is %s\n",
            fname_noext,
            lua_typename(l, lua_type(l, -1))
        );
        lua_settop(l, 0);
        return NULL;
    }

    lua_pushstring(l, obj_name);
    type = lua_gettable(l, -2);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_get_fmt: no such object '%s' in table %s",
            obj_name, fname_noext
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

void metaloader_print_all(MetaLoader *ml) {
    /*
    assert(ml);
    assert(ml->lua);

    if (!ml->ref_tbl_root) {
        trace("metaloader_print_all: ref_tbl_root == 0\n");
        return;
    }

    lua_State *l = ml->lua;
    int type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    if (type != LUA_TTABLE) {
        lua_settop(l, 0);
        return;
    }

    lua_pushnil(l);
    while (lua_next(l, lua_gettop(l) - 1)) {
        trace(
            "metaloader_print_all: type %s - type %s\n",
            lua_typename(l, lua_type(l, -1)),
            lua_typename(l, lua_type(l, -2))
        );

        char *s = NULL;

        lua_pushvalue(l, -1);
        s = table_dump2allocated_str(l);
        trace("metaloader_print: dump '%s'\n", s);
        if (s)
            free(s);

        lua_pushvalue(l, -3);
        s = table_dump2allocated_str(l);
        trace("metaloader_print: dump '%s'\n", s);
        if (s)
            free(s);

        trace("metaloader_print_all: [%s]\n", stack_dump(l));

        //lua_pop(l, 1);

        lua_pop(l, 1);
        trace("metaloader_print_all: [%s]\n", stack_dump(l));
    }

    lua_settop(l, 0);
    */
}

void metaloader_print(MetaLoader *ml) {
    assert(ml);
    assert(ml->lua);
    int type = lua_rawgeti(ml->lua, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    if (type == LUA_TTABLE) {
        int top = lua_gettop(ml->lua);
        //trace("metaloader_print: %s\n", table_get_print(ml->lua, top, NULL));
        char *s = table_dump2allocated_str(ml->lua);
        trace("metaloader_print: %s\n", table_get_print(ml->lua, top, NULL));
        trace("metaloader_print: dump %s\n", s);
        if (s)
            free(s);
        lua_pop(ml->lua, 1);
    }
}

struct MetaLoaderFilesList metaloader_fileslist(MetaLoader *ml) {
    assert(ml);
    struct MetaLoaderFilesList fl = {};
    lua_State *l = ml->lua;
    assert(l);
    lua_settop(l, 0);

    int cap = 10;
    fl.fnames = calloc(cap, sizeof(fl.fnames[0]));

    int type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    if (type != LUA_TTABLE) {
        trace("metaloader_fileslist: ref_tbl_root is not a table\n");
        return fl;
    }

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

    lua_settop(l, 0);
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
    if (fl->fnames) {
        free(fl->fnames);
        fl->fnames = NULL;
    }
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

    /*
    char *str = table_dump2allocated_str(l);
    trace("metaloader_objects_get: dump '%s'\n", str);
    if (str)
        free(str);
    */

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
        // Таблица со значениями для Rectangle
        if (lua_istable(l, -1)) {
            lua_pushnil(l);

            float values[4] = {};
            // Чтение полей значений Rectangle
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
        /*trace("metaloader_objects_get: field_name %s\n", field_name);*/
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
    trace("metaloader_file_new:\n");
}

/*
void metaloader_object_set(
    MetaLoader *ml, const char *fname_noext, 
    const char *objname, Rectangle rect
) {
    trace("metaloader_object_set:\n");
}
*/

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
        lua_settop(ml->lua, 0);
        return false;
    }
    const char *fname_noext = extract_filename(fname, ".lua");
    return _metaloader_load(ml, fname_noext, NULL);
}

void metaloader_set_fmt(
    MetaLoader *ml,
    Rectangle rect,
    const char *fname_noext, 
    const char *obj_name_fmt, ...
) {
    assert(ml);
    assert(fname_noext);
    assert(obj_name_fmt);

    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int type;
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);
    
    char obj_name[64] = {};
    va_list args;
    va_start(args, obj_name_fmt);
    vsnprintf(obj_name, sizeof(obj_name) - 1, obj_name_fmt, args);
    va_end(args);

    trace("metaloader_set_fmt: [%s]\n", stack_dump(l));

    /*
    char *dump;
    dump = table_dump2allocated_str(l);
    if (dump) {
        trace("metaloader_set_fmt: dump '%s'\n", dump);
        free(dump);
    }
    trace("metaloader_set_fmt: [%s]\n", stack_dump(l));
    */

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    //assert(type == LUA_TTABLE);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_get_fmt: no such fname name '%s' in table, "
            "instead type is %s\n",
            fname_noext,
            lua_typename(l, lua_type(l, -1))
        );
        lua_settop(l, 0);
        return;
    }

    //lua_pushstring(l, fname_noext);
    lua_pushstring(l, obj_name);
    table_push_rect_as_arr(l, rect);
    lua_settable(l, -3);

    lua_settop(l, 0);
    /*
    lua_pushstring(l, obj_name);
    type = lua_gettable(l, -2);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_get_fmt: no such object '%s' in table %s",
            obj_name, fname_noext
        );
        lua_settop(l, 0);
        return;
    }

    // TODO: На вершине стека лежит таблица, все элементы которой нужно
    // поменять на другие.
    // Или просто заменить таблицу новой.

    */

    /*
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
    */

}

void metaloader_set(
    MetaLoader *ml, 
    Rectangle rect,
    const char *fname_noext, 
    const char *objname
) {
    metaloader_set_fmt(ml, rect, fname_noext, "%s", objname);
}

void metaloader_set_rename_fmt(
    MetaLoader *ml,
    const char *fname_noext, 
    const char *prev_objname,
    const char *new_objname_fmt,
    ...
) {
    assert(ml);
    assert(fname_noext);
    assert(prev_objname);
    assert(new_objname_fmt);

    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int type;
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);
    
    char obj_name[64] = {};
    va_list args;
    va_start(args, new_objname_fmt);
    vsnprintf(obj_name, sizeof(obj_name) - 1, new_objname_fmt, args);
    va_end(args);

    trace("metaloader_set_rename_fmt: [%s]\n", stack_dump(l));

    /*
    char *dump;
    dump = table_dump2allocated_str(l);
    if (dump) {
        trace("metaloader_set_fmt: dump '%s'\n", dump);
        free(dump);
    }
    trace("metaloader_set_fmt: [%s]\n", stack_dump(l));
    */

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    //assert(type == LUA_TTABLE);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_set_rename_fmt: no such fname name '%s' in table, "
            "instead type is %s\n",
            fname_noext,
            lua_typename(l, lua_type(l, -1))
        );
        goto _cleanup;
    }

    //lua_pushstring(l, fname_noext);
    lua_pushstring(l, prev_objname);
    /*table_push_rect_as_arr(l, rect);*/
    /*lua_settable(l, -3);*/
    lua_gettable(l, -2);
    /*int top = lua_gettop(l);*/
    if (lua_type(l, -1) != LUA_TTABLE) {
        trace(
            "metaloader_set_rename_fmt: not table in '%s' - '%s'\n",
            fname_noext,
            prev_objname
        );
        goto _cleanup;
    }

    trace("metaloader_set_rename_fmt: [%s]\n", stack_dump(l));

    /*lua_pushvalue(l, -1);*/
    int ref_rect_arr = luaL_ref(l, LUA_REGISTRYINDEX);
    // [ "tank", {0, 0, 100, 100} ]
    trace("metaloader_set_rename_fmt: [%s]\n", stack_dump(l));

    lua_pushstring(l, prev_objname);
    lua_pushnil(l);
    lua_settable(l, -3);

    trace("metaloader_set_rename_fmt: [%s]\n", stack_dump(l));

    lua_pushstring(l, obj_name);
    lua_rawgeti(l, LUA_REGISTRYINDEX, ref_rect_arr);
    lua_settable(l, -3);

    luaL_unref(l, LUA_REGISTRYINDEX, ref_rect_arr);

    trace("metaloader_set_rename_fmt: [%s]\n", stack_dump(l));

_cleanup:
    lua_settop(l, 0);
}

void metaloader_remove_fmt(
    MetaLoader *ml,
    const char *fname_noext, 
    const char *objname_fmt, ...
) {
    assert(ml);
    assert(fname_noext);
    assert(objname_fmt);

    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int type;
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);
    
    char obj_name[64] = {};
    va_list args;
    va_start(args, objname_fmt);
    vsnprintf(obj_name, sizeof(obj_name) - 1, objname_fmt, args);
    va_end(args);

    /*
    char *dump;
    dump = table_dump2allocated_str(l);
    if (dump) {
        trace("metaloader_remove_fmt: dump '%s'\n", dump);
        free(dump);
    }
    */

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    if (type != LUA_TTABLE) {
        trace(
            "metaloader_remove_fmt: no such fname name '%s' in table, "
            "instead type is %s\n",
            fname_noext,
            lua_typename(l, lua_type(l, -1))
        );
        goto _cleanup;
    }

    /*
    dump = table_dump2allocated_str(l);
    if (dump) {
        trace("metaloader_remove_fmt: dump '%s'\n", dump);
        free(dump);
    }
    */

    lua_pushstring(l, obj_name);
    lua_pushnil(l);
    lua_settable(l, -3);
_cleanup:
    lua_settop(l, 0);
}
