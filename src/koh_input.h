// vicolor: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct InputBase {
    // прокрутка меню вверх
    bool (*is_up)();
    // прокрутка меню вниз
    bool (*is_down)();
    // подтверждение выбора
    bool (*is_select)();
    // меню игрока
    bool (*is_player_menu)();
    // движение вперед
};

typedef struct Input {
    struct InputBase parent;

    // прокрутка меню вверх
    bool (*is_up)();
    // прокрутка меню вниз
    bool (*is_down)();
    // подтверждение выбора
    bool (*is_select)();
    // меню игрока
    bool (*is_player_menu)();
    // движение вперед

    bool (*is_forward)();
    // движение назад
    bool (*is_backward)();

    // поворот вправо
    bool (*is_rot_right)();
    // поворот влево
    bool (*is_rot_left)();
    // педаль газа
    float (*is_accel)();
    // клавиша выстрела
    float (*is_fire)();
    // поворот башни
    float (*stick_turret_x)();
    // поворот башни
    float (*stick_turret_y)();
    // перемотка карты
    float (*stick_map_x)();
    // перемотка карты
    float (*stick_map_y)();
    // переключение режима карты
    bool (*camera_mode_change)();
} Input;

typedef enum InputAction {
    INP_ACT_player_menu        = 0,
    INP_ACT_is_up              = 1,
    INP_ACT_is_down            = 2,
    INP_ACT_is_select          = 3,
    INP_ACT_is_forward         = 4,
    INP_ACT_is_backward        = 5,
    INP_ACT_is_rot_right       = 6,
    INP_ACT_is_rot_left        = 7,
    INP_ACT_is_accel           = 8,
    INP_ACT_is_fire            = 9,
    INP_ACT_stick_turret_x     = 10,
    INP_ACT_stick_turret_y     = 11,
    INP_ACT_stick_map_x        = 12,
    INP_ACT_stick_map_y        = 13,
    INP_ACT_camera_mode_change = 14,
} InputAction;

#define INPUT_ACT_MAX_VALUE 14

typedef struct InputActionBuf {
    uint32_t    act;
    uint32_t    arg; // bool or float
} InputActionBuf;

extern Input input;

void input_init(void);
void input_shutdown(void);
bool input_gamepad_check();
void input_gp_print_changed_axises(void);
void input_gp_print_changed_buttons(void);
bool input_buffer_get(InputActionBuf *action, int index);
const char *input_action2str(uint32_t action);
