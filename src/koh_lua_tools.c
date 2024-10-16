// vim: set colorcolumn=85
// vim: fdm=marker
#include "koh_lua_tools.h"

#include "koh_common.h"
#include "koh_logger.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

// Максимальное количество методов в таблице. Простая защита от 
// переполнения
static const int max_fields_num = 100;
bool L_verbose = false;

TypeEntry *typelist = NULL;

DocArray doc_init(lua_State *lua, const char *mtname) {
    DocArray docarr = {0};
    //printf("doc_get: [%s]\n", stack_dump(lua));

    lua_createtable(lua, 0, 0);
    luaL_setmetatable(lua, mtname);
    int has_metatable = lua_getmetatable(lua, -1);
    //printf("has_metatable %d\n", has_metatable);

    if (!has_metatable) return docarr;
    if (lua_getfield(lua, -1, "__doc") != LUA_TTABLE) return docarr;
    
    int arr_capacity = 30;
    docarr.arr = calloc(arr_capacity, sizeof(docarr.arr[0]));

    int table_index = lua_gettop(lua);

    lua_pushnil(lua);  /* first key */

    printf("doc_get: [%s]\n", L_stack_dump(lua));
    while (lua_next(lua, table_index) != 0) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        int key_index = -2;
        int val_index = -1;
        printf("%s - %s\n",
                lua_typename(lua, lua_type(lua, key_index)),
                lua_typename(lua, lua_type(lua, val_index)));

        const char *desc = NULL, *desc_detailed = NULL;
        const char *name = lua_tostring(lua, key_index);

        if (lua_rawgeti(lua, -1, 1) == LUA_TSTRING) {
            desc = lua_tostring(lua, -1);
            lua_pop(lua, 1);
        }

        if (lua_rawgeti(lua, -1, 2) == LUA_TSTRING) {
            desc_detailed = lua_tostring(lua, -1);
            lua_pop(lua, 1);
        }

        if (name)
            docarr.arr[docarr.num].name = strdup(name);
        if (desc)
            docarr.arr[docarr.num].desc = strdup(desc);
        if (desc_detailed)
            docarr.arr[docarr.num].desc_detailed = strdup(desc_detailed);

        docarr.num++;

        if (docarr.num == arr_capacity) {
            arr_capacity *= 2;
            docarr.arr = realloc(
                docarr.arr, sizeof(docarr.arr[0]) * arr_capacity
            );
        }

        lua_pop(lua, 1);
    }

    lua_settop(lua, 0);
    return docarr;
}

void doc_shutdown(DocArray *docarr) {
    assert(docarr);
    if (!docarr->arr) return;

    for (int i = docarr->num - 1; i >= 0; i--) {
        if (docarr->arr[i].desc_detailed) {
            free(docarr->arr[i].desc_detailed);
            docarr->arr[i].desc_detailed = NULL;
        }
        if (docarr->arr[i].name) {
            free(docarr->arr[i].name);
            docarr->arr[i].name = NULL;
        }
        if (docarr->arr[i].desc) {
            free(docarr->arr[i].desc);
            docarr->arr[i].desc = NULL;
        }
    }

    free(docarr->arr);
    docarr->arr = NULL;
}

