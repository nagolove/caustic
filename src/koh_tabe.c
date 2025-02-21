#include "koh_tabe.h"

#include "koh_console.h"
#include "koh_logger.h"
#include "koh_lua.h"
#include "lua.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void tabe_init(TabEngine *te, lua_State *lua) {
    assert(te);
    te->lua = lua;
    te->variantsnum = 0;
}

void tabe_shutdown(TabEngine *te) {
    assert(te);
    if (te->variants) {
        for(int i = 0; i < te->variantsnum; ++i) {
            free(te->variants[i]);
        }
        free(te->variants);
        te->variants = NULL;
    }
}

const char* tabe_tab(TabEngine *te, const char *line) {
    assert(te);
    static char ret_line[MAX_LINE] = {0};
    memset(ret_line, 0, sizeof(char) * MAX_LINE);

    if (!te->lua)
        return ret_line;

    switch(te->state) {
        case TAB_STATE_NONE:
            te->state = TAB_STATE_NEXT;

            lua_State *lua = te->lua;
            trace("tabe_tab: [%s]\n", L_stack_dump(lua));

            lua_getglobal(lua, "_G");
            int table_index = lua_gettop(lua);
            lua_pushnil(te->lua);

            //int tab_counter = 0;
            while (lua_next(lua, table_index)) {
                const char *key = lua_tostring(lua, -2);
                //printf("key %s\n", key);
                if (strstr(key, line) == key) {
                    trace("-> %s\n", key);

                    char *src = (char*)&ret_line;
                    int input_len = strlen(line);
                    for(int i = input_len; i < strlen(key); i++) {
                        *src++ = key[i];
                    }

                    te->state = TAB_STATE_NEXT;
                    break;
                }
                lua_pop(lua, 1);
                //tab_counter++;
            }

            //te->tab_counter = tab_counter;
            lua_settop(lua, 0);
            break;
        case TAB_STATE_NEXT:
            break;
    }

    return (char*)&ret_line;
}

void tabe_break(TabEngine *te) {
    assert(te);
    te->state = TAB_STATE_NONE;
}
