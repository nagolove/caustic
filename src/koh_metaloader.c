#include "koh_metaloader.h"

#include "koh_common.h"
#include "koh_logger.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "koh_lua.h"
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
    bool        verbose;
};

// На верху луа стека должна лежать загруженная строка кода
// fname - необязательный агрумент для последующего сохранения файлов через
// metaloader_write()
static bool _metaloader_load(
    MetaLoader *ml, const char *fname_noext, const char *fname
) {
    assert(ml);
    lua_State *l = ml->lua;
    assert(l);
    char *str;

    if (ml->verbose)
        trace(
            "_metaloader_load: fname_noext '%s' [%s]\n",
            fname_noext, L_stack_dump(l)
        );

    //////////// DEBUG
    trace(
        "_metaloader_load: before table_dump2allocated_str [%s]\n",
        L_stack_dump(l)
    );
    str = L_table_serpent_alloc(l, NULL);
    trace(
        "_metaloader_load: before table_dump2allocated_str [%s]\n",
        L_stack_dump(l)
    );
    if (ml->verbose)
        trace("_metaloader_load: dump '%s'\n", str);
    if (str)
        free(str);
    //////////// DEBUG

    trace("_metaloader_load: [%s]\n", L_stack_dump(l));

    assert(lua_type(l, -1) == LUA_TFUNCTION);
    //trace("_metaloader_load: [%s]\n",L_stack_dump(l));
    lua_call(l, 0, LUA_MULTRET);
    int tbl_data_index = lua_gettop(l);

    //table_print(l, -1);
    //table_print(l, lua_gettop(l) - 1);
    //trace("_metaloader_load: [%s]\n",L_stack_dump(l));

    //////////// DEBUG
    str = L_table_serpent_alloc(l, NULL);
    if (ml->verbose)
        trace("_metaloader_load: '%s'\n", str);
    if (str)
        free(str);
    //////////// DEBUG

    if (!l) {
        if (ml->verbose)
            trace("_metaloader_load: l == NULL\n");
        return false;
    }

    int type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    if (type != LUA_TTABLE)  {
        if (ml->verbose)
            trace(
                "_metaloader_load: could not get ref_tbl_root as table [%s]\n", 
                L_stack_dump(l)
            );
        return false;
    }
    int tbl_index = lua_gettop(l);

    lua_pushstring(l, fname_noext);
    //trace("_metaloader_load: before lua_copy [%s]\n",L_stack_dump(l));
    lua_pushvalue(l, tbl_data_index);
    //trace("_metaloader_load: [%s]\n",L_stack_dump(l));
    lua_settable(l, tbl_index);
    //trace("_metaloader_load: [%s]\n",L_stack_dump(l));

    //trace("metaloader_load: [%s]\n",L_stack_dump(l));
    //lua_call(l, 0, 1);
    //trace("metaloader_load: [%s]\n",L_stack_dump(l));
    //table_print(l, 1);
    //lua_rawset(l, 1);

    //trace("_metaloader_load: [%s]\n",L_stack_dump(l));
    
    if (ml->verbose) {
        trace("_metaloader_load: done [%s]\n", L_stack_dump(l));
        trace("_metaloader_load: ref_tbl_fnames %d\n", ml->ref_tbl_fnames);
    }
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_fnames);
    if (type != LUA_TTABLE) {
        if (ml->verbose) {
            trace("_metaloader_load: [%s]\n", L_stack_dump(l));
            trace("_metaloader_load: could not get ml->ref_tbl_fnames\n");
        }
        exit(EXIT_FAILURE);
    }
    lua_pushstring(l, fname_noext);
    lua_pushstring(l, fname);
    lua_settable(l, -3);

    lua_settop(l, 0);

    return true;
}