void L_table_print(lua_State *lua, int idx) {

    if (lua_type(lua, idx) != LUA_TTABLE)
        return;

    lua_pushnil(lua);  /* first key */
    while (lua_next(lua, idx) != 0) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        int tmp_idx = -2;

        int t = lua_type(lua, tmp_idx);
        switch (t) {
            case LUA_TSTRING: 
                printf("'%s'", lua_tostring(lua, tmp_idx));
                break;
            case LUA_TBOOLEAN: 
                printf("%s", lua_toboolean(lua, tmp_idx) ? "true" : "false");
                break;
            case LUA_TNUMBER: 
                printf("%g", lua_tonumber(lua, tmp_idx));
                break;
            case LUA_TFUNCTION: 
                printf("%p\n", lua_topointer(lua, tmp_idx));
                break;
            case LUA_TTABLE:
                printf("%p\n", lua_tostring(lua, tmp_idx));
                break;
            default: 
                printf("%s", lua_typename(lua, t));
                break;
        }

        printf(" = ");

        tmp_idx = -1;
        t = lua_type(lua, tmp_idx);
        switch (t) {
            case LUA_TSTRING: 
                printf("%s\n", lua_tostring(lua, tmp_idx));
                break;
            case LUA_TBOOLEAN: 
                printf("%s\n", lua_toboolean(lua, tmp_idx) ? "true" : "false");
                break;
            case LUA_TNUMBER: 
                printf("%g\n", lua_tonumber(lua, tmp_idx));
                break;
            case LUA_TFUNCTION: 
                printf("function(%p)\n", lua_topointer(lua, tmp_idx));
                /*printf("%p\n", lua_tostring(lua, tmp_idx));*/
                break;
            default: 
                printf("%s\n", lua_typename(lua, t));
                break;
        }

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(lua, 1);
    }

    /*printf("print_table: [%s]\n", stack_dump(lua));*/
}

void table_print(lua_State *lua, int idx) {
    L_table_print(lua, idx);
}

// TODO: Сделать рекурсивную распачатку таблицы
char *L_table_get_print(
    lua_State *lua, int idx, const struct TablePrintOpts *opts
) {
    static char buf[2048] = {};
    // Статические переменные на случай реализацити рекурсивного вызова
    static char *pbuf;
    static int buf_used;

    buf_used = 0;
    pbuf = buf;

    if (lua_type(lua, idx) != LUA_TTABLE)
        return buf;

    lua_pushnil(lua);  /* first key */
    while (lua_next(lua, idx) != 0) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        int tmp_idx = -2;

        int t = lua_type(lua, tmp_idx);

        if (opts && opts->tabulate) {
            int num = sprintf(pbuf, "%s", "    ");
            buf_used += num;
            if (buf_used + 1 == sizeof(buf))
                return buf;
            pbuf += num;
        }

        switch (t) {
            int num;
            // {{{ parse type
            case LUA_TSTRING:
                num = sprintf(pbuf, "%s", lua_tostring(lua, tmp_idx));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            case LUA_TBOOLEAN: 
                num = sprintf(
                    pbuf, "%s", lua_toboolean(lua, tmp_idx) ? "true" : "false"
                );
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += buf_used;
                break;
            case LUA_TNUMBER: 
                num = sprintf(pbuf, "%g", lua_tonumber(lua, tmp_idx));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            case LUA_TFUNCTION: 
                num = sprintf(pbuf, "%p\n", lua_topointer(lua, tmp_idx));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            case LUA_TTABLE:
                num = sprintf(pbuf, "%p\n", lua_tostring(lua, tmp_idx));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            default: 
                num = sprintf(pbuf, "%s", lua_typename(lua, t));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            // }}}
        }

        int n = sprintf(pbuf, " = ");
        buf_used += n;
        if (buf_used + 1 == sizeof(buf))
            return buf;
        pbuf += n;

        tmp_idx = -1;
        t = lua_type(lua, tmp_idx);
        switch (t) {
            int num;
            // {{{ parse value
            case LUA_TSTRING: 
                num = sprintf(pbuf, "%s,\n", lua_tostring(lua, tmp_idx));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            case LUA_TBOOLEAN: 
                num = sprintf(pbuf, "%s,\n", lua_toboolean(lua, tmp_idx) ? "true" : "false");
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            case LUA_TNUMBER: 
                num = sprintf(pbuf, "%g,\n", lua_tonumber(lua, tmp_idx));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            case LUA_TFUNCTION: 
                num = sprintf(pbuf, "function(%p),\n", lua_topointer(lua, tmp_idx));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            default: 
                num = sprintf(pbuf, "%s,\n", lua_typename(lua, t));
                buf_used += num;
                if (buf_used + 1 == sizeof(buf))
                    return buf;
                pbuf += num;
                break;
            // }}}
        }

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(lua, 1);
    }

    /*printf("print_table: [%s]\n", stack_dump(lua));*/
    return buf;
}

