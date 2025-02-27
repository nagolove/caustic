#pragma once

#include <stdlib.h>
#include "raylib.h"

struct GuiColorCombo {
    // Имя комбобокса
    char        label[128];
    // Выбранный цвет
    Color       color;
    // Выбранное имя цвета
    const char  *color_name;
    // Для служебного пользования
    bool        *line_color_selected;
};

struct GuiColorCombo gui_color_combo_init(const char *label);
void gui_color_combo_load(struct GuiColorCombo *gcc, const char *lua_fname);
void gui_color_combo_save(struct GuiColorCombo *gcc, const char *lua_fname);
void gui_color_combo_shutdown(struct GuiColorCombo *gcc);
Color gui_color_combo(struct GuiColorCombo *gcc, bool *changed);

struct GuiCombo {
    // Имя комбобокса
    const char  *label;
    // Надпись на выбранном пункте
    const char  *selected_name;
    // Выбранные данные
    const void  *selected_item;
    int         item_sz, items_num;
    char        **labels;
    void        *items;

    // Для служебного пользования
    // TODO: Спрятать под прозрачный указатель если появятся еще поля.
    bool        *selected;

    void        (*on_change)(struct GuiCombo *gc,
                             int prev_selected,
                             int new_selected);
};


struct GuiCombo gui_combo_init(
    const char **labels, const void *items, int item_sz, int items_num
);
void gui_combo_load(struct GuiCombo *gc, const char *lua_fname);
void gui_combo_save(struct GuiCombo *gc, const char *lua_fname);
void gui_combo_shutdown(struct GuiCombo *gc);
const void *gui_combo(struct GuiCombo *gc, bool *changed);

