// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include <chipmunk/chipmunk.h>
#include <chipmunk/chipmunk_structs.h>
#include <chipmunk/chipmunk_types.h>
#include <raylib.h>

#include "stage_fight.h"
#include "layer.h"
#include "das_interface.h"
#include "flames.h"
#include "graph.h"
#include "hotkey.h"
#include "iface.h"
#include "object.h"
#include "rand.h"
//#include "resource.h"
#include "stages.h"
#include "tile.h"
#include "timer.h"
#include "weapon.h"
#include "team.h"

/*
typedef struct Defaults {
    Rectangle tank_source, turret_source;
    Rectangle tree_source;
    Rectangle hangar_source, teleport_source;
    Rectangle xturret_b_source, xturret_t_source;
    Rectangle helicopter_source, helicopter_blades;

    float tree_stump_radius;
    Vector2 xturret_rot_point;
    // Максимальная скорость, максимальная угловая скорость
    float velocity_limit, avelocity_limit;
    Vector2 anchor_b, turret_rot_point;
    Vector2 teleport_radare_offset;

    int tank_radius;    // Окружность в которую влезает танк
    int hangar_radius;  // Окружность в которую влезает ангар
    int teleport_radius;
    int xturret_radius;

    // Импульс для перемещения, импульс для вращения башни
    float impulse_amount, rot_impulse_amount;
    // Длина ствола. Используется для расчета точки вылета снаряда.
    float barrel_lenght;
    // Время горения вспышки при попадании.
    double flame_anim_time;
    // Количество едениц площади на которую создается один танк, ангар, 
    // телепорт или дерево
    int sq_per_tank, sq_per_hangar, sq_per_teleport, sq_per_tree;
    // Коэффициент скорости передвижения камеры
    float camera_move_k;
} Defaults;
*/

typedef struct Stage_Fight2 {
    Stage parent;
    // {{{
    // Флаги состояния - сцена инициализарована, загружена
    StageStage state;
    // Константы определяемые по-умолчанию на этапе7 создания сцены
    Defaults defs;

    // Площать сцены в квадратах на rez пикселей
    // Проходимая площадь сцены
    int square, square_pass;

    // Счетчик для bb запросов
    int query_counter;  

    Layer layer_helicopters;
    
    HotkeyStorage *hk_storage;

    // Хранилище игровых объектов
    ObjectStorage obj_store;    
                                
    // ГПСЧ для объектов
    xorshift32_state rng_objects; 

    // Запрос на видимую часть экрана в физический движок
    cpBB bb_query;

    // Карта высот и ее отображение
    Tiles t;

    TimerStore timers;

    Vector2 minimap_pos;

    //Resource **res_textures;

    // {{{ assets
    Font fnt_first, fnt_second; // Большой и малый шрифт
    Font fnt_hud;

    bool is_action_remove, is_action_select_tank, is_action_new;

    StatusBar *sbar;

    float zoom_step;
    CamMode camera_mode;

    Color space_debug_color;
    bool is_l_update;
    // }}}
} Stage_Fight2;

bool get_cell_under_local_point(
        Stage_Fight *st,
        Vector2 local_pos,
        int *i, int *j
);
bool get_cell_under_world_point(
        Stage_Fight *st,
        Vector2 world_pos,
        int *i, int *j
);

// Можно-ли пройти через данную ячейку? Определяется по высоте ландшафта.
bool is_passability(Tiles *t, int i, int j);
bool is_passability_safe(Tiles *t, int i, int j);

Texture2D team_get_decal(Stage_Fight *st, int team);

extern bool is_space_debug_draw;
extern bool is_draw_tile_border;
extern bool is_draw_minimap;
extern bool is_graph_visible;
extern bool is_utiles_visible;
extern bool is_regions_draw;
extern bool is_draw_under_point;
extern cpShapeFilter ALL_FILTER;

Stage *stage_fight2_new(HotkeyStorage *hk_store);
