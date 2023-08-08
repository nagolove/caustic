#pragma once

#include "raylib.h"

/*
    TODO:
    Пока сцена не становится активной, то ее хоткеи не работают.
 */

#define MAX_STAGES_NUM  32
#define MAX_STAGE_NAME  64

struct Stage;

typedef void (*Stage_callback)(struct Stage *s);
typedef void (*Stage_data_callback)(struct Stage *s, void *data);

typedef struct Stage {
    Stage_data_callback init;
    Stage_callback      shutdown;
    Stage_callback      draw, update, gui;
    Stage_data_callback enter, leave;

    void *data;
    char name[MAX_STAGE_NAME];
} Stage;

// Инициализация подсистемы
void stage_init(void);
// Добавить сцену
Stage *stage_add(Stage *st, const char *name);
// Инициализация добавленных сцен
void stage_subinit(void);
// Деинициализация всех сцен
void stage_shutdown_all(void);
// Установить текущую сцену
void stage_set_active(const char *name, void *data);
// Обработать сцену
void stage_update_active(void);
// Получить имя текущей сцены
const char *stage_get_active_name();
Stage *stage_find(const char *name);
// Где применяется?
void *stage_assert(Stage *st, const char *name);
// Распечатать имена сцен
void stages_print();
// ImGui окошко работы со сценами
void stages_gui_window();
void stage_active_gui_render();