char *table_get_print(
    lua_State *lua, int idx, const struct TablePrintOpts *opts
) {
    return L_table_get_print(lua, idx, opts);
}

static void type_alloc(const char *mtname, const Reg_ext *methods) {
    assert(mtname);
    assert(methods);

    TypeEntry *new = calloc(1, sizeof(*new));
    assert(new);

    new->next = typelist;
    new->mtname = strdup(mtname);

    Reg_ext *cur = (Reg_ext*)methods - 1;
    while (true) {
        if (max_fields_num <= 0) {
            trace(
                "type_alloc: %d max_fields_num methods reached in "
                "table '%s'", max_fields_num, mtname
            );
            break;
        }
        cur++;
        if (!cur->name) break;

        if (cur->desc)
            new->reg.desc = strdup(cur->desc);
        if (cur->desc_detailed)
            new->reg.desc_detailed = strdup(cur->desc_detailed);
        if (cur->name)
            new->reg.name = strdup(cur->name);
        if (cur->desc_detailed)
            new->reg.func = cur->func;
    }

    typelist = new;
}

static void fill_methods_table(
    lua_State *lua, const char *mtname, const Reg_ext *methods
) {
    Reg_ext *cur = (Reg_ext*)methods - 1;
    luaL_newmetatable(lua, mtname);

    lua_pushvalue(lua, -1);
    lua_setfield(lua, -2, "__index");

    lua_pushvalue(lua, -2);
    lua_setfield(lua, -2, "__doc");

    /*lua_remove(lua, -2);*/

    while (true) {
        if (max_fields_num <= 0) {
            trace(
                "fill_methods_table: %d max_fields_num methods reached in "
                "table '%s'", max_fields_num, mtname
            );
            break;
        }
        cur++;
        if (!cur->name) {
            break;
        }
        lua_pushcclosure(lua, cur->func, 0);
        lua_setfield(lua, -2, cur->name);
        //printf("cur->func, cur->name %p, %s\n", cur->func, cur->name);
    }
}

static void fill_help_table(
    lua_State *lua, const char *mtname, const Reg_ext *methods
) {
    // табличка со справкой
    lua_createtable(lua, 0, 0);
    Reg_ext *cur = (Reg_ext*)methods - 1;

    while (true) {
        if (max_fields_num<= 0) {
            trace(
                "fill_help_table: %d max_fields_num methods reached in "
                "table '%s'", max_fields_num, mtname
            );
            break;
        }
        cur++;
        if (!cur->name) {
            break;
        }

        lua_createtable(lua, 0, 0);
        lua_pushstring(lua, cur->desc);
        lua_seti(lua, -2, 1);
        lua_pushstring(lua, cur->desc_detailed);
        lua_seti(lua, -2, 2);

        lua_setfield(lua, -2, cur->name);
    }
}

void sc_register_methods_and_doc(
    lua_State *lua,
    const char *mtname,
    const Reg_ext *methods
) {
    if (L_verbose)
        trace(
            "register_methods_and_doc: '%s' [%s]\n", mtname, L_stack_dump(lua)
        );

    /*
    // {{{
    local mt = {}
    mt.__index = mt.__index

    mt.__doc = {}
    mt.__doc.func1 = {}
    mt.__doc.func1[1] = "короткое описание"
    mt.__doc.func1[2] = "описание с примером"

    mt.func1 = function(self, arg1, arg2, ..)
    end

    mt.func2 = function(self, arg1, arg2, ..)
    end

    local obj = setmetatable({}, mt)
    obj:func1()
    // }}}
     */

    type_alloc(mtname, methods);

    fill_help_table(lua, mtname, methods);
    fill_methods_table(lua, mtname,  methods);

    lua_pop(lua, 2);
    //printf("register_methods_and_doc: [%s]\n", stack_dump(lua));
}

const char *stack_dump(lua_State *lua) {
    return L_stack_dump(lua);
}

void types_init() {
    typelist = NULL;
}

