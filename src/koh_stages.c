// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stages.h"

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

struct StagesStore {
    Stage    *stages[MAX_STAGES_NUM];
    bool     selected[MAX_STAGES_NUM];
    Stage    *cur;
    uint32_t num;
};

static struct StagesStore stages = {0, };

int l_stages_get(lua_State *lua);
int l_stage_restart(lua_State *lua);
int l_stage_set_active(lua_State *lua);
int l_stage_get_active(lua_State *lua);

void stage_init(void) {
    memset(&stages, 0, sizeof(stages));
    
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
}

Stage *stage_add(Stage *st, const char *name) {
    assert(st);
    assert(name);
    assert(strlen(name) < MAX_STAGE_NAME);
    assert(stages.num <= MAX_STAGES_NUM);

    stages.stages[stages.num++] = st;
    strncpy(st->name, name, MAX_STAGE_NAME - 1);
    return st;
}

void stage_shutdown_all(void) {
    for(int i = 0; i < stages.num; i++) {
        Stage *st = stages.stages[i];
        if (st) {
            if (st->shutdown) 
                st->shutdown(st);
            free(st);
            stages.stages[i] = NULL;
        }
    }
}

// TODO: Разделить обновление и рисование в разные функции
void stage_update_active(void) {
    Stage *st = stages.cur;
    if (st && st->update) st->update(st);
    if (st && st->draw) st->draw(st);
}

Stage *stage_find(const char *name) {
    for(int i = 0; i < stages.num; i++) {
        Stage *st = stages.stages[i];
        if (!strcmp(name, st->name)) {
            return st;
        }
    }
    return NULL;
}

void stage_set_active(const char *name, void *data) {
    Stage *st = stage_find(name);
    //assert(st);

    if (!st) {
        trace("stage_set_active: '%s' not found\n", name);
    }

    if (stages.cur && stages.cur->leave) {
        trace(
            "stage_set_active: leave from '%s'\n",
            stages.cur->name
        );
        stages.cur->leave(stages.cur, data);
    }

    stages.cur = st;

    if (st && st->enter) {
        trace("stage_set_active: enter to '%s'\n", st->name);
        st->enter(st, data);
    }
}

void stage_subinit(void) {
    for(int i = 0; i < stages.num; i++) {
        Stage *st = stages.stages[i];
        if (st->init) {
            st->init(st, NULL);
        }
    }
}

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

void stages_print() {
    printf("stages_print:\n");
    for (int i = 0; i < stages.num; i++) {
        printf("stages_print: '%s'\n", stages.stages[i]->name);
    }
}

const char *stage_get_active_name() {
    if (stages.cur) {
        static char ret_buf[MAX_STAGE_NAME] = {0};
        strcpy(ret_buf, stages.cur->name);
        return ret_buf;
    }
    return NULL;
}

void *stage_assert(Stage *st, const char *name) {
    assert(st);
    Stage *res = stage_find(st->name);
    assert(res);
    return res;
}

static Stage *get_selected_stage() {
    for (int i = 0; i < stages.num; i++) {
        if (stages.selected[i])
            return stages.stages[i];
    }
    return NULL;
}

void stages_gui_window() {
    bool opened = true;
    ImGuiWindowFlags flags = 0;
    igBegin("stages window", &opened, flags);

    igText("active stage: %s", stages.cur ? stages.cur->name : NULL);

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

        for (int i = 0; i < stages.num; ++i) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            char name_label[64] = {};
            sprintf(name_label, "%s", stages.stages[i]->name);
            igTableSetColumnIndex(0);
            if (igSelectable_BoolPtr(
                name_label, &stages.selected[i],
                ImGuiSelectableFlags_SpanAllColumns, (ImVec2){0, 0}
            )) {
                for (int j = 0; j < stages.num; ++j) {
                    if (j != i)
                        stages.selected[j] = false;
                }
            }

            igTableSetColumnIndex(1);
            igText("%p", stages.stages[i]->init);

            igTableSetColumnIndex(2);
            igText("%p", stages.stages[i]->shutdown);

            igTableSetColumnIndex(3);
            igText("%p", stages.stages[i]->draw);

            igTableSetColumnIndex(4);
            igText("%p", stages.stages[i]->update);

            igTableSetColumnIndex(5);
            igText("%p", stages.stages[i]->enter);

            igTableSetColumnIndex(6);
            igText("%p", stages.stages[i]->leave);

            igTableSetColumnIndex(7);
            igText("%p", stages.stages[i]->data);

        }
        igEndTable();
    }

    static char stage_str_argument[64] = {};
    igInputText(
        "stage string argument",
        stage_str_argument, sizeof(stage_str_argument), input_flags, 0, NULL
    );

    if (igButton("switch to selected stage", (ImVec2) {0, 0})) {
        const Stage *selected = get_selected_stage();
        if (selected)
            stage_set_active(selected->name, NULL);
    }

    igEnd();
}

void stage_active_gui_render() {
    Stage *st = stages.cur;
    if (st && st->gui) st->gui(st);
}
