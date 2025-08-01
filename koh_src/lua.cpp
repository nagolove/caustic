// vim: fdm=marker 

/*
 Сперва сделать выполнение последовательным.
 Переделать так - сперва формируется массив с задачами
 Потом запускаются первые N задач.
 В задачах не будет цикла ожидания.
 */


#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    lua_State *l = luaL_newstate();
    luaL_openlibs(l); 

    lua_close(l);
    return EXIT_SUCCESS;
}

