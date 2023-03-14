#pragma once

#include "raylib.h"

#include <stdbool.h>

#define MAX_CONTROLS        32
#define CONTAINER_MAX_NAME  64
#define MAX_BUTTON_CAPTION  128

typedef struct VContainer VContainer;
typedef struct Control Control;
typedef void (*ControlFunc)(Control *ctrl);

typedef enum ControlType {
    CT_UNUSED_FIRST = 0,
    CT_BUTTON       = 1,
    CT_SLIDER       = 2,
    CT_UNUSED_LAST  = 3,
} ControlType;

typedef struct Control {
    ControlType type;
    ControlFunc init, shutdown;
    ControlFunc update;
    ControlFunc click;

    Rectangle (*get_rect)(Control *ctrl);
    void (*set_rect)(Control *ctrl, Rectangle rect);

    VContainer *container;
    void *data;
} Control;

struct IInputSources {
    bool (*is_up)(void *udata);
    bool (*is_down)(void *udata);
    bool (*is_select)(void *udata);
    Vector2 (*mouse_position)(void *udata);
    bool (*mouse_is_button_pressed)(int button, void *udata);
};

// Содержит элементы управления в вертикальной раскладке.
typedef struct VContainer {
    struct VContainer *next;

    struct IInputSources inp;
    char name[CONTAINER_MAX_NAME];
    bool is_builded;
    Control **ctrls;
    int ctrlsnum;
    int selected;
    Vector2 pos; // Позиция рисования
    Vector2 mousep;
    Rectangle border; // Рамка вокруг меню
    Color color_border;
    //bool is_mnu_open, is_mnu;
    bool is_mnu_open;

    int mousebtn_open;
    int mousebtn_click;
    void *data; // user context
} VContainer;

typedef struct Button {
    Control ctrl;
    Font fnt;
    Rectangle rect;
    char caption[MAX_BUTTON_CAPTION];
    // Цвет фона, текста, выделенной кнопки
    Color color, color_caption, color_selected;
} Button;

typedef struct Button_Def {
    Font fnt;
    char *caption;
    ControlFunc click;
} Button_Def;

typedef struct Slider {
    Control ctrl;
} Slider;

VContainer *vcontainer_new(
    int mousebtn_open, const char *name, struct IInputSources inp
);
void vcontainer_shutdown(VContainer *c);

void vcontainers_init();
void vcontainers_shutdown();

Control* vcontainer_add(VContainer *c, Control *control);
void vcontainer_build(VContainer *c);
void vcontainer_get_size(VContainer *c, float *width, float *height);

bool vcontainer_update(VContainer *c, void *udata);

// Для передвижения между пунктами меню и действия
void vcontainer_next(VContainer *c);
void vcontainer_prev(VContainer *c);
void vcontainer_ok(VContainer *c);

Button *vcontainer_find(VContainer *c, const char *caption);
//bool vcontainer_update_gp(VContainer *c, int mousebtn, bool usegamepad);
void vcontainer_open(VContainer *c, Vector2 pos);
bool vcontainer_is_in_area(VContainer *c, Vector2 pos);

Control *btn_new(Button_Def def);
void btn_done(Button *btn);
