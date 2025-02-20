#include "koh_script.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lauxlib.h"
#include "lua.h"
#include "koh_console.h"
#include "koh_inotifier.h"
#include "koh_logger.h"
#include "koh_lua_tools.h"
#include "raylib.h"

#ifndef KOH_DEP_NO_RLWR
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
    return strcmp(((struct Pair*)a)->name, ((struct Pair*)b)->name);
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

    console_buf_write_c(BLUE, "Следущие функции доступны из консоли:\n");

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
        console_buf_write_c(BLACK, "%s = %s", p->name, p->desc);
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

int new_fullud_ref(Stage *st, Object *obj, const char *tname) {
    assert(obj);
    assert(tname);

    /*printf("new_fullud_ref: [%s]\n", stack_dump(cmn.lua));*/

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

int l_con_print(lua_State *s) {
    int argsnum = lua_gettop(s);
    const Color color = DARKPURPLE;
    /*printf("l_con_print [%s]\n", stack_dump(s));*/
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
