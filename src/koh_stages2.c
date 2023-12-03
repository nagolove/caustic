// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stages2.h"
#include "lualib.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
//#include "koh.h"
#include "lauxlib.h"
#include "lua.h"
#include "raylib.h"
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"

#define MAX_STAGES_NUM  32

struct StagesStore2 {
    Stage2   *stages[MAX_STAGES_NUM];
    bool     selected[MAX_STAGES_NUM];
    Stage2   *cur;
    uint32_t num;
};

// Убрать глобальную переменную, использовать переносимый контекст сцен.
//static struct StagesStore2 stages = {0, };

int l_stages_get(lua_State *lua);
int l_stage2_restart(lua_State *lua);
int l_stage2_set_active(lua_State *lua);
int l_stage2_get_active(lua_State *lua);

/*
Как провести попытку рефакторинга минимально разрушая текущую конфигурацию?
Скопировать файлы, переименовать подсистему, добавить суффикс7
 */
Stage2 *stage2_new(struct StageStoreSetup2 *setup) {
    Stage2 *ss = calloc(1, sizeof(*ss));
    assert(ss);
    ss->store = calloc(1, sizeof(struct StagesStore2));
    if (setup && setup->l && setup->stage_store_name) {
        // FIXME: Починить условие
        /*
        if (lua_getglobal(setup->l, setup->stage_store_name) != LUA_TNONE) {
            trace(
                "stage_new: there is a '%s' value in global space\n",
                setup->stage_store_name
            );
        } else 
            */
        {
            trace(
                "stage_new: setup global light user data '%s'\n",
                setup->stage_store_name
            );
            lua_pushlightuserdata(setup->l, ss);
            lua_setglobal(setup->l, setup->stage_store_name);
        }
    }
    return ss;
}

void stage2_init(Stage2 *ss) {
    //memset(&stages, 0, sizeof(stages));
    assert(ss);

    if (ss->store) {
        for(int i = 0; i < ss->store->num; i++) {
            Stage2 *st = ss->store->stages[i];
            if (st->init) {
                st->init(st, NULL);
            }
        }
    }

    // TODO: Переделать скриптование с учетом разных луа-машин в системе
    /*
    if (!sc_get_state()) return;

    sc_register_function(
            l_stage_restart,
            "stage_restart",
            "Удалить и снова создать объект сцены"
    );
    sc_register_function(
            l_stage_get_active,
            "stage_get_active",
            "Возвращает имя активной сцены"
    );
    sc_register_function(
            l_stage_set_active, 
            "stage_set_active", 
            "Установить сцену по имени [\"\"]"
    );
    sc_register_function(
            l_stages_get, 
            "stages_get", 
            "Получить список зарегистрированных сцен"
    );
    */
}

Stage2 *stage2_add(Stage2 *ss, Stage2 *st, const char *name) {
    assert(ss);
    assert(st);
    assert(name);
    assert(strlen(name) < MAX_STAGE_NAME);
    // Не может быть NULL, но может число элементов может == 0
    assert(ss->store);  
    assert(ss->store->num < MAX_STAGES_NUM);

    ss->store->stages[ss->store->num++] = st;
    strncpy(st->name, name, MAX_STAGE_NAME - 1);
    trace(
        "stage_add: name '%s', num %d\n",
        ss->store->stages[ss->store->num - 1]->name,
        ss->store->num
    );
    return st;
}

/*
void stage2_shutdown(Stage2 *ss) {
    assert(ss);
    for(int i = 0; i < ss->store->num; i++) {
        Stage2 *st = ss->store->stages[i];
        if (st) {
            if (st->shutdown) 
                st->shutdown(st);
            free(st);
            ss->store->stages[i] = NULL;
        }
    }
}
*/

void stage2_active_update(Stage2 *ss) {
    assert(ss);
    if (!ss->store->cur)
        return;
    Stage2 *st = ss->store->cur;
    // TODO: Разделить обновление и рисовку?
    if (st->update) st->update(st);
    if (st->draw) st->draw(st);
}

Stage2 *stage2_find(Stage2 *ss, const char *name) {
    if (ss->store) {
        for(int i = 0; i < ss->store->num; i++) {
            Stage2 *st = ss->store->stages[i];
            if (!strcmp(name, st->name)) {
                return st;
            }
        }
    }
    return NULL;
}

