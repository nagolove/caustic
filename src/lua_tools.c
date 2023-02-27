// vim: fdm=marker
#include "lua_tools.h"

#include <string.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <stdbool.h>
#include <string.h>

TypeEntry *typelist;

DocArray doc_get(lua_State *lua, const char *mtname) {
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

    printf("doc_get: [%s]\n", stack_dump(lua));
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

void doc_release(DocArray *docarr) {
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

// XXX: maxrecurse is unused
void print_table(lua_State *lua, int idx, int maxrecurse) {
    /*printf("print_table: [%s]\n", stack_dump(lua));*/

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

void type_alloc(const char *mtname) {
    TypeEntry *new = calloc(1, sizeof(*new));
    new->next = typelist;
    strncpy(new->mtname, mtname, MAX_MTNAME);
    typelist = new;
}

void register_methods_and_doc(
    lua_State *lua,
    const char *mtname,
    const Reg_ext *methods
) {
    printf("register_methods_and_doc: [%s]\n", stack_dump(lua));

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

    type_alloc(mtname);

    // табличка со справкой
    lua_createtable(lua, 0, 0);
    Reg_ext *cur = (Reg_ext*)methods - 1;

    while (true) {
        cur++;
        if (!cur->name) {
            break;
        }
        printf("[%s]\n", stack_dump(lua));

        lua_createtable(lua, 0, 0);
        lua_pushstring(lua, cur->desc);
        lua_seti(lua, -2, 1);
        lua_pushstring(lua, cur->desc_detailed);
        lua_seti(lua, -2, 2);

        //printf("[%s]\n", stack_dump(lua));
        lua_setfield(lua, -2, cur->name);
        /*printf("[%s]\n", stack_dump(lua));*/

        /*printf("cur->func, cur->name %p, %s\n", cur->func, cur->name);*/
        //printf("-------------------\n");
    }

    luaL_newmetatable(lua, mtname);
    cur = (Reg_ext*)methods - 1;

    lua_pushvalue(lua, -1);
    lua_setfield(lua, -2, "__index");

    lua_pushvalue(lua, -2);
    lua_setfield(lua, -2, "__doc");

    /*lua_remove(lua, -2);*/

    while (true) {
        cur++;
        if (!cur->name) {
            break;
        }
        lua_pushcclosure(lua, cur->func, 0);
        lua_setfield(lua, -2, cur->name);
        //printf("cur->func, cur->name %p, %s\n", cur->func, cur->name);
    }

    lua_settop(lua, 0);
    //printf("register_methods_and_doc: [%s]\n", stack_dump(lua));
}

const char *stack_dump(lua_State *lua) {
    if (!lua) 
        return NULL;

    static char ret[1024] = {0, };
    char *ptr = ret;
    int top = lua_gettop(lua);
    for (int i = 1; i <= top; i++) {
        int t = lua_type(lua, i);
        switch (t) {
            case LUA_TUSERDATA: 
                ptr += snprintf(
                    ptr, sizeof(ret), "userdata %p", lua_topointer(lua, i)
                );
                break;
            case LUA_TSTRING: 
                ptr += snprintf(ptr, sizeof(ret), "’%s’", lua_tostring(lua, i));
                break;
            case LUA_TBOOLEAN: 
                ptr += snprintf(ptr, sizeof(ret), lua_toboolean(lua, i) ? "true" : "false");
                break;
            case LUA_TNUMBER: 
                ptr += snprintf(ptr, sizeof(ret), "%g", lua_tonumber(lua, i));
                break;
            default: 
                ptr += snprintf(ptr, sizeof(ret), "%s", lua_typename(lua, t));
                break;
        }
        ptr += sprintf(ptr, " "); 
    }
    return ret;
}

void types_init() {
    typelist = NULL;
}

void types_shutdown() {
    TypeEntry *curr = typelist;
    while (curr) {
        TypeEntry *next = curr->next;
        free(curr);
        curr = next;
    }
    typelist = NULL;
}

TypeEntry *types_getlist() {
    return typelist;
}