void types_shutdown() {
    int i = 0;
    TypeEntry *curr = typelist;
    while (curr) {
        TypeEntry *next = curr->next;
        if (curr->mtname) {
            free(curr->mtname);
            curr->mtname = NULL;
        }
        if (curr->reg.desc) {
            free(curr->reg.desc);
            curr->reg.desc = NULL;
        }
        if (curr->reg.desc_detailed) {
            free(curr->reg.desc_detailed);
            curr->reg.desc_detailed = NULL;
        }
        if (curr->reg.name) {
            free(curr->reg.name);
            curr->reg.name = NULL;
        }
        free(curr);
        curr = next;
        i++;
    }
    if (L_verbose)
        trace("types_shutdown: %d type freed\n", i);
    typelist = NULL;
}

TypeEntry *types_getlist() {
    return typelist;
}

//#include "koh_lua_serpent.inc"
#include "serpent.lua.h"

// XXX: Протестировать
char *L_table_dump2allocated_str(lua_State *l) {
    // TODO:
    // Проверить, модуль serpent загружен
    // Если модуль отсутствует, то загрузить его из строки

    int top = lua_gettop(l);

    lua_getglobal(l, "package");
    //printf("L_table_dump2allocated_str: 0 [%s]\n", L_stack_dump(l));
    lua_getfield(l, -1, "preload");
    /*printf("L_table_dump2allocated_str: 1 [%s]\n", L_stack_dump(l));*/

    /*
    printf(
        "L_table_dump2allocated_str: strlen(serpent_lua) == %zu\n",
        strlen(serpent_lua)
    );
    */

    if (luaL_loadstring(l, (char*)serpent_lua) != LUA_OK) {
        printf(
            "L_table_dump2allocated_str: luaL_loadstring error '%s'\n",
            lua_tostring(l, -1)
        );
    }

    /*printf("L_table_dump2allocated_str: 2 [%s]\n", L_stack_dump(l));*/
    lua_setfield(l, -2, "serpent");
    /*printf("L_table_dump2allocated_str: 3 [%s]\n", L_stack_dump(l));*/

    lua_settop(l, top);

    const char *code =  "local function DUMP(tbl)\n"
                        "   local serpent\n"
                        "   local ok, errmsg = pcall(function()\n"
                        "       serpent = require 'serpent'\n"
                        "   end)\n"
                        "   if ok then\n"
                        "       return serpent.dump(tbl)\n"
                        "   else\n"
                        // XXX: Отдадочная печать!
                        "       --print(errmsg)\n"
                        "       return 171\n"
                        "   end\n"
                        "end\n"
                        "return DUMP";

    //printf("L_table_dump2allocated_str: [%s]\n", stack_dump(l));
    //int top = lua_gettop(l);

    // XXX: Что делать с принудительной загрузкой библиотек?
    // Кажется, что загрузка может занимать время и не всегда желательно
    // наличие библиотек в виртуальной машине по причинам безопасности.
    luaL_openlibs(l);

    luaL_loadstring(l, code);
    lua_call(l, 0, LUA_MULTRET);

    lua_pushvalue(l, -2);

    lua_call(l, 1, LUA_MULTRET);

    if (lua_type(l, -1) != LUA_TSTRING) {
        lua_pop(l, 1);
        return NULL;
    }
    const char *dumped_data = lua_tostring(l, -1);
    /*printf("L_table_dump2allocated_str: [%s]\n", stack_dump(l));*/
    if (dumped_data) {
        lua_pop(l, 1);
        //printf("L_table_dump2allocated_str: [%s]\n", stack_dump(l));
        /*assert(lua_gettop(l) == top);*/
        /*lua_pop(l, 1);*/
        return strdup(dumped_data);
    }

    lua_pop(l, 1);
    return NULL;
}

char *table_dump2allocated_str(lua_State *l) {
    return L_table_dump2allocated_str(l);
}

