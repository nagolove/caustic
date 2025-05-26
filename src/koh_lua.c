// vim: set colorcolumn=85
// vim: fdm=marker
#include "koh_lua.h"

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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lauxlib.h"
#include "lua.h"
//#include "koh_console.h"
#include "koh_inotifier.h"
#include "koh_logger.h"
#include "koh_lua.h"
#include "raylib.h"


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

// TODO: Сделать рекурсивную распeчатку таблицы
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

    const Reg_ext *cur = (const Reg_ext*)methods - 1;
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
    const Reg_ext *cur = (const Reg_ext*)methods - 1;
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
    const Reg_ext *cur = (const Reg_ext*)methods - 1;

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

char *L_table_serpent_alloc(lua_State *l, enum L_DumpError *err) {
    assert(l);

    int top = lua_gettop(l);

    // TODO: Проверить, модуль serpent загружен. 
    // Если модуль отсутствует, то загрузить его из строки
    // Зачем lua_getglobal() и lua_getfield() ?
    lua_getglobal(l, "package");
    lua_getfield(l, -1, "preload");

    // XXX: Что делать с принудительной загрузкой библиотек?
    // Кажется, что загрузка может занимать время и не всегда желательно
    // наличие библиотек в виртуальной машине по причинам безопасности.
    luaL_openlibs(l);

    if (luaL_loadstring(l, (const char*)serpent_lua) != LUA_OK) {
        printf(
            "L_table_dump2allocated_str: luaL_loadstring error '%s'\n",
            lua_tostring(l, -1)
        );
        if (err) {
            *err = L_DE_SERPENT;
            return NULL;
        }
    }

    lua_setfield(l, -2, "serpent");
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

    luaL_loadstring(l, code);
    lua_call(l, 0, LUA_MULTRET);

    lua_pushvalue(l, -2);
    lua_call(l, 1, LUA_MULTRET);

    if (lua_type(l, -1) != LUA_TSTRING) {
        lua_pop(l, 1);
        if (err)
            *err = L_DE_BADTYPE;
        return NULL;
    }

    const char *dumped_data = lua_tostring(l, -1);
    if (dumped_data) {
        char *ret = strdup(dumped_data);
        lua_pop(l, 1);
        if (err) 
            *err = 0;
        return ret;
    }

    lua_pop(l, 1);
        if (err) 
            *err = L_DE_BADTYPE;
    return NULL;
}

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

    if (luaL_loadstring(l, (const char*)serpent_lua) != LUA_OK) {
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

/*
char *table_dump2allocated_str(lua_State *l) {
    return L_table_dump2allocated_str(l);
}
*/

void L_table_push_points_as_arr(lua_State *l, Vector2 *points, int points_num) {
    assert(l);
    assert(points);
    assert(points_num >= 0);

    lua_createtable(l, points_num, 0);

    int j = 1;

    char *buf = points2table_alloc(points, points_num);
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


// TODO: Для каждого указателя на состояние хранить указатель на rlwr_t 
/*static rlwr_t *rlwr[128] = {};*/

lua_State *L_newstate() {
    lua_State *l = NULL;
#ifdef KOH_DEP_RLWR
    rlwr = rlwr_new();
    lua = rlwr_state(rlwr);
#else
    l = luaL_newstate();
#endif
    //trace("sc_init: lua version %f\n", lua_version(l));
    luaL_openlibs(l);
    return l;
}

void L_free(lua_State *l) {
}

void L_call(lua_State *l, const char *func_name, bool *is_ok) {
    lua_getglobal(l, func_name);  // Получаем функцию update() из глобальной области Lua
    lua_call(l, 0, 0);
}


const char *L_pcall(lua_State *l, const char *func_name, bool *is_ok) {
    static char slots[5][512] = {};
    static int i = 0;
    char *buf = slots[i];

    i = (i + i) % 5;

    strcpy(buf, "");

    if (!l) {
        return buf;
    }

    lua_getglobal(l, func_name);  // Получаем функцию update() из глобальной области Lua

    if (is_ok)
        *is_ok = false;

    if (lua_pcall(l, 0, 0, 0) != LUA_OK) { // Вызываем без аргументов, без возврата
        if (is_ok)
            *is_ok = false;
        strncpy(buf, lua_tostring(l, -1), sizeof(slots[0]));
        lua_pop(l, 1);
    }

    return buf;
}

#ifdef KOH_DEP_RLWR
#include "rlwr.h"
#endif

/*
#if __linux__ && KOH_DEP_NO_LFS
#include "lfs.h"
#endif
*/

typedef struct Script {
    char fname[300];
    struct Script *next;
} Script;

#ifdef KOH_DEP_RLWR
static rlwr_t *rlwr;
#endif

static ScriptFunc *script_funcs = NULL;
static lua_State *lua = NULL;
static Script *scripts = NULL;
static const char *init_fname = "assets/init.lua";
static int ref_functions_desc = -1;
static int ref_require = 0;
static int ref_print = 0;
static int ref_types_table = 0;
static bool verbose = false;

int l_script(lua_State *lua);
static void hook(lua_State *lua, lua_Debug *ar);
void sc_init_script();

struct Pair {
    const char *name, *desc;
};

static int cmp(const void *a, const void * b) {
    const char *_a = ((const struct Pair*)a)->name,
               *_b = ((const struct Pair*)b)->name;
    return strcmp(_a, _b);
}

void sc_register_all_functions(void) {
    assert(ref_types_table);
    ScriptFunc *curr = script_funcs;
    while (curr) {
        lua_rawgeti(lua, LUA_REGISTRYINDEX, ref_types_table);
        lua_pushstring(lua, curr->fname);
        lua_pushcclosure(lua, curr->func, 0);
        lua_settable(lua, -3);

        sc_register_func_desc(curr->fname, curr->desc);
        curr = curr->next;
    }
}

void sc_register_function(lua_CFunction f, const char *fname, const char *desc) {
    assert(f);
    assert(fname);
    assert(desc);

    if (!lua)
        return;

    ScriptFunc *sfunc = calloc(1, sizeof(*sfunc));
    strncpy(sfunc->desc, desc, sizeof(sfunc->desc) - 1);
    strncpy(sfunc->fname, fname, sizeof(sfunc->fname) - 1);
    sfunc->next = script_funcs;
    script_funcs = sfunc;

    assert(ref_types_table);
    if (verbose)
        trace("register_function: '%s' [%s]\n", fname, L_stack_dump(lua));
    lua_rawgeti(lua, LUA_REGISTRYINDEX, ref_types_table);
    lua_pushstring(lua, fname);
    lua_pushcclosure(lua, f, 0);
    lua_settable(lua, -3);
    lua_pop(lua, 1);
    if (verbose)
        trace("register_function: [%s]\n", L_stack_dump(lua));

    /*lua_register(lua, fname, f);*/
    sc_register_func_desc(fname, desc);
}

void print_avaible_functions(lua_State *lua) {
    int cap = 30, len = 0;
    struct Pair *pairs = calloc(cap, sizeof(pairs[0]));

    printf("print_avaible_functions: Следущие функции доступны из консоли:\n");

    lua_rawgeti(lua, LUA_REGISTRYINDEX, ref_functions_desc);
    int table_index = lua_gettop(lua);

    lua_pushnil(lua);
    while(lua_next(lua, table_index)) {
        const char *fname = lua_tostring(lua, -2);
        const char *descript = lua_tostring(lua, -1);

        pairs[len].desc = descript;
        pairs[len].name = fname;
        len++;

        if (len == cap) {
            cap *= 2;
            void *new_pairs = realloc(pairs, sizeof(pairs[0]) * cap);

            if (!new_pairs) {
                printf("Could not realloc() in print_avaible_functions()\n");
                exit(EXIT_FAILURE);
            } else
                pairs = new_pairs;
        }

        lua_pop(lua, 1);
    }

    lua_settop(lua, 0);

    qsort(pairs, len, sizeof(struct Pair), cmp);
    for (int i = 0; i < len; ++i) {
        struct Pair *p = &pairs[i];
        printf("%s = %s", p->name, p->desc);
    }

    free(pairs);
}

/*
void print_type_methods(lua_State *lua, const char *type) {
    const char *mtname = lua_tostring(lua, 1);
    DocArray docarr = doc_init(lua, mtname);
    for (int i = 0; i < docarr.num; i++) {
        DocField *docfield = &docarr.arr[i];
        console_buf_write_c(
                BLACK, "%s: %s", docfield->name, docfield->desc
                );
        if (docfield->desc_detailed)
            console_buf_write_c(
                BLACK, "   %s", docfield->desc_detailed
            );
        console_buf_write(" ");
    }
    doc_shutdown(&docarr);
}
*/

/*
void print_avaible_types(lua_State *lua) {
    printf("print_avaible_types: [%s]\n", L_stack_dump(lua));

    TypeEntry *curr = types_getlist();
    while (curr) {
        console_buf_write_c(BLACK, "%s", curr->mtname);
        curr = curr->next;
    }
}
*/

/*
int l_help(lua_State *lua) {
    printf("[%s]\n", L_stack_dump(lua));

    int argsnum = lua_gettop(lua);

    if (argsnum == 1) {
        const char *arg = lua_tostring(lua, 1); 

        console_buf_write_c(MAGENTA, "Список доступных функций");

        if (!strcmp(arg, "functions") || 
            !strcmp(arg, "funcs") || 
            !strcmp(arg, "f"))
            print_avaible_functions(lua);
        else if (!strcmp(arg, "types") ||
                 !strcmp(arg, "t"))
            print_avaible_types(lua);
        else
            print_type_methods(lua, arg);

    } else {
        console_buf_write_c(
            MAGENTA, "help('functions') - список доступных функций"
        );
        console_buf_write_c(
            MAGENTA, "help('types') - список типов для которых доступны справка"
        );
        //console_buf_write_c(BLUE, "Примеры команды:");
        //console_buf_write_c(BLUE, "help('Tank')");
    }

    printf("[%s]\n", L_stack_dump(lua));
    return 0;
}
*/

int l_GetTime(lua_State *l) {
    lua_pushnumber(l, GetTime());
    return 1;
}

int l_GetFrameTime(lua_State *l) {
    lua_pushnumber(l, GetFrameTime());
    return 1;
}

int l_GetScreenWidth(lua_State *lua) {
    lua_pushnumber(lua, GetScreenWidth());
    return 1;
}

int l_GetScreenHeight(lua_State *lua) {
    lua_pushnumber(lua, GetScreenHeight());
    return 1;
}

int l_GetScreenDimensions(lua_State *lua) {
    lua_pushnumber(lua, GetScreenWidth());
    lua_pushnumber(lua, GetScreenHeight());
    return 2;
}

int l_export_doc(lua_State *lua) {
    //XXX: place your code here
    return 0;
}

static int l_trace(lua_State *lua) {
    if (!lua_isboolean(lua, 1)) 
        return 0;

    if (lua_toboolean(lua, 1)) {
        lua_sethook(lua, hook, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
    } else {
        lua_sethook(lua, NULL, LUA_MASKCALL | LUA_MASKLINE | LUA_MASKRET, 0);
    }

    return 0;
}

void register_internal() {
    sc_register_function(
        l_trace,
        "trace",
        "trace(state: boolean) - включить или выключить построчную отладку"
        "сценария"
    );

    /*
    sc_register_function(
        l_help, "help", "List of global registered functions."
    );
    */

    //register_function(l_con_print, "con_print", "Print to console");
    sc_register_function(
        l_GetTime,
        "GetTime",
        "Возвращает время с начала запуска программы"
    );
    sc_register_function(
        l_GetFrameTime,
        "GetFrameTime",
        "Возвращает время за которое был нарисован прошлый кадр"
    );

    sc_register_function(
        l_GetScreenWidth,
        "GetScreenWidth",
        "Возвращает ширину экрана в пикселях"
    );
    sc_register_function(
        l_GetScreenHeight,
        "GetScreenHeight",
        "Возвращает высоту экрана в пикселях"
    );
    sc_register_function(
        l_GetScreenDimensions,
        "GetScreenDimensions",
        "Возвращает ширину и высоту экрана в пикселях"
    );

    const char *desc = 
        "Вывести в файл список зарегистрированных в Lua функций"
        "и их документацию";
    sc_register_function(
        l_export_doc,
        "export_doc",
        desc
    );

    desc =  "Загрузить и выполнить скрипт,"
        "который будет добавлен при сохранении состояния";

    sc_register_function(
        l_script,
        "script",
        desc
    );
}

// Попытка переопределить функцию загрузки модулей для последующей "горячей"
// перезагрузки через inotifier()
static int require_reload(lua_State *lua) {
    if (verbose)
        trace("require_reload:\n");
    int stack_pos1 = lua_gettop(lua);
    lua_rawgeti(lua, LUA_REGISTRYINDEX, ref_require);
    lua_pushvalue(lua, -2);
    lua_call(lua, 1, LUA_MULTRET);
    int stack_pos2 = lua_gettop(lua);
    int stack_diff = stack_pos2 - stack_pos1;
    return stack_diff;
}

static void require_redefine() {
    if (verbose)
        trace("require_redefine: [%s]\n", sc_stack_dump());
    int type = lua_getglobal(lua, "require");
    if (type != LUA_TFUNCTION) {
        trace("require_redefine: bad require type\n");
        return;
    }
    ref_require = luaL_ref(lua, LUA_REGISTRYINDEX);
    lua_pushcclosure(lua, require_reload, 0);
    lua_setglobal(lua, "require");
    if (verbose) {
        trace("ref_require %d\n", ref_require);
        trace("require_redefine: [%s]\n", sc_stack_dump());
    }
}

static int print_with_filter(lua_State *lua) {
    //printf("print_with_filter:\n");
    char out_buf[2048] = {0};
    const int reserve = 10; // Запас буфера для перевода строки в конце печати
    int out_buf_size = 0;
    int n = lua_gettop(lua);  /* number of arguments */
    int i;
    for (i = 1; i <= n; i++) {  /* for each argument */
        size_t l;
        const char *s = luaL_tolstring(lua, i, &l);  /* convert it to string */

        if (out_buf_size + l + reserve < sizeof(out_buf)) {
            if (i > 1) { /* not the first element? */
                strcat(out_buf, "\t");  /* add a tab before it */
                out_buf_size++;
            }
            strncat(out_buf, s, l);  /* print it */
            out_buf_size += l;
            lua_pop(lua, 1);  /* pop result */
        }
    }
    strncat(out_buf, "\n", sizeof(out_buf) - 2);
    trace("%s", out_buf);
    return 0;
}

static void print_redefine() {
    printf("print_redefine: [%s]\n", sc_stack_dump());
    int type = lua_getglobal(lua, "print");
    if (type != LUA_TFUNCTION) {
        printf("print_redefine: bad print type\n");
        return;
    }
    ref_print = luaL_ref(lua, LUA_REGISTRYINDEX);
    lua_pushcclosure(lua, print_with_filter, 0);
    lua_setglobal(lua, "print");
    //printf("ref_print %d\n", ref_print);
    //printf("print_redefine: [%s]\n", sc_stack_dump());
}

static int open_types(lua_State *lua) {
    lua_rawgeti(lua, LUA_REGISTRYINDEX, ref_types_table);
    return 1;
}

void sc_init(void) {
#ifdef KOH_DEP_RLWR
    rlwl = rlwr_new();
    lua = rlwr_state(rlwr);
#else
    lua = luaL_newstate();
#endif
    trace("sc_init: lua version %f\n", lua_version(lua));
    luaL_openlibs(lua);

/*
#if __linux__ && KOH_DEP_NO_LFS
    luaopen_lfs(lua);
#endif
*/

    printf("[%s]\n", L_stack_dump(lua));
    lua_createtable(lua, 0, 0);
    ref_functions_desc = luaL_ref(lua, LUA_REGISTRYINDEX);

    lua_createtable(lua, 0, 0);
    ref_types_table = luaL_ref(lua, LUA_REGISTRYINDEX);
    lua_rawgeti(lua, LUA_REGISTRYINDEX, ref_types_table);

    luaL_requiref(lua, "types", open_types, true);
    lua_pop(lua, 1);
    /*lua_setglobal(lua, "types");*/

    register_internal();

    print_redefine();
    require_redefine();
    printf("sc_init [%s]\n", L_stack_dump(lua));
}

void sc_shutdown() {
    ScriptFunc *curr = script_funcs;
    while (curr) {
        ScriptFunc *next = curr->next;
        free(curr);
        curr = next;
    }
    script_funcs = NULL;

    Script *cur = scripts;
    while (cur) {
        Script *next = cur->next;
        free(cur);
        cur = next;
    }
    cur = NULL;

    if (lua) {
#ifdef KOH_DEP_RLWR
        rlwr_free(rlwr);
#else
        lua_close(lua);
#endif
        lua = NULL;
    }
}

lua_State *sc_get_state() {
    /*assert(lua);*/
    return lua;
}

static void load_init_script() {
    if (luaL_dofile(lua, init_fname) != LUA_OK) {
        trace(
            "load_init_script: could not do lua init file '%s' with [%s].\n",
            init_fname,
            lua_tostring(lua, -1)
        );
        lua_settop(lua, 0);
    } else
        trace(
            "load_init_script: loading %s was failed with %s\n",
            init_fname, lua_tostring(lua, lua_gettop(lua))
        );
}

void reload_init_script(const char *fname, void *data) {
    trace("reload_init_script:\n");
    load_init_script();
    inotifier_watch(fname, reload_init_script, NULL);
}

void sc_init_script() {
    inotifier_watch(init_fname, reload_init_script, NULL);
    load_init_script();
}

void save_scripts(FILE *file) {
    fprintf(file, "-- {{{ scripts\n");
    Script *cur = scripts;
    while (cur) {
        fprintf(file, "script '%s'\n", cur->fname);
        cur = cur->next;
    }
    fprintf(file, "-- }}} scripts\n");
}

int l_script(lua_State *lua) {
    if (lua_isstring(lua, 1)) {
        const char *fname = lua_tostring(lua, 1);
        Script *scr = calloc(1, sizeof(Script));
        strcpy(scr->fname, fname);
        scr->next = scripts;
        scripts = scr;
    }
    return 0;
}

void sc_register_func_desc(const char *funcname, const char *description) {
    assert(funcname);
    assert(description);

    if (!lua) {
        trace("sc_register_func_desc: lua == NULL\n");
        return;
    }

    if (verbose)
        trace(
            "sc_register_func_desc: func '%s', desc %s\n", 
            funcname, description
        );

    //int top = lua_gettop(lua);

    //printf("common_register_function_desc [%s]\n", stack_dump(cmn.lua));
    int type = lua_rawgeti(lua, LUA_REGISTRYINDEX, ref_functions_desc);
    if (type == LUA_TTABLE) {
        //trace("sc_register_func_desc: 1 [%s]\n", stack_dump(lua));
        lua_pushstring(lua, funcname);
        lua_pushstring(lua, description);
        lua_settable(lua, -3);

        /*
        trace(
            "sc_register_func_desc: 2 [%s]\n",
            stack_dump(lua)
        );
        */

        // TODO: Аккуратно очистить стек, не весь
        /*lua_pop(lua, 1);*/
        /*lua_settop(lua, 0);*/
        /*
        int new_top = lua_gettop(lua);
        int diff_top = new_top - top;
        if (diff_top > 0) {
            trace("sc_register_func_desc: pop %d elements\n", diff_top);
            //lua_pop(lua, diff_top);
        }
        */
        
        lua_settop(lua, 0);
    } else {
        trace("sc_register_func_desc: there is no cmn.ref_functions_desc\n");
    }
    //printf("[%s]\n", stack_dump(cmn.lua));
}

uint32_t read_id(lua_State *lua) {
    assert(lua);
    uint32_t id = 0;
    lua_pushstring(lua, "id");
    lua_gettable(lua, -2);
    if (lua_isnumber(lua, -1)) {
        id = lua_tonumber(lua, -1);
        if (id < 0. || id >= UINT32_MAX) {
            id = 0;
        }
    }
    lua_remove(lua, -1);
    return id;
}

Rectangle read_rect(lua_State *lua, int index, bool *err) {
    if (err) *err = false;

    if (!lua_istable(lua, index)) {
        if (err) *err = true;
        return (Rectangle){ 0, 0, 0, 0};
    }

    double nums[4] = {0};
    int i = 0;

    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        /*int key_index = -2;*/
        const int val_index = -1;
        if (lua_isnumber(lua, val_index)) {
            nums[i++] = lua_tonumber(lua, val_index);
        } else {
            lua_pop(lua, 2);
            if (err) *err = true;
            return (Rectangle){ 0, 0, 0, 0};
        }
        lua_pop(lua, 1);
        if (i >= 4)
            break;
    }

    return (Rectangle) { 
        .x = nums[0], .y = nums[1],
        .width = nums[2], .height = nums[3],
    };
}

double *read_number_table(lua_State *lua, int index, int *len) {
    assert(len);

    if (!lua_istable(lua, index)) return NULL;

    lua_pushnil(lua);
    int cap = 100;
    *len = 0;
    double *arr = malloc(sizeof(double) * cap);
    while (lua_next(lua, index)) {
        /*int key_index = -2;*/
        int val_index = -1;
        if (lua_isnumber(lua, val_index)) {
            printf("len %d\n", *len);
            arr[(*len)++] = lua_tonumber(lua, val_index);

            if (*len == cap) {
                cap *= 1.5;
                void *new_arr = realloc(arr, sizeof(arr[0]) * cap);
                assert(new_arr);
                arr = new_arr;
            }
        }
        lua_pop(lua, 1);
    }
    if (*len == 0) {
        free(arr);
        arr = NULL;
    }
    return arr;
}


Vector2 read_pos(lua_State *lua, bool *notfound) {
    Vector2 pos = {0};
    lua_pushstring(lua, "pos");
    lua_gettable(lua, -2);

    if (lua_istable(lua, -1)) {
        lua_pushnumber(lua, 1);
        lua_gettable(lua, -2);
        pos.x = lua_tonumber(lua, -1);
        lua_remove(lua, -1);

        lua_pushnumber(lua, 2);
        lua_gettable(lua, -2);
        pos.y = lua_tonumber(lua, -1);
        lua_remove(lua, -1);

        lua_remove(lua, -1);
        if (notfound)
            *notfound = false;
    } else if (notfound) 
        *notfound = true;

    return pos;
}

/*
int new_fullud_ref(Stage *st, Object *obj, const char *tname) {
    assert(obj);
    assert(tname);

    //printf("new_fullud_ref: [%s]\n", stack_dump(cmn.lua));

    if (luaL_getmetatable(lua, tname) != LUA_TTABLE) {
        printf("new_fullud_ref: there is no such metatable '%s'\n", tname);
        abort();
        return -1;
    }
    lua_pop(lua, 1);

    lua_settop(lua, 0);
    Object_ud *oud = lua_newuserdata(lua, sizeof(Object_ud));
    if (!oud) {
        printf("lua_newuserdata() returned NULL in new_fullud_ref()\n");
        exit(EXIT_FAILURE);
    }
    oud->obj = obj;
    oud->st = st;
    obj->ud = oud;
    lua_pushvalue(lua, -1);
    int ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    luaL_getmetatable(lua, tname);
    lua_setmetatable(lua, -2);
    lua_settop(lua, 0);
    if (verbose)
        trace("new_fullud_ref: [%s]\n", L_stack_dump(lua));

    return ref;
}
*/

/*
int new_fullud_get_ref(Stage *st, Object *obj, const char *tname) {
    assert(obj);
    assert(tname);

    trace("new_fullud_get_ref: [%s]\n", L_stack_dump(lua));

    lua_settop(lua, 0);
    Object_ud *oud = lua_newuserdata(lua, sizeof(Object_ud));
    if (!oud) {
        trace("lua_newuserdata() returned NULL in new_fullud_get_ref()\n");
        exit(EXIT_FAILURE);
    }
    oud->obj = obj;
    oud->st = st;
    lua_pushvalue(lua, -1);
    int ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    if (luaL_getmetatable(lua, tname) == LUA_TTABLE) {
        lua_setmetatable(lua, -2);
    } else {
        trace("new_fullud_get_ref: there is no such metatable '%s'\n", tname);
    }

    trace("new_fullud_get_ref: [%s]\n", L_stack_dump(lua));

    return ref;
}
*/

/*
bool object_return_ref_script(Object *obj, int offset) {
    assert(obj);
    char *tmp = (char*)obj + offset;
    int *ref_script = (int*)tmp;
    if (*ref_script) {
        lua_settop(lua, 0);
        lua_rawgeti(lua, LUA_REGISTRYINDEX, *ref_script);
    }
    return ref_script != 0;
}
*/

bool read_fullud(lua_State *lua) {
    bool fullud = false;
    if (!lua_istable(lua, -1)) 
        return fullud;

    lua_pushstring(lua, "fullud");
    lua_gettable(lua, -2);
    if (lua_isboolean(lua, -1)) {
        fullud = lua_toboolean(lua, -1);
    }

    lua_remove(lua, -1);
    return fullud;
}

float read_angle(lua_State *lua, float def_value) {
    float angle = def_value;
    if (!lua_istable(lua, -1)) 
        return angle;

    lua_pushstring(lua, "angle");
    lua_gettable(lua, -2);
    if (lua_isnumber(lua, -1)) {
        angle = lua_tonumber(lua, -1);
    }
    lua_remove(lua, -1);

    return angle;
}

/*
void sc_dostring(const char *str) {
    lua_State *lua = sc_get_state();

    if (!lua) return;

    printf("dostring: %s\n", str);
    printf("dostring: 1 [%s]\n", L_stack_dump(lua));
    if (luaL_dostring(lua, str) != LUA_OK) {
        //const char *err_msg = lua_tostring(lua, lua_gettop(lua));
        const char *err_msg = lua_tostring(lua, -1);
        printf("dostring: 2 [%s]\n", L_stack_dump(lua));

        const char *last = err_msg + strlen(err_msg);
        char line[MAX_LINE] = {0}, *line_ptr = line;
        for (const char *ptr = err_msg; ptr != last; ptr++) {
            if (*ptr != '\n') {
                if (line_ptr - line < sizeof(line)) {
                    *line_ptr++ = *ptr;
                }
            } else {
                console_buf_write_c(RED, "%s", line);
                memset(line, 0, sizeof(line));
                line_ptr = line;
            }
        }

        //console_buf_write2(RED, "%s",err_msg);
        lua_settop(lua, 0);
    } else {
        // XXX: Зачем здесь con\.input_line?
        console_do_strange();
    }
}
*/

/*
int l_con_print(lua_State *s) {
    int argsnum = lua_gettop(s);
    const Color color = DARKPURPLE;
    //printf("l_con_print [%s]\n", stack_dump(s));
    for(int i = 1; i <= argsnum; ++i) {
        if (lua_isstring(s, i)) {
            const char *str = lua_tostring(s, i);
            console_buf_write_c(color, "%s", str);
        } else if (lua_isnumber(s, i)) {
            double num = lua_tonumber(s, i);
            console_buf_write_c(color, "%f", num);
        } else if (lua_isboolean(s, i)) {
            bool b = lua_toboolean(s, i);
            console_buf_write_c(color, "%s", b ? "true" : "false");
        }
    }
    //console_buf_write2(color, format, 
    return 0;
}
*/

static void hook(lua_State *lua, lua_Debug *ar) {
    assert(lua);
    assert(ar);
    printf("currentline %d\n", ar->currentline);

    /*
    if (ar->source) {
        char buf[512] = {0};
        int maxlen = ar->srclen;
        if (ar->srclen >= sizeof(buf))
            maxlen = sizeof(buf);
        strncpy(buf, ar->source, maxlen);
        printf("source %s\n", buf);
    }
    // */
    //if (ar->name)
        //printf("name %s\n", ar->name);
    printf("short_src %s\n", ar->short_src);
}

const char *sc_stack_dump() {
    return L_stack_dump(lua);
}

int make_ret_table(int num, void**arr, size_t offset) {
    lua_State *lua = sc_get_state();
    if (!arr) 
        return 0;

    lua_createtable(lua, num, 0);
    for (int i = 0; i < num; i++) {
        lua_pushnumber(lua, i + 1);
        int ref = *(int*)((char*)arr[i] + offset);
        lua_rawgeti(lua, LUA_REGISTRYINDEX, ref);
        lua_settable(lua, -3);
    }
    return 1;
}

/*
void koh_sc_from_args(int argc, char **argv) {
    char joined_args[512] = {0};
    for(int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "-e")) {
            for (int rest = i + 1; rest < argc; rest++) {
                int len = strlen(joined_args) + strlen(argv[rest]); 
                if (len < sizeof(joined_args)) {
                    strcat(joined_args, argv[rest]);
                }
            }
        }
    }
    if (strlen(joined_args) > 0) {
        sc_dostring(joined_args);
    }
}
*/

lua_State *sc_state_new(bool openlibs) {
    lua = luaL_newstate();
    assert(lua);
    trace("sc_init: lua version %f\n", lua_version(lua));
    /*
    // Тут будет загрузка из файла?
    if (luaL_dostring(lua, "package.path = package.path .. ''") != LUA_OK) {
        trace("sc_state_new: could not changed package.path\n");
        abort();
    }
    */
    if (openlibs)
        luaL_openlibs(lua);
    return lua;
}

void *koh_lua_default_alloc(
    void *ud, void *ptr, size_t osize, size_t nsize
) {
    (void)ud; (void)osize; // не используем в этом примере
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else {
        return realloc(ptr, nsize);
    }
}

void L_inspect(lua_State *l, int idx) {
    assert(l);
    // копирую значение
    lua_pushvalue(l, idx);
    char buf_uniq_name[128] = {};
    sprintf(buf_uniq_name, "X_%d_%d", rand() % 100, rand() % 100);
    lua_setglobal(l, buf_uniq_name);
    const char *code = 
        "local inspect = require 'inspect'\n"
        "print(inspect(%s))";
    char buf_code[128 * 2] = {};
    sprintf(buf_code, code, buf_uniq_name);
    L_inline(l, buf_code);

    lua_pushnil(l);
    lua_setglobal(l, buf_uniq_name);
}


void L_inline(lua_State *l, const char *code) {
    assert(l);
    int err = luaL_dostring(l, code);
    if (err != LUA_OK) {
        printf("l_inline: error '%s'\n", lua_tostring(l, -1));
        abort();
    } 
}
