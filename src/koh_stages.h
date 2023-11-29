#pragma once

#include "lua.h"
#include "raylib.h"
#include "koh_common.h"
#include "koh_logger.h"
#include "koh_lua_tools.h"

/*
TODO: Пока сцена не становится активной, то ее хоткеи не работают.
*/

struct Stage;

#define MAX_STAGE_NAME  64

typedef void (*Stage_callback)(struct Stage *s);
typedef void (*Stage_data_callback)(struct Stage *s, const char *str);
typedef struct StagesStore StagesStore;

typedef struct Stage {
    StagesStore         *store;
    Stage_data_callback init, enter, leave;
    Stage_callback      shutdown, draw, update, gui;
    void                *data;
    char                name[MAX_STAGE_NAME];
} Stage;

struct StageStoreSetup {
    lua_State   *l;
    char        *stage_store_name;
};

// Инициализация подсистемы
StagesStore *stage_new(struct StageStoreSetup *setup);
// Добавить сцену. Внутри - st освобождается через free()
Stage *stage_add(StagesStore *ss, Stage *st, const char *name);
// Инициализация добавленных сцен
void stage_init(StagesStore *ss);
// Деинициализация всех сцен
void stage_shutdown(StagesStore *ss);
void stage_free(StagesStore *ss);
// Установить текущую сцену
void stage_active_set(StagesStore *ss, const char *name, void *data);
// Обработать сцену
void stage_active_update(StagesStore *ss);
// Получить имя текущей сцены
const char *stage_active_name_get(StagesStore *ss);
Stage *stage_find(StagesStore *ss, const char *name);

// Где применяется?
//void *stage_assert(Stage *st, const char *name);

// Распечатать имена сцен
void stage_print(StagesStore *ss);

// ImGui окошко работы со сценами
void stages_gui_window(StagesStore *ss);

void stage_active_gui_render(StagesStore *ss);

KOH_FORCE_INLINE static Stage *l_stage_find(
    lua_State *l, const char *store_name, const char *stage_name
) {
    assert(l);
    //trace("l_stage_find: [%s]\n", stack_dump(l));
    lua_getglobal(l, store_name);
    StagesStore *ss = lua_touserdata(l, -1);
    assert(ss);
    lua_pop(l, 1);
    //trace("l_stage_find: [%s]\n", stack_dump(l));
    return stage_find(ss, stage_name);
}
