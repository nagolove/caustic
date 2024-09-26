// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "raylib.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>

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

// XXX: Возвращается статическая или выделенная память?
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

char *L_table_dump2allocated_str(lua_State *l);

void table_push_rect_as_arr(lua_State *l, Rectangle rect);
// { x0, y0, x1, y1, x2, y2, ... }
void table_push_points_as_arr(lua_State *l, Vector2 *points, int points_num);

void L_table_push_rect_as_arr(lua_State *l, Rectangle rect);
// { x0, y0, x1, y1, x2, y2, ... }
void L_table_push_points_as_arr(lua_State *l, Vector2 *points, int points_num);

extern bool L_verbose;
