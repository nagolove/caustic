#pragma once

#include "raylib.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>

#define MAX_MTNAME  64

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

/*
Создать метатаблицу, связать ее поле __index с ней, записать методы из массива
methods. Массив должен заканчиваться парой значений NULL в структурах.
*/
void sc_register_methods_and_doc(
    lua_State *lua,
    const char *mtname,
    const Reg_ext *methods
);

void types_init();
void types_shutdown();
TypeEntry *types_getlist();

DocArray doc_init(lua_State *lua, const char *mtname);
void doc_shutdown(DocArray *docarr);

// Возвращает указатель на статический буфер.
const char *stack_dump(lua_State *lua);
void table_print(lua_State *lua, int idx);

struct TablePrintOpts {
    bool tabulate;
};

char *table_get_print(
    lua_State *lua, int idx, const struct TablePrintOpts *opts
);
// На вершине стека должна лежать таблица которая будет записана модулем
// serpent в строку. Возвращаемая память выделяется через alloc()
// Стандартные библиотеки Луа принудительно загружаются.
// Луа стек не изменяется.
char *table_dump2allocated_str(lua_State *l);
void table_push_rect_as_arr(lua_State *l, Rectangle rect);
