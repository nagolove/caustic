#pragma once

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define MAX_MTNAME  64

typedef struct Reg_ext {
    //const char *name;
    char *name;
    lua_CFunction func;
    //const char *desc, *desc_detailed;
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

const char *stack_dump(lua_State *lua);
void print_table(lua_State *lua, int idx, int maxrecurse);
