#pragma once

// TODO: Совместить в koh_lua_tools.h

#include "koh_object.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "raylib.h"

typedef struct ScriptFunc {
    lua_CFunction func;
    char fname[64], desc[256];
    struct ScriptFunc *next;
} ScriptFunc;

void sc_init();
void sc_shutdown();
lua_State *sc_get_state();
void sc_init_script();

ScriptFunc *sc_get_head();
void sc_register_all_functions(void);
void sc_register_function(lua_CFunction f, const char *fname, const char *desc);
void sc_register_func_desc(const char *funcname, const char *description);
const char * sc_stack_dump();
int new_fullud_ref(Stage *st, Object *obj, const char *tname);
int new_fullud_get_ref(Stage *st, Object *obj, const char *tname);

//const char *skip_lead_zeros(const char *s);
uint32_t read_id(lua_State *lua);
//Vector2 read_pos(lua_State *lua);
Vector2 read_pos(lua_State *lua, bool *notfound);
bool read_fullud(lua_State *lua);
float read_angle(lua_State *lua, float def_value);
double *read_number_table(lua_State *lua, int index, int *len);
void sc_dostring(const char *str);
double *read_number_table(lua_State *lua, int index, int *len);
Rectangle read_rect(lua_State *lua, int index, bool *err);
int make_ret_table(int num, void**arr, size_t offset);
bool object_return_ref_script(Object *obj, int offset);
void koh_sc_from_args(int argc, char **argv);

static inline Object_ud checkudata(lua_State *l, int ud, const char *tname) {
    Object_ud r = {0};
    Object_ud *ref = (Object_ud*)luaL_checkudata(l, ud, tname);
    if (ref) {
        r.obj = ref->obj;
        r.st = ref->st;
    }
    return r;
}

lua_State *sc_state_new(bool openlibs);
