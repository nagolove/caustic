// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stages.h"
#include "lualib.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh.h"
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

struct StagesStore {
    Stage    *stages[MAX_STAGES_NUM];
    bool     selected[MAX_STAGES_NUM];
    Stage    *cur;
    uint32_t num;
    ArgV     argv;
};

// Убрать глобальную переменную, использовать переносимый контекст сцен.
//static struct StagesStore stages = {0, };

int l_stages_get(lua_State *lua);
int l_stage_restart(lua_State *lua);
int l_stage_set_active(lua_State *lua);
int l_stage_get_active(lua_State *lua);

/*
Как провести попытку рефакторинга минимально разрушая текущую конфигурацию?
Скопировать файлы, переименовать подсистему, добавить суффикс7
 */
StagesStore *stage_new(struct StageStoreSetup *setup, int argc, char **argv) {
    StagesStore *ss = calloc(1, sizeof(*ss));
    assert(ss);
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

    ss->argv.argv = argv;
    ss->argv.argc = argc;

    return ss;
}

void stage_init(StagesStore *ss) {
    //memset(&stages, 0, sizeof(stages));
    assert(ss);

    for(int i = 0; i < ss->num; i++) {
        Stage *st = ss->stages[i];
        if (st->init) {
            st->init(st);
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

Stage *stage_add(StagesStore *ss, Stage *st, const char *name) {
    if (!st) {
        trace("stage_add: NULL stage, ignoring\n");
        return NULL;
    }

    assert(st);
    assert(name);
    assert(strlen(name) < MAX_STAGE_NAME);
    assert(ss->num < MAX_STAGES_NUM);

    st->store = ss;
    ss->stages[ss->num++] = st;
    strncpy(st->name, name, MAX_STAGE_NAME - 1);
    trace(
        "stage_add: name '%s', num %d\n",
        ss->stages[ss->num - 1]->name,
        ss->num
    );
    return st;
}

void stage_shutdown(StagesStore *ss) {
    assert(ss);
    trace("stage_shutdown:\n");
    for(int i = 0; i < ss->num; i++) {
        Stage *st = ss->stages[i];

        assert(st);
        //if (st) {
        {
            if (st->shutdown) 
                st->shutdown(st);
            free(st);
            ss->stages[i] = NULL;
        }
    }
}

void stage_active_update(StagesStore *ss) {
    assert(ss);
    Stage *st = ss->cur;

    // XXX: Какое поведение выбрать - молчать если нет активной сцены или
    // падать?

    if (!st) {
        trace("stage_active_update: active stage == NULL\n");
        koh_trap();
    } else {
        if (st->update) st->update(st);
        if (st->draw) st->draw(st);
    }
}

Stage *stage_find(StagesStore *ss, const char *name) {
    for(int i = 0; i < ss->num; i++) {
        Stage *st = ss->stages[i];
        assert(st);
        if (!strcmp(name, st->name)) {
            return st;
        }
    }
    return NULL;
}

void stage_active_set(StagesStore *ss, const char *name) {
    Stage *st = stage_find(ss, name);

    if (!st) {
        trace("stage_set_active: '%s' not found\n", name);
    }

    if (ss->cur && ss->cur->leave) {
        trace(
            "stage_set_active: leave from '%s'\n",
            ss->cur->name
        );
        ss->cur->leave(ss->cur);
    }

    ss->cur = st;

    if (st && st->enter) {
        trace("stage_set_active: enter to '%s'\n", st->name);
        st->enter(st);
    }
}

/*
int l_stages_get(lua_State *lua) {
    for(int i = 0; i < stages.num; i++) {
        console_buf_write_c(DARKGREEN, "\"%s\"", stages.stages[i]->name);
    }
    return 0;
}

int l_stage_restart(lua_State *lua) {
    luaL_checktype(lua, 1, LUA_TSTRING);
    const char *stage_name = lua_tostring(lua, 1);
    Stage *st = stage_find(stage_name);
    if (st) {
        if (st->shutdown)
            st->shutdown(st);
        if (st->init)
            st->init(st, NULL);
    }
    return 0;
}

int l_stage_set_active(lua_State *lua) {
    bool arg1 = lua_isstring(lua, 1);
    bool arg2 = lua_isstring(lua, 2);
    if (arg1 && !arg2) {
        const char *stage_name = lua_tostring(lua, 1);
        trace("l_stage_set_active: \"%s\"\n", stage_name);
        stage_set_active(stage_name, NULL);
    } else if (arg1 && arg2) {
        const char *name = lua_tostring(lua, 1);
        const char *arg = lua_tostring(lua, 2);
        trace("l_stage_set_active: \"%s\" %s\n", name, arg);
        // XXX: Может быть ошиюка при передачи arg из-за освобождения 
        // памяти строки
        stage_set_active(name, (void*)arg);
    } else 
        trace("l_stage_set_active: bad arguments\n");
    return 0;
}

int l_stage_get_active(lua_State *lua) {
    if (stages.cur) {
        lua_pushstring(lua, stages.cur->name);
        return 1;
    } else
        return 0;
}
*/

void stage_print(StagesStore *ss) {
    // {{{
    assert(ss);
    trace("stage_print:\n");
    if (!ss->num) return;

    lua_State *l = luaL_newstate();
    luaL_openlibs(l);
    const size_t len = 1024;
    size_t sz = len * ss->num;
    char *table_str = malloc(sz);
    char *p = table_str; 

    for (int i = 0; i < ss->num; i++) {
        const Stage *st = ss->stages[i];
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
        if (!p)
            break;
        p += sprintf(p, "}");
    }

    if (!luaL_dostring(l, table_str)) {
        trace(
            "stage_print: luaL_dostring(l, table_str) failed with '%s'\n",
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
    // }}}
}

const char *stage_active_name_get(StagesStore *ss) {
    assert(ss);
    if (ss->cur) {
        static char ret_buf[MAX_STAGE_NAME] = {0};
        strncpy(ret_buf, ss->cur->name, sizeof(ret_buf));
        return ret_buf;
    }
    return NULL;
}

/*
void *stage_assert(Stage *st, const char *name) {
    assert(st);
    Stage *res = stage_find(st->name);
    assert(res);
    return res;
}
*/

static Stage *get_selected_stage(StagesStore *ss) {
    assert(ss);
    for (int i = 0; i < ss->num; i++) {
        if (ss->selected[i])
            return ss->stages[i];
    }
    return NULL;
}

void sort_table_specs(ImGuiTableSortSpecs *specs) {
    assert(specs);
    trace("sort_table_specs: SpecsCount %d\n", specs->SpecsCount);
    for (int i = 0; i < specs->SpecsCount; i++) {
        trace(
            "ColumnUserID %u ColumnIndex %d SortOrder %d SortDirection %d\n",
            specs->Specs[i].ColumnUserID,
            specs->Specs[i].ColumnIndex,
            specs->Specs[i].SortOrder,
            specs->Specs[i].SortDirection
        );
    }
}

void stages_gui_window(StagesStore *ss) {
    assert(ss);
    bool opened = true;
    ImGuiWindowFlags flags = //ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize; 
    char buf[128] = {};
    if (ss->cur) {
        sprintf(buf, "stages window - '%.64s'", ss->cur->name);
    } else {
        sprintf(buf, "stages window");
    }

    igBegin(buf, &opened, flags);

    igText("active stage: %s", ss->cur ? ss->cur->name : NULL);

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody |
        ImGuiTableFlags_Sortable |
        ImGuiTableFlags_SortMulti ;

    //ImGuiInputTextFlags input_flags = 0;

    ImVec2 outer_size = {0., 0.};
    const int columns_num = 8;
    if (igBeginTable("stage", columns_num, table_flags, outer_size, 0.)) {
        // table {{{
        igTableSetupColumn("name", 0, 0, 0);
        igTableSetupColumn("init", 0, 0, 1);
        igTableSetupColumn("shutdown", 0, 0, 2);
        igTableSetupColumn("draw", 0, 0, 3);
        igTableSetupColumn("update", 0, 0, 4);
        igTableSetupColumn("enter", 0, 0, 5);
        igTableSetupColumn("leave", 0, 0, 6);
        igTableSetupColumn("data", 0, 0, 7);
        igTableHeadersRow();

        ImGuiTableSortSpecs* sort_specs = igTableGetSortSpecs();
        if (sort_specs->SpecsDirty) {
            sort_table_specs(sort_specs);
            sort_specs->SpecsDirty = false;
        }

        // TODO: Доделать вывод через клиппер
        /*
        ImGuiListClipper *clipper = ImGuiListClipper_ImGuiListClipper();
        assert(clipper);
        while (ImGuiListClipper_Step(clipper)) {
            int start = clipper->DisplayStart, end = clipper->DisplayEnd;
            for (int row_n = start; row_n < end; row_n++) {
                trace("stages_gui_window: row_n %d\n", row_n);
            }
            trace("stages_gui_window:\n\n");
        }
        ImGuiListClipper_destroy(clipper);
        // */

        for (int i = 0; i < ss->num; ++i) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            char name_label[64] = {};
            sprintf(name_label, "%s", ss->stages[i]->name);
            igTableSetColumnIndex(0);
            if (igSelectable_BoolPtr(
                name_label, &ss->selected[i],
                ImGuiSelectableFlags_SpanAllColumns, (ImVec2){0, 0}
            )) {
                for (int j = 0; j < ss->num; ++j) {
                    if (j != i)
                        ss->selected[j] = false;
                }
            }

            igTableSetColumnIndex(1);
            igText("%p", ss->stages[i]->init);

            igTableSetColumnIndex(2);
            igText("%p", ss->stages[i]->shutdown);

            igTableSetColumnIndex(3);
            igText("%p", ss->stages[i]->draw);

            igTableSetColumnIndex(4);
            igText("%p", ss->stages[i]->update);

            igTableSetColumnIndex(5);
            igText("%p", ss->stages[i]->enter);

            igTableSetColumnIndex(6);
            igText("%p", ss->stages[i]->leave);

            igTableSetColumnIndex(7);
            igText("%p", ss->stages[i]->data);

        }
        igEndTable();
        // }}}
    }

    /*
    static char stage_str_argument[64] = {};
    igInputText(
        "stage string argument",
        stage_str_argument, sizeof(stage_str_argument), input_flags, 0, NULL
    );
    */

    if (igButton("switch to selected stage", (ImVec2) {0, 0})) {
        const Stage *selected = get_selected_stage(ss);
        if (selected)
            stage_active_set(ss, selected->name);
    }

    igSameLine(0., 5.);

    if (igButton("shutdown/init selected", (ImVec2) {0, 0})) {
        Stage *selected = get_selected_stage(ss);
        if (selected) {
            if (selected->shutdown && selected->init) {
                selected->shutdown(selected);
                selected->init(selected);
            }
            //stage_active_set(ss, selected->name, NULL);
        }
    }

    koh_window_post();
    igEnd();
}

void stage_active_gui_render(StagesStore *ss) {
    Stage *st = ss->cur;
    if (st && st->gui) st->gui(st);
}

void stage_free(StagesStore *ss) {
    if (ss) {
        free(ss);
    }
}

ArgV stage_argv(StagesStore *ss) {
    assert(ss);
    return ss->argv;
}

bool stage_arg_check(StagesStore *ss, const char *flag) {
    assert(ss);
    assert(flag);

    if (ss->argv.argc <= 1) 
        return false;

    for (int i = 0; i < ss->argv.argc; i++) {
        if (!strcmp(ss->argv.argv[i], flag)) {
            return true;
        }
    }

    return false;
}