void L_table_push_points_as_arr(lua_State *l, Vector2 *points, int points_num) {
    assert(l);
    assert(points);
    assert(points_num >= 0);

    lua_createtable(l, points_num, 0);

    int j = 1;

    char *buf = points2table_allocated(points, points_num);
    trace("table_push_points_as_arr: %s\n", buf);
    free(buf);

    for (int i = 0; i < points_num; i++) {
        lua_pushnumber(l, points[i].x);
        lua_rawseti(l, -2, j++);

        lua_pushnumber(l, points[i].y);
        lua_rawseti(l, -2, j++);
    }
}

void table_push_points_as_arr(lua_State *l, Vector2 *points, int points_num) {
    L_table_push_points_as_arr(l, points, points_num);
}

void L_table_push_rect_as_arr(lua_State *l, Rectangle rect) {
    assert(l);
    lua_createtable(l, 0, 0);

    lua_pushnumber(l, rect.x);
    lua_rawseti(l, -2, 1);

    lua_pushnumber(l, rect.y);
    lua_rawseti(l, -2, 2);

    lua_pushnumber(l, rect.width);
    lua_rawseti(l, -2, 3);

    lua_pushnumber(l, rect.height);
    lua_rawseti(l, -2, 4);
}

void table_push_rect_as_arr(lua_State *l, Rectangle rect) {
    return L_table_push_rect_as_arr(l, rect);
}

const char *L_stack_dump(lua_State *lua) {
    if (!lua) 
        return NULL;
    
    // TODO: Проверка на вместимость во внутреннего буфера
    static char ret[1024] = {0, };
    memset(ret, 0, sizeof(ret));
    char *ptr = ret;
    int top = lua_gettop(lua);
    for (int i = 1; i <= top; i++) {
        int t = lua_type(lua, i);
        int max_chars = sizeof(ret) - (ptr - ret) - 1; // -1 is random value
        switch (t) {
            case LUA_TUSERDATA: 
                ptr += snprintf(
                    ptr, max_chars, "userdata %p", lua_topointer(lua, i)
                );
                break;
            case LUA_TSTRING: 
                ptr += snprintf(ptr, max_chars, "’%s’", lua_tostring(lua, i));
                break;
            case LUA_TBOOLEAN: 
                ptr += snprintf(ptr, max_chars, lua_toboolean(lua, i) ? "true" : "false");
                break;
            case LUA_TNUMBER: 
                ptr += snprintf(ptr, max_chars, "%g", lua_tonumber(lua, i));
                break;
            default: 
                ptr += snprintf(ptr, max_chars, "%s", lua_typename(lua, t));
                break;
        }
        ptr += snprintf(ptr, max_chars, " "); 
    }
    return ret;
}

static const char *L_tabular_print_internal(
    lua_State *l, const char *global_name
) {
    assert(l);
    assert(global_name);

    const char *s = 
        // TODO: добавить код tabular.lua в включаемый файл
        // Как-то добавить включение koh_lua_tabular.h
        "package.path = package.path .. ';/usr/share/lua/5.4/?.lua'\n"
        "T = require 'tabular'\n"
        "return T(";
    const char *e = ")\n";
    char chunk[1024 * 10] = {};

//  #include "koh_lua_tabular.h"
//  luaL_dostring(fileTable[0].data);

    strcat(chunk, s);
    strcat(chunk, global_name);
    strcat(chunk, e);

    if (luaL_dostring(l, chunk) != LUA_OK) {
        printf("L_tabular: '%s'\n", lua_tostring(l, -1));
    }

    assert(lua_isstring(l, -1));
    return lua_tostring(l, -1);
}

void L_tabular_print(lua_State *l, const char *global_name) {
    const char *t = L_tabular_print_internal(l, global_name);
    assert(t);
    printf("%s", t);
}

char *L_tabular_alloc(lua_State *l, const char *global_name) {
    const char *t = L_tabular_print_internal(l, global_name);
    assert(t);
    return strdup(t);
}

char *L_tabular_alloc_s(lua_State *l, const char *lua_str) {
    const char *t = L_tabular_print_internal(l, lua_str);
    assert(t);
    return strdup(t);
}