void stage2_active_set(Stage2 *ss, const char *name, void *data) {
    Stage2 *st = stage2_find(ss, name);
    //assert(st);

    if (!st) {
        trace("stage_set_active: '%s' not found\n", name);
    }

    // TODO: Добавить проверку на ss->store

    if (ss->store->cur && ss->store->cur->leave) {
        trace(
            "stage_set_active: leave from '%s'\n",
            ss->store->cur->name
        );
        ss->store->cur->leave(ss->store->cur, data);
    }

    ss->store->cur = st;

    if (st && st->enter) {
        trace("stage_set_active: enter to '%s'\n", st->name);
        st->enter(st, data);
    }
}

/*
int l_stages_get(lua_State *lua) {
    for(int i = 0; i < stages.num; i++) {
        console_buf_write_c(DARKGREEN, "\"%s\"", stages.stages[i]->name);
    }
    return 0;
}

int l_stage2_restart(lua_State *lua) {
    luaL_checktype(lua, 1, LUA_TSTRING);
    const char *stage2_name = lua_tostring(lua, 1);
    Stage *st = stage2_find(stage_name);
    if (st) {
        if (st->shutdown)
            st->shutdown(st);
        if (st->init)
            st->init(st, NULL);
    }
    return 0;
}

int l_stage2_set_active(lua_State *lua) {
    bool arg1 = lua_isstring(lua, 1);
    bool arg2 = lua_isstring(lua, 2);
    if (arg1 && !arg2) {
        const char *stage2_name = lua_tostring(lua, 1);
        trace("l_stage_set_active: \"%s\"\n", stage_name);
        stage2_set_active(stage_name, NULL);
    } else if (arg1 && arg2) {
        const char *name = lua_tostring(lua, 1);
        const char *arg = lua_tostring(lua, 2);
        trace("l_stage_set_active: \"%s\" %s\n", name, arg);
        // XXX: Может быть ошиюка при передачи arg из-за освобождения 
        // памяти строки
        stage2_set_active(name, (void*)arg);
    } else 
        trace("l_stage_set_active: bad arguments\n");
    return 0;
}

int l_stage2_get_active(lua_State *lua) {
    if (stages.cur) {
        lua_pushstring(lua, stages.cur->name);
        return 1;
    } else
        return 0;
}
*/

// TODO: Добавить получение табличного Lua представления для всех всложенных 
// сцен. Такая функция должна возвращать char**, построчно
void stage2_print(Stage2 *ss) {
    assert(ss);
    trace("stage_print:\n");
    if (!ss->store)
        return;
    if (!ss->store->num) 
        return;

    lua_State *l = luaL_newstate();
    luaL_openlibs(l);
    const size_t len = 1024;
    char *table_str = malloc(len * ss->store->num);
    char *p = table_str;

    for (int i = 0; i < ss->store->num; i++) {
        const Stage2 *st = ss->store->stages[i];
        p += sprintf(p, "{ ");
        p += sprintf(p, "init = %p", st->init);
        p += sprintf(p, "enter = %p", st->enter);
        p += sprintf(p, "leave = %p", st->leave);
        p += sprintf(p, "shutdown = %p", st->shutdown);
        p += sprintf(p, "draw = %p", st->draw);
        p += sprintf(p, "update = %p", st->update);
        p += sprintf(p, "gui = %p", st->gui);
        p += sprintf(p, "data = %p", st->data);
        p += sprintf(p, "name = '%s'", st->name);
        p += sprintf(p, "}");
    }

    if (!luaL_dostring(l, table_str)) {
        trace(
            "stage2_print: luaL_dostring(l, table_str) failed with '%s'\n",
            lua_tostring(l, -1)
        );
        goto _exit;
    }

    lua_setglobal(l, "ss");
    if (luaL_dostring(
        l, 
        "package.path = package.path .. ';assets/?.lua'\n"
        "local tabular = requre 'tabular'\n"
        "if ss then\n"
        "   print(tabular(ss))\n"
        "end\n"
    ) != LUA_OK) {
        trace(
            "stage_print: tabular code failed with '%s'",
            lua_tostring(l, -1)
        );
        goto _exit;
    }

_exit:
    lua_close(l);
    free(table_str);

    /*
    for (int i = 0; i < ss->num; i++) {
        trace("stage_print: '%s'\n", ss->stages[i]->name);
    }
    */
}