MetaLoader *metaloader_new(const struct MetaLoaderSetup *setup) {
    MetaLoader *ml = calloc(1, sizeof(MetaLoader));
    assert(ml);
    // TODO: Или лучше использовать фильтрацию trace_filter_add()?
    if (setup) {
        ml->verbose = setup->verbose;
    }
    ml->lua = luaL_newstate();
    assert(ml->lua);
    if (ml->verbose)
        trace("metaloader_new:\n");
    luaL_openlibs(ml->lua);

    lua_createtable(ml->lua, 0, 0);
    ml->ref_tbl_root = luaL_ref(ml->lua, LUA_REGISTRYINDEX);

    if (ml->verbose)
        trace("metaloader_new: [%s]\n", L_stack_dump(ml->lua));
    lua_createtable(ml->lua, 0, 0);
    ml->ref_tbl_fnames = luaL_ref(ml->lua, LUA_REGISTRYINDEX);
    if (ml->verbose)
        trace("metaloader_new: [%s]\n", L_stack_dump(ml->lua));

    return ml;
}

void metaloader_free(MetaLoader *ml) {
    assert(ml);
    if (ml->verbose)
        trace("metaloader_free:\n");
    lua_close(ml->lua);
    free(ml);
}

bool metaloader_load_f(MetaLoader *ml, const char *fname) {
    assert(ml);
    assert(fname);
    if (luaL_loadfile(ml->lua, fname) != LUA_OK) {
        if (ml->verbose)
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
    if (ml->verbose)
        trace("metaloader_get:\n");
    return metaloader_get_fmt(ml, fname_noext, "%s", objname);
}

Rectangle *metaloader_get_fmt(
    MetaLoader *ml, const char *fname_noext, const char *objname_fmt, ...
) {
    if (ml->verbose)
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
        if (ml->verbose)
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
        if (ml->verbose)
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
    assert(ml);
    if (ml->verbose)
        trace("metaloader_write:\n");

    lua_State *l = ml->lua;

    /*
    lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_fnames);
    trace("metaloader_write: [%s]\n",L_stack_dump(l));
    char *dump = table_dump2allocated_str(l);
    if (dump) {
        printf("metaloader_write: dump %s\n", dump);
        free(dump);
    }
    lua_settop(l, 0);
    // */
    
    // Цикл по таблице ml->ref_tbl_fnames
    // Запрос значения
    // Сохранение значения

    /*
    lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_fnames);
    trace("metaloader_write: [%s]\n",L_stack_dump(l));
    lua_pushnil(l);
    while (lua_next(l, -2)) {
        trace(
            "metaloader_write: %s - %s\n",
            lua_tostring(l, -1),
            lua_tostring(l, -2)
        );
        lua_pop(l, 1);
    }
    */

    if (ml->verbose)
        trace("metaloader_write:\n");

    struct MetaLoaderFilesList fl = metaloader_fileslist(ml);
    for (int i = 0; i < fl.num; i++) {
        const char *fname = fl.fnames[i];

        if (ml->verbose)
            trace("metaloader_write: [%s]\n", L_stack_dump(l));

        lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_fnames);
        lua_pushstring(l, fname);
        lua_rawget(l, -2);
        const char *path = lua_tostring(l, -1);
        lua_pop(l, 2);
        if (!path) {
            if (ml->verbose)
                trace(
                    "metaloader_write: object without path, could not write\n"
                );
            continue;
        }

        if (ml->verbose) {
            trace("metaloader_write: [%s]\n", L_stack_dump(l));
            trace("metaloader_write: fname %s\n", fname);
            trace("metaloader_write: path %s\n", path);
        }

        char new_path[256] = {};
        strcat(new_path, path);
        strcat(new_path, "_");
        FILE *out = fopen(new_path, "w");
        if (!out) {
            continue;
        }

        fprintf(out, "return {\n");

        struct MetaLoaderObjects2 objs = metaloader_objects_get2(
            ml, extract_filename(fname, ".lua")
        );
        for (int j = 0; j < objs.num; j++) {
            if (objs.objs[j]) {
                struct MetaLoaderObject2Str str_repr;
                str_repr = metaloader_object2str(objs.objs[j]);
                if (ml->verbose)
                    trace(
                        "metaloader_write: %s - %s\n",
                        objs.names[j], str_repr.s
                    );
                fprintf(out, "%s = %s,\n", objs.names[j], str_repr.s);
                if (str_repr.is_allocated)
                    free((char*)str_repr.s);
            }
        }
        metaloader_objects_shutdown2(&objs);
        //trace("metaloader_write:\n");
        if (ml->verbose)
            trace("\n");

        fprintf(out, "}\n");
        fclose(out);
    }
    metaloader_fileslist_shutdown(&fl);

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

        trace("metaloader_print_all: [%s]\n",L_stack_dump(l));

        //lua_pop(l, 1);

        lua_pop(l, 1);
        trace("metaloader_print_all: [%s]\n",L_stack_dump(l));
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
        char *s = L_table_serpent_alloc(ml->lua, NULL);
        if (ml->verbose) {
            trace(
                "metaloader_print: %s\n",
                L_table_get_print(ml->lua, top, NULL)
            );
            trace("metaloader_print: dump %s\n", s);
        }
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
        if (ml->verbose) 
            trace("metaloader_fileslist: ref_tbl_root is not a table\n");
        return fl;
    }

    lua_pushnil(l);
    while (lua_next(l, 1)) {
        if (fl.num + 1 == cap) {
            void *new_mem = realloc(fl.fnames, sizeof(fl.fnames[0]));
            if (!new_mem) {
                if (ml->verbose) 
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
        if (ml->verbose)
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
            if (ml->verbose) 
                trace(
                    "metaloader_objects_get: METALOADER_MAX_OBJECTS reached\n"
                );
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
            if (ml->verbose) 
                trace(
                    "metaloader_objects_get: no field_name [%s]\n",
                    L_stack_dump(l)
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
    if (ml->verbose) 
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
        if (ml->verbose) 
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

    if (ml->verbose) 
        trace("metaloader_set_fmt: [%s]\n", L_stack_dump(l));

    /*
    char *dump;
    dump = table_dump2allocated_str(l);
    if (dump) {
        trace("metaloader_set_fmt: dump '%s'\n", dump);
        free(dump);
    }
    trace("metaloader_set_fmt: [%s]\n",L_stack_dump(l));
    */

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    //assert(type == LUA_TTABLE);
    if (type != LUA_TTABLE) {
        if (ml->verbose)
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

    if (ml->verbose)
        trace("metaloader_set_rename_fmt: [%s]\n", L_stack_dump(l));

    /*
    char *dump;
    dump = table_dump2allocated_str(l);
    if (dump) {
        trace("metaloader_set_fmt: dump '%s'\n", dump);
        free(dump);
    }
    trace("metaloader_set_fmt: [%s]\n",L_stack_dump(l));
    */

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    //assert(type == LUA_TTABLE);
    if (type != LUA_TTABLE) {
        if (ml->verbose)
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
        if (ml->verbose)
            trace(
                "metaloader_set_rename_fmt: not table in '%s' - '%s'\n",
                fname_noext,
                prev_objname
            );
        goto _cleanup;
    }

    if (ml->verbose)
        trace("metaloader_set_rename_fmt: [%s]\n",L_stack_dump(l));

    /*lua_pushvalue(l, -1);*/
    int ref_rect_arr = luaL_ref(l, LUA_REGISTRYINDEX);
    // [ "tank", {0, 0, 100, 100} ]
    if (ml->verbose)
        trace("metaloader_set_rename_fmt: [%s]\n",L_stack_dump(l));

    lua_pushstring(l, prev_objname);
    lua_pushnil(l);
    lua_settable(l, -3);

    if (ml->verbose)
        trace("metaloader_set_rename_fmt: [%s]\n",L_stack_dump(l));

    lua_pushstring(l, obj_name);
    lua_rawgeti(l, LUA_REGISTRYINDEX, ref_rect_arr);
    lua_settable(l, -3);

    luaL_unref(l, LUA_REGISTRYINDEX, ref_rect_arr);

    if (ml->verbose)
        trace("metaloader_set_rename_fmt: [%s]\n",L_stack_dump(l));

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
        if (ml->verbose)
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

enum ReadObjectResult {
    ROR_NONE,
    ROR_CONTINUE,
    ROR_SKIP,
};

static Rectangle *_read_object_rect(lua_State *l, bool verbose) {
    //read_object_untyped(l, object);
    assert(l);

    static Rectangle rect = {};
    if (verbose)
        trace("_read_object_rect: [%s]\n",L_stack_dump(l));
    
    lua_pushstring(l, "rect");
    int type = lua_gettable(l, -2);
    if (type != LUA_TTABLE) {
        if (verbose)
            trace("_read_object_rect: has not table on key 'rect'\n");
        lua_pop(l, 1);
        return NULL;
    }

    int i = 0;

    lua_pushnil(l);
    float values[4] = {};
    // Чтение полей значений Rectangle
    int maxiters = 10;
    while (lua_next(l, lua_gettop(l) - 1)) {
        if (lua_isnumber(l, -1)) 
            values[i++] = lua_tonumber(l, -1);
        else {
            lua_pop(l, 1);
            continue;
            //lua_pop(l, 2);
            ////return ROR_SKIP;
            //return NULL;
        }
        lua_pop(l, 1);

        if (i == 4)
            break;

        maxiters--;
        if (maxiters < 0) {
            if (verbose)
                trace("_read_object_rect: iterations limit reached\n");
            break;
        }
    }

    if (verbose)
        trace("_read_object_rect: i = %d [%s]\n", i,L_stack_dump(l));
    if (i > 0) 
        lua_pop(l, 1);
    lua_pop(l, 1);
    if (verbose)
        trace("_read_object_rect: end [%s]\n",L_stack_dump(l));

    rect = rect_from_arr(values);

    return &rect;
}

static struct MetaLoaderReturn *read_object_rect_oriented(
    lua_State *l, bool verbose
) {
    assert(l);

    lua_pushstring(l, "angle");
    int type = lua_gettable(l, -2);
    if (type != LUA_TNUMBER) {
        if (verbose)
            trace("read_object_rect_oriented: angle is not a number\n");
        //lua_pop(l, 1);
    }
    double angle = lua_tonumber(l, -1);
    lua_pop(l, 1);

    const Rectangle *rect = _read_object_rect(l, verbose);
    struct MetaLoaderRectangleOriented *ret = calloc(1, sizeof(*ret));
    assert(ret);
    ret->ret.type = MLT_RECTANGLE_ORIENTED;
    ret->a = angle;
    if (rect)
        ret->rect = *rect;
    return (struct MetaLoaderReturn*)ret;
}

static struct MetaLoaderReturn *read_object_rect(lua_State *l, bool verbose) {
    assert(l);
    struct MetaLoaderRectangle *ret = calloc(1, sizeof(*ret));
    assert(ret);
    const Rectangle *rect = _read_object_rect(l, verbose);
    if (rect)
        ret->rect = *rect;
    ret->ret.type = MLT_RECTANGLE;
    return (struct MetaLoaderReturn*)ret;
}

static struct MetaLoaderReturn *read_object_sector(
    lua_State *l, bool verbose
) {
    assert(l);
    int type;

    lua_pushstring(l, "radius");
    type = lua_gettable(l, -2);
    if (type != LUA_TNUMBER) {
        if (verbose)
            trace("read_object_sector: 'radius' key is not a number\n");
    }
    float radius = lua_tonumber(l, -1);
    lua_pop(l, 1);

    int pop_num = 1;

    lua_pushstring(l, "a1");
    type = lua_gettable(l, -2);
    if (type != LUA_TNUMBER) {
        lua_pushstring(l, "angle1");
        type = lua_gettable(l, -2);
        pop_num += 1;

        if (type != LUA_TNUMBER)
            if (verbose)
                trace(
                    "read_object_sector: 'a1' or 'angle1'"
                    "key is not a number\n"
                );
    }
    float a1 = lua_tonumber(l, -1);
    lua_pop(l, pop_num);

    pop_num = 1;
    lua_pushstring(l, "a2");
    type = lua_gettable(l, -2);
    if (type != LUA_TNUMBER) {
        lua_pushstring(l, "angle2");
        type = lua_gettable(l, -2);
        pop_num += 1;

        if (type != LUA_TNUMBER)
            if (verbose)
                trace(
                    "read_object_sector: 'a1' or 'angle1'"
                    "key is not a number\n"
                );
    }
    float a2 = lua_tonumber(l, -1);
    lua_pop(l, pop_num);

    struct MetaLoaderSector *sector = calloc(1, sizeof(*sector));
    assert(sector);
    sector->ret.type = MLT_SECTOR;
    sector->angle1 = a1;
    sector->angle2 = a2;
    sector->radius = radius;

    return (struct MetaLoaderReturn*)sector;
}

static struct MetaLoaderReturn *read_object_polyline(
    lua_State *l, bool verbose
) {
    if (verbose)
        trace("read_object_polyline: [%s]\n",L_stack_dump(l));

    lua_pushstring(l, "points");
    int type = lua_gettable(l, -2);
    if (type != LUA_TTABLE) {
        if (verbose)
            trace("read_object_polyline: type is not a table\n");
        // TODO: скинуть лишнее значение со стека и выйти из функции?
    }

    struct MetaLoaderPolyline *pl = calloc(1, sizeof(*pl));
    assert(pl);

    pl->cap = 10;
    pl->points = calloc(pl->cap, sizeof(pl->points[0]));
    assert(pl->points);

    int points_num = 0, num = 0;
    lua_pushnil(l);
    while (lua_next(l, -2)) {
        Vector2 point = {};

        if (lua_type(l, -1) != LUA_TNUMBER) {
            if (verbose)
                trace("read_object_polyline: not number in points table\n");
        } else {
            point.x = lua_tonumber(l, -1);
        }
        num++;

        lua_pop(l, 1);
        if (!lua_next(l, -2)) {
            if (verbose)
                trace(
                    "read_object_polyline: break reading points at %d num\n",
                    num
                );
            break;
        }
        num++;

        if (lua_type(l, -1) != LUA_TNUMBER) {
            if (verbose)
                trace("read_object_polyline: not number in points table\n");
        } else {
            point.y = lua_tonumber(l, -1);
        }
        lua_pop(l, 1);

        points_num++;
        if (pl->cap >= points_num) {
            pl->cap += 1;
            pl->cap *= 2;
            int bytes = sizeof(pl->points[0]) * pl->cap;
            void *new_ptr = realloc(pl->points, bytes);
            if (!new_ptr) {
                // TODO: Не выходить, а возвращать те точки, 
                // что получилось считать
                if (verbose)
                    trace(
                        "read_object_polyline: could not do realloc"
                        "with capacity %d\n",
                        pl->cap
                    );
                exit(EXIT_FAILURE);
            }
            pl->points = new_ptr;
        }

        pl->points[pl->num++] = point;
    }
    
    // */

    if (verbose)
        trace(
            "read_object_polyline: pl->num %d, [%s]\n",
            pl->num,L_stack_dump(l)
        );
    lua_pop(l, 1);

    return (struct MetaLoaderReturn*)pl;
}

static struct MetaLoaderReturn *read_object(lua_State *l, bool verbose) {
    assert(l);

    if (!lua_istable(l, -1)) {
        if (verbose)
            trace("read_object: object is not a table, skipping\n");
        return NULL;
    }

    // indices      [-4  -3  -2     -1      ]
    // Lua stack:   [.., .., table, "type"  ]
    
    //printf("read_object: [%s]\n",L_stack_dump(l));
    lua_pushstring(l, "type");
    //printf("read_object: [%s]\n",L_stack_dump(l));
    int type = lua_gettable(l, -2);
    //printf("read_object: [%s]\n",L_stack_dump(l));
    //printf("type %s\n", lua_typename(l, type));
    //printf("read_object: has type %s\n", lua_typename(l, type));

    if (type != LUA_TSTRING) {
        if (verbose)
            trace("read_object: 'type' key is not a string\n");
        lua_pop(l, 1);
        return NULL;
    }

    const char *type_str = lua_tostring(l, -1);
    if (verbose)
        trace("read_object: type_str '%s'\n", type_str);
    lua_pop(l, 1);

    struct MetaLoaderReturn *ret = NULL;

    if (!strcmp(type_str, "rect_oriented")) {
        ret = read_object_rect_oriented(l, verbose);
    } else if (!strcmp(type_str, "rect")) {
        ret = read_object_rect(l, verbose);
    } else if (!strcmp(type_str, "sector")) {
        ret = read_object_sector(l, verbose);
    } else if (!strcmp(type_str, "polyline")) {
        ret = read_object_polyline(l, verbose);
    }

    return ret;
}

struct MetaLoaderObjects2 metaloader_objects_get2(
    struct MetaLoader *ml, const char *fname_noext
) {
    assert(ml);
    struct MetaLoaderObjects2 object = {};

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
        if (ml->verbose)
            trace(
                "metaloader_objects_get: could not get table '%s' "
                "by ref_tbl_root\n",
                fname_noext
            );
        return (struct MetaLoaderObjects2){};
    }

    lua_pushnil(l);
    int tbl_index = lua_gettop(l) - 1;
    while (lua_next(l, tbl_index)) {
        if (object.num + 1 == METALOADER_MAX_OBJECTS) {
            lua_settop(l, 0);
            if (ml->verbose)
                trace("metaloader_objects_get: METALOADER_MAX_OBJECTS reached\n");
            return object;
        }

        const char *field_name = lua_tostring(l, -2);
        if (ml->verbose)
            trace("metaloader_objects_get2: field_name %s\n", field_name);
        if (!field_name) {
            if (ml->verbose)
                trace(
                    "metaloader_objects_get2: no field_name [%s]\n",
                   L_stack_dump(l)
                );
        } else {
            object.names[object.num] = strdup(field_name);
        }

        struct MetaLoaderReturn *ret = read_object(l, ml->verbose);
        if (ret)
            object.objs[object.num] = ret;
        object.num++;

        lua_pop(l, 1);
    }
    lua_settop(l, 0);

    return object;
}

void metaloader_objects_shutdown2(struct MetaLoaderObjects2 *objects) {
    assert(objects);
    //trace("metaloader_objects_shutdown: objects->num %d\n", objects->num);
    for (int j = 0; j < objects->num; ++j) {
        if (objects->names[j]) {
            free(objects->names[j]);
            objects->names[j] = NULL;
        }
    }

    for (int j = 0; j < objects->num; ++j) {
        struct MetaLoaderReturn *obj = objects->objs[j];

        if (!obj) continue;

        metaloader_return_shutdown(obj);

        free(obj);
        objects->objs[j] = NULL;
    }

    memset(objects, 0, sizeof(*objects));
}

void metaloader_return_shutdown(struct MetaLoaderReturn *ret) {
    assert(ret);
    switch (ret->type) {
        case MLT_POLYLINE: {
            struct MetaLoaderPolyline *pl = (struct MetaLoaderPolyline*)ret;
            if (pl->points) {
                free(pl->points);
                pl->points = NULL;
            }
            break;
        }
        case MLT_RECTANGLE:
            break;
        case MLT_RECTANGLE_ORIENTED:
            break;
        case MLT_SECTOR:
            break;
    }
}

// XXX: Что делать, если строка вывода не влезает в буфер и требуется
// динамическое выделение памяти?
// Как потом понять, что буфер надо освобождать?
struct MetaLoaderObject2Str metaloader_object2str(
    struct MetaLoaderReturn *obj
) {
    assert(obj);
    static char buf[256] = {};
    memset(buf, 0, sizeof(buf));
    switch (obj->type) {
        case MLT_POLYLINE: {
            struct MetaLoaderPolyline *pl;
            pl = (struct MetaLoaderPolyline*)obj;
            const char *fmt_beg = 
                "{\n"
                "   type = \"polyline\",\n"
                "   num = %d,\n"
                "   points = {\n"
                "   ";
            const char *fmt_end = 
                "\n"
                "   }\n"
                "}\n";
            int pos = snprintf(buf, sizeof(buf) - 1,  fmt_beg, pl->num);
            char *ptr = buf + pos;
            for (int i = 0; i < pl->num; i++) {
                if (!ptr)
                    break;
                ptr += sprintf(
                    ptr, "%f, %f, ", 
                    pl->points[i].x,
                    pl->points[i].y
                );
            }
            sprintf(ptr, "%s", fmt_end);
            break;
        }
        case MLT_RECTANGLE: {
            struct MetaLoaderRectangle *rect;
            rect = (struct MetaLoaderRectangle*)obj;
            const char *fmt = 
                "{\n"
                "   type = \"rect\",\n"
                "   rect = {%f, %f, %f, %f},\n"
                "}\n";
            snprintf(
                buf, sizeof(buf) - 1,  fmt, 
                rect->rect.x, rect->rect.y,
                rect->rect.width, rect->rect.height
            );
            break;
        }
        case MLT_RECTANGLE_ORIENTED: {
            struct MetaLoaderRectangleOriented *recto;
            recto = (struct MetaLoaderRectangleOriented*)obj;
            const char *fmt = 
                "{\n"
                "   type = \"rect_oriented\",\n"
                "   angle = %f,\n"
                "   rect = {%f, %f, %f, %f},\n"
                "}\n";
            snprintf(
                buf, sizeof(buf) - 1,  fmt, 
                recto->a,
                recto->rect.x, recto->rect.y,
                recto->rect.width, recto->rect.height
            );
            break;
        }
        case MLT_SECTOR: {
            struct MetaLoaderSector *sector;
            sector = (struct MetaLoaderSector*)obj;
            const char *fmt = 
                "{\n"
                "   type = \"sector\",\n"
                "   a1 = %f,\n"
                "   a2 = %f,\n"
                "   radius = %f,\n"
                "}\n";
            snprintf(
                buf, sizeof(buf) - 1,  fmt, 
                sector->angle1, sector->angle2,
                sector->radius
            );
            break;
        }
        default:
            return (struct MetaLoaderObject2Str) {
                .is_allocated = false,
                .s = NULL,
            };
    }
    return (struct MetaLoaderObject2Str) {
        .is_allocated = false,
        .s = buf,
    };
}

struct MetaLoaderReturn *metaloader_get2(
    MetaLoader *ml, const char *fname_noext, const char *objname
) {
    if (ml->verbose)
        trace("metaloader_get2:\n");
    return metaloader_get2_fmt(ml, fname_noext, "%s", objname);
}

struct MetaLoaderReturn *metaloader_get2_fmt(
    MetaLoader *ml, const char *fname_noext, const char *objname_fmt, ...
) {
    if (ml->verbose)
        trace("metaloader_get2_fmt:\n");

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
        if (ml->verbose)
            trace(
                "metaloader_get2_fmt: no such fname name '%s' in table, "
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
            "metaloader_get2_fmt: no such object '%s' in table %s",
            obj_name, fname_noext
        );
        lua_settop(l, 0);
        return NULL;
    }

    struct MetaLoaderReturn *obj = read_object(l, ml->verbose);

    lua_settop(l, 0);
    return obj;
}

void metaloader_set_fmt2_rect(
    MetaLoader *ml,
    Rectangle rect,
    const char *fname_noext, 
    const char *objname, ...
) {
    if (ml->verbose)
        trace("metaloader_set_fmt2_rect:\n");
}

void metaloader_set_fmt2_sector(
    MetaLoader *ml,
    float radius, float angle1, float angle2, Vector2 pos,
    const char *fname_noext, 
    const char *objname, ...
) {
    if (ml->verbose)
        trace("metaloader_set_fmt2_sector:\n");
}

void metaloader_set_fmt2_polyline(
    MetaLoader *ml,
    Vector2 *points, int num,
    const char *fname_noext, 
    const char *objname, ...
) {
    if (ml->verbose)
        trace("metaloader_set_fmt2_polyline:\n");

    assert(ml);
    assert(fname_noext);
    assert(objname);

    lua_State *l = ml->lua;
    lua_settop(l, 0);

    int type;
    type = lua_rawgeti(l, LUA_REGISTRYINDEX, ml->ref_tbl_root);
    assert(type == LUA_TTABLE);

    char obj_name[64] = {};
    sprintf(obj_name, "%s", objname);
    /*
    va_list args;
    va_start(args, obj_name_fmt);
    vsnprintf(obj_name, sizeof(obj_name) - 1, obj_name_fmt, args);
    va_end(args);
    */

    if (ml->verbose) 
        trace("metaloader_set_fmt2_polyline: [%s]\n",L_stack_dump(l));

    /*
    char *dump;
    dump = table_dump2allocated_str(l);
    if (dump) {
        trace("metaloader_set_fmt: dump '%s'\n", dump);
        free(dump);
    }
    trace("metaloader_set_fmt: [%s]\n",L_stack_dump(l));
    */

    lua_pushstring(l, fname_noext);
    type = lua_gettable(l, -2);
    //assert(type == LUA_TTABLE);
    if (type != LUA_TTABLE) {
        if (ml->verbose)
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
    table_push_points_as_arr(l, points, num);
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

