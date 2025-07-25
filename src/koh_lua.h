// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "raylib.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include "lua.h"
#include "raylib.h"
#include "koh_common.h"

#define MAX_MTNAME  64

// {{{ struct
typedef struct Reg_ext {
    char *name;
    lua_CFunction func;
    char *desc, *desc_detailed;
} Reg_ext;

typedef struct DocField {
    char *name, *desc, *desc_detailed;
} DocField;

typedef struct DocArray {
    int num;
    DocField *arr;
} DocArray;

typedef struct TypeEntry {
    struct TypeEntry *next;
    char *mtname;
    Reg_ext reg;
} TypeEntry;
// }}}

/*
Создать метатаблицу, связать ее поле __index с ней, записать методы из массива
methods. Массив должен заканчиваться парой значений NULL в структурах.
*/
void sc_register_methods_and_doc(
    lua_State *lua,
    const char *mtname,
    const Reg_ext *methods
);

/*
// TODO: Убрать глобальный контекс
void types_init();
void types_shutdown();
TypeEntry *types_getlist();
*/

/*
// XXX: Зачем эти функции?
DocArray doc_init(lua_State *lua, const char *mtname);
void doc_shutdown(DocArray *docarr);
*/

__attribute_deprecated__
// Возвращает указатель на статический буфер.
const char *stack_dump(lua_State *lua);
// Возвращает указатель на статический буфер.
const char *L_stack_dump(lua_State *lua);

// Печатать таблицу из состояния lua по индексу стека idx
__attribute_deprecated__
void table_print(lua_State *lua, int idx);

// Печатать таблицу из состояния lua по индексу стека idx
void L_table_print(lua_State *lua, int idx);

struct TablePrintOpts {
    bool tabulate;
};

// XXX: Возвращается статическая или выделенная память?
__attribute_deprecated__
char *table_get_print(
    lua_State *lua, int idx, const struct TablePrintOpts *opts
);

// Распечатывает значение со стека по указанному индексу в буфер. Возвращает
// указатель на этот статический буфер.
// TODO: Обработка переполнения буфера.
char *L_table_get_print(
    lua_State *lua, int idx, const struct TablePrintOpts *opts
);

// На вершине стека должна лежать таблица которая будет записана модулем
// serpent в строку. Возвращаемая память выделяется через alloc()
// Стандартные библиотеки Луа принудительно загружаются.
// Луа стек не изменяется.
// TODO: Переиновать в table_dump2str_alloc()
__attribute_deprecated__
char *table_dump2allocated_str(lua_State *l);

__attribute_deprecated__
char *L_table_dump2allocated_str(lua_State *l);

enum L_DumpError {
    L_DE_BADTYPE = 0,   // если на вершине стека лежит не таблица
    L_DE_SERPENT = 1,   // ошибка загрузки serpent модуля
};

char *L_table_serpent_alloc(lua_State *l, enum L_DumpError *err);

void table_push_rect_as_arr(lua_State *l, Rectangle rect);
// { x0, y0, x1, y1, x2, y2, ... }
void table_push_points_as_arr(lua_State *l, Vector2 *points, int points_num);

void L_table_push_rect_as_arr(lua_State *l, Rectangle rect);
// { x0, y0, x1, y1, x2, y2, ... }
void L_table_push_points_as_arr(lua_State *l, Vector2 *points, int points_num);

void L_tabular_print(lua_State *l, const char *global_name);
char *L_tabular_alloc(lua_State *l, const char *global_name);
// Вернуть строку с распечатанной Луа структорой в строке как форматированной
// таблицей. Память выделяется на куче, необходим вызов free()
char *L_tabular_alloc_s(lua_State *l, const char *lua_str);

// Вызов глобальной функции скрипта. Возвращает указатель на статический
// буфер с описанием ошибки.
const char *L_pcall(lua_State *l, const char *func_name, bool *is_ok);
void L_call(lua_State *l, const char *func_name, bool *is_ok);

extern bool L_verbose;


typedef struct ScriptFunc {
    lua_CFunction func;
         // имя функции
    char fname[64], 
         // описание, краткая документация
         desc[256];
    struct ScriptFunc *next;
} ScriptFunc;

/*
TODO: Убрать глобальное состояние в переменную контекста

typedef struct ScriptState {
    lua_State  *l;
    ScriptFunc *funcs;
};

 */

void sc_init();
void sc_shutdown();

// TODO: Объявить устаревшим и удалить.
/*__attribute_deprecated__*/
lua_State *sc_get_state();

void sc_init_script();

ScriptFunc *sc_get_head();
void sc_register_all_functions(void);
void sc_register_function(lua_CFunction f, const char *fname, const char *desc);
void sc_register_func_desc(const char *funcname, const char *description);
const char * sc_stack_dump();

uint32_t read_id(lua_State *lua);
Vector2 read_pos(lua_State *lua, bool *notfound);
bool read_fullud(lua_State *lua);
float read_angle(lua_State *lua, float def_value);
double *read_number_table(lua_State *lua, int index, int *len);
//void sc_dostring(const char *str);
double *read_number_table(lua_State *lua, int index, int *len);
Rectangle read_rect(lua_State *lua, int index, bool *err);
int make_ret_table(int num, void**arr, size_t offset);
//bool object_return_ref_script(Object *obj, int offset);
void koh_sc_from_args(int argc, char **argv);

lua_State *sc_state_new(bool openlibs);

// Выделитель памяти для Луа по умолчанию.
void *koh_lua_default_alloc(void *ud, void *ptr, size_t osize, size_t nsize);

void L_inspect(lua_State *l, int idx);
void L_inline(lua_State *l, const char *code);