const char *stage2_active_name_get(Stage2 *ss) {
    assert(ss);
    if (!ss->store)
        return NULL;
    if (ss->store->cur) {
        static char ret_buf[MAX_STAGE_NAME] = {0};
        strcpy(ret_buf, ss->store->cur->name);
        return ret_buf;
    }
    return NULL;
}

/*
void *stage2_assert(Stage *st, const char *name) {
    assert(st);
    Stage *res = stage2_find(st->name);
    assert(res);
    return res;
}
*/

static Stage2 *get_selected_stage(Stage2 *ss) {
    assert(ss);
    // FIXME: Можно поставить утверждение
    if (!ss->store)
        return NULL;
    for (int i = 0; i < ss->store->num; i++) {
        if (ss->store->selected[i])
            return ss->store->stages[i];
    }
    return NULL;
}

void stage2_gui_window(Stage2 *ss) {
    assert(ss);
    bool opened = true;
    ImGuiWindowFlags flags = 0;
    igBegin("stages window", &opened, flags);

    igText("active stage: %s", ss->store->cur ? ss->store->cur->name : NULL);

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody;

    ImGuiInputTextFlags input_flags = 0;

    ImVec2 outer_size = {0., 0.};
    const int columns_num = 8;
    if (igBeginTable("stage", columns_num, table_flags, outer_size, 0.)) {

        igTableSetupColumn("name", 0, 0, 0);
        igTableSetupColumn("init", 0, 0, 1);
        igTableSetupColumn("shutdown", 0, 0, 2);
        igTableSetupColumn("draw", 0, 0, 3);
        igTableSetupColumn("update", 0, 0, 4);
        igTableSetupColumn("enter", 0, 0, 5);
        igTableSetupColumn("leave", 0, 0, 6);
        igTableSetupColumn("data", 0, 0, 7);
        igTableHeadersRow();

        for (int i = 0; i < ss->store->num; ++i) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            char name_label[64] = {};
            sprintf(name_label, "%s", ss->store->stages[i]->name);
            igTableSetColumnIndex(0);
            if (igSelectable_BoolPtr(
                name_label, &ss->store->selected[i],
                ImGuiSelectableFlags_SpanAllColumns, (ImVec2){0, 0}
            )) {
                for (int j = 0; j < ss->store->num; ++j) {
                    if (j != i)
                        ss->store->selected[j] = false;
                }
            }

            igTableSetColumnIndex(1);
            igText("%p", ss->store->stages[i]->init);

            igTableSetColumnIndex(2);
            igText("%p", ss->store->stages[i]->shutdown);

            igTableSetColumnIndex(3);
            igText("%p", ss->store->stages[i]->draw);

            igTableSetColumnIndex(4);
            igText("%p", ss->store->stages[i]->update);

            igTableSetColumnIndex(5);
            igText("%p", ss->store->stages[i]->enter);

            igTableSetColumnIndex(6);
            igText("%p", ss->store->stages[i]->leave);

            igTableSetColumnIndex(7);
            igText("%p", ss->store->stages[i]->data);

        }
        igEndTable();
    }

    static char stage_str_argument[64] = {};
    igInputText(
        "stage string argument",
        stage_str_argument, sizeof(stage_str_argument), input_flags, 0, NULL
    );

    if (igButton("switch to selected stage", (ImVec2) {0, 0})) {
        const Stage2 *selected = get_selected_stage(ss);
        if (selected)
            stage2_active_set(ss, selected->name, NULL);
    }

    koh_window_post();
    igEnd();
}

void stage2_active_gui_render(Stage2 *ss) {
    assert(ss);
    if (!ss->store)
        return;
    Stage2 *st = ss->store->cur;
    if (st && st->gui) st->gui(st);
}

void stage2_free(Stage2 *ss) {
    if (ss) {
        // Пройти по всем дочерним сценам
        if (ss->store) {
            for (int i = 0; i < ss->store->num; i++) {
                stage2_free(ss->store->stages[i]);
                ss->store->stages[i] = NULL;
            }
            // Освободить память хранилища
            free(ss->store);
            ss->store = NULL;
        }
        // Освободить память сцены
        free(ss);
    }
}
