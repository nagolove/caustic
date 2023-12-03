#pragma once

#include "lua.h"
#include "raylib.h"
#include "koh_common.h"
#include "koh_logger.h"
#include "koh_lua_tools.h"

/*
TODO: Пока сцена не становится активной, то ее хоткеи не работают.
*/

struct Stage2;

#define MAX_STAGE_NAME  64

typedef void (*Stage_callback)(struct Stage2 *s);
typedef void (*Stage_data_callback)(struct Stage2 *s, const char *str);
typedef struct StagesStore2 StagesStore2;

// TODO: Оставить одну структуру сцены. В ней хранить подсцены вместо
// StagesStore
typedef struct Stage2 {
    StagesStore2        *store;
    Stage_data_callback init, enter, leave;
    Stage_callback      shutdown, draw, update, gui;
    void                *data;
    char                name[MAX_STAGE_NAME];
} Stage2;

struct StageStoreSetup2 {
    lua_State   *l;
    char        *stage_store_name;
};

// Инициализация подсистемы
Stage2 *stage2_new(struct StageStoreSetup2 *setup);
// Добавить сцену. Внутри - st освобождается через free()
Stage2 *stage2_add(Stage2 *ss, Stage2 *st, const char *name);
// Инициализация добавленных сцен
void stage2_init(Stage2 *ss);
// Деинициализация всех сцен
void stage2_shutdown(Stage2 *ss);
void stage2_free(Stage2 *ss);
// Установить текущую сцену
void stage2_active_set(Stage2 *ss, const char *name, void *data);
// Обработать сцену
void stage2_active_update(Stage2 *ss);
// Получить имя текущей сцены
const char *stage2_active_name_get(Stage2 *ss);
Stage2 *stage2_find(Stage2 *ss, const char *name);

// Где применяется?
//void *stage2_assert(Stage *st, const char *name);

// Распечатать имена сцен
void stage2_print(Stage2 *ss);

// ImGui окошко работы со сценами
void stage2_gui_window(Stage2 *ss);

void stage2_active_gui_render(Stage2 *ss);

KOH_FORCE_INLINE static Stage2 *l_stage2_find(
    lua_State *l, const char *store_name, const char *stage_name
) {
    assert(l);
    //trace("l_stage2_find: [%s]\n", stack_dump(l));
    lua_getglobal(l, store_name);
    // XXX: Полученный указатель light user data не безопасен
    // Как добавить ему проверку на тип объекта?
    Stage2 *ss = lua_touserdata(l, -1);
    assert(ss);
    lua_pop(l, 1);
    //trace("l_stage2_find: [%s]\n", stack_dump(l));
    return stage2_find(ss, stage_name);
}
