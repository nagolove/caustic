// vim: fdm=marker
#include "koh_gui_combo.h"
#include "koh_lua.h"
#include <stdio.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include <stdbool.h>
#include <string.h>
#include "koh_logger.h"
#include "lua.h"
#include "lauxlib.h"
#include "cimgui.h"
#include "cimgui_impl.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>

static const Color gui_combo_colors[] = {
// {{{
    LIGHTGRAY,
    GRAY,
    DARKGRAY,
    YELLOW,
    GOLD,
    ORANGE,
    PINK,
    RED,
    MAROON,
    GREEN,
    LIME,
    DARKGREEN,
    SKYBLUE,
    BLUE,
    DARKBLUE,
    PURPLE,
    VIOLET,
    DARKPURPLE,
    BEIGE,
    BROWN,
    DARKBROWN,
    WHITE,
    BLACK,
    MAGENTA,
};
// }}}

static const char *gui_combo_colors_names[] = {
// {{{
    "LIGHTGRAY",
    "GRAY",
    "DARKGRAY",
    "YELLOW",
    "GOLD",
    "ORANGE",
    "PINK",
    "RED",
    "MAROON",
    "GREEN",
    "LIME",
    "DARKGREEN",
    "SKYBLUE",
    "BLUE",
    "DARKBLUE",
    "PURPLE",
    "VIOLET",
    "DARKPURPLE",
    "BEIGE",
    "BROWN",
    "DARKBROWN",
    "WHITE",
    "BLACK",
    "MAGENTA",
};
// }}}

static const int gui_combo_colors_num = sizeof(gui_combo_colors) / 
                                        sizeof(gui_combo_colors[0]);

static void gui_color_combo_set_by_name(
    struct GuiColorCombo *gcc, const char *colorname
) {
    assert(gcc);
    assert(colorname);
    trace("gui_color_combo_set_by_name: colorname %s\n", colorname);
    for (int i = 0; i < gui_combo_colors_num; i++) {
        if (!strcmp(gui_combo_colors_names[i], colorname)) {
            gcc->line_color_selected[i] = true;
            gcc->color_name = gui_combo_colors_names[i];
            for (int j = 0; j < gui_combo_colors_num; ++j) {
                if (i != j)
                    gcc->line_color_selected[j] = false;
            }
            trace("gui_color_combo_set_by_name: found\n");
            return;
        }
    }
    trace("gui_color_combo_set_by_name: not found\n");
}

Color gui_color_combo(struct GuiColorCombo *gcc, bool *changed) {
    assert(gcc);
    const char *preview_value = NULL;

    if (changed)
        *changed = false;

    int prev_selected = -1;

    for (int i = 0; i < gui_combo_colors_num; i++) {
        if (gcc->line_color_selected[i]) {
            preview_value = gui_combo_colors_names[i];
            prev_selected = i;
            break;
        }
    }

    if (igBeginCombo(gcc->label, preview_value, 0)) {
        for (int i = 0; i < gui_combo_colors_num; i++) {
            if (igSelectable_BoolPtr(
                gui_combo_colors_names[i], 
                &gcc->line_color_selected[i], 
                0, (ImVec2){}
            )) {
                if (i != prev_selected && changed) {
                    *changed = true;
                }
                gcc->color_name = gui_combo_colors_names[i];
                for (int j = 0; j < gui_combo_colors_num; j++) {
                    if (i != j)
                        gcc->line_color_selected[j] = false;
                }
            }
        }
        igEndCombo();
    }

    for (int j = 0; j < gui_combo_colors_num; j++) {
        if (gcc->line_color_selected[j]) {
            gcc->color = gui_combo_colors[j];
        }
    }
    return gcc->color;
}

struct GuiColorCombo gui_color_combo_init(const char *label) {
    struct GuiColorCombo gcc = {
    };
    gcc.line_color_selected = calloc(
        gui_combo_colors_num,
        sizeof(gcc.line_color_selected[0])
    );

    const int initial_color_index = 0;
    gcc.line_color_selected[initial_color_index] = true;
    gcc.color_name = gui_combo_colors_names[initial_color_index];

    if (label) {
        strncpy(gcc.label, label, sizeof(gcc.label) - 1);
        gcc.label[sizeof(gcc.label) - 1] = 0;
    }

    return gcc;
}

void gui_color_combo_shutdown(struct GuiColorCombo *gcc) {
    assert(gcc);
    if (gcc->line_color_selected) {
        free(gcc->line_color_selected);
        gcc->line_color_selected = NULL;
    }
}

void gui_color_combo_load(struct GuiColorCombo *gcc, const char *lua_fname) {
    assert(gcc);
    assert(lua_fname);
    lua_State *l = luaL_newstate();
    if (luaL_loadfile(l, lua_fname) != LUA_OK) {
        trace(
            "gui_color_combo_load: could not load file '%s' with '%s'\n",
            lua_fname, lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_call(l, 0, LUA_MULTRET);
    if (lua_type(l, lua_gettop(l)) != LUA_TTABLE) {
        trace(
            "gui_color_combo_load: fail to load table with '%s'\n",
            lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_pushstring(l, gcc->label);
    lua_gettable(l, -2);
    
    if (lua_type(l, lua_gettop(l)) != LUA_TSTRING) {
        trace(
            "gui_color_combo_load: could not read string value by key '%s'\n",
            gcc->label
        );
        goto _cleanup;
    }

    gui_color_combo_set_by_name(gcc, lua_tostring(l, -1));
_cleanup:
    lua_close(l);
}

void gui_color_combo_save(struct GuiColorCombo *gcc, const char *lua_fname) {
    assert(gcc);
    assert(lua_fname);
    lua_State *l = luaL_newstate();
    if (luaL_loadfile(l, lua_fname) != LUA_OK) {
        trace(
            "gui_color_combo_save: could not load file '%s' with '%s'\n",
            lua_fname, lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_call(l, 0, LUA_MULTRET);
    if (lua_type(l, lua_gettop(l)) != LUA_TTABLE) {
        trace(
            "gui_color_combo_save: fail to load table with '%s'\n",
            lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_pushstring(l, gcc->label);
    // TODO: Проверка на существование цвета данного имени
    assert(gcc->color_name);
    lua_pushstring(l, gcc->color_name);
    lua_settable(l, -3);
   
    char *dump = L_table_dump2allocated_str(l);
    if (dump) {
        trace("gui_color_combo_save: dump '%s'\n", dump);
        FILE *file = fopen(lua_fname, "w");
        if (file) {
            fprintf(file, "%s", dump);
            fclose(file);
        }
        free(dump);
    }

_cleanup:
    lua_close(l);
}

// */

struct GuiCombo gui_combo_init(
    const char **labels, const void *items, int item_sz, int items_num
) {
    assert(labels);
    assert(items);
    assert(item_sz > 0);
    assert(items_num >= 0);

    struct GuiCombo gc = { };
    gc.selected = calloc(items_num, sizeof(gc.selected[0]));

    gc.labels = calloc(items_num, sizeof(gc.labels[0]));
    assert(gc.labels);
    for (int i = 0; i < items_num; ++i) {
        assert(labels[i]);
        gc.labels[i] = strdup(labels[i]);
    }

    printf(
        "gui_combo_init: items_sz %d, items_num %d\n", item_sz, items_num
    );

    int items_arr_sz = item_sz * items_num;
    gc.items = malloc(items_arr_sz);
    assert(gc.items);
    memcpy(gc.items, items, items_arr_sz);

    gc.items_num = items_num;
    gc.item_sz = item_sz;

    gc.selected[0] = true;
    gc.label = "Undefined label";

    return gc;
}

static void gui_combo_set_by_name(
    struct GuiCombo *gc, const char *name
) {
    assert(gc);
    assert(name);
    trace("gui_combo_set_by_name: name %s\n", name);
    for (int i = 0; i < gc->items_num; i++) {
        if (!strcmp(gc->labels[i], name)) {
            gc->selected[i] = true;
            gc->selected_name = gc->labels[i];
            for (int j = 0; j < gc->items_num; ++j) {
                if (i != j)
                    gc->selected[j] = false;
            }
            trace("gui_combo_set_by_name: found\n");
            return;
        }
    }
    trace("gui_combo_set_by_name: not found\n");
}

void gui_combo_load(struct GuiCombo *gc, const char *lua_fname) {
    assert(gc);
    assert(lua_fname);
    lua_State *l = luaL_newstate();
    if (luaL_loadfile(l, lua_fname) != LUA_OK) {
        trace(
            "gui_color_combo_load: could not load file '%s' with '%s'\n",
            lua_fname, lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_call(l, 0, LUA_MULTRET);
    if (lua_type(l, lua_gettop(l)) != LUA_TTABLE) {
        trace(
            "gui_color_combo_load: fail to load table with '%s'\n",
            lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_pushstring(l, gc->label);
    lua_gettable(l, -2);
    
    if (lua_type(l, lua_gettop(l)) != LUA_TSTRING) {
        trace(
            "gui_color_combo_load: could not read string value by key '%s'\n",
            gc->label
        );
        goto _cleanup;
    }

    gui_combo_set_by_name(gc, lua_tostring(l, -1));
_cleanup:
    lua_close(l);
}

void gui_combo_save(struct GuiCombo *gc, const char *lua_fname) {
    assert(gc);
    assert(lua_fname);
    lua_State *l = luaL_newstate();
    if (luaL_loadfile(l, lua_fname) != LUA_OK) {
        trace(
            "gui_combo_save: could not save file '%s' with '%s'\n",
            lua_fname, lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_call(l, 0, LUA_MULTRET);
    if (lua_type(l, lua_gettop(l)) != LUA_TTABLE) {
        trace(
            "gui_combo_save: fail to save table with '%s'\n",
            lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_pushstring(l, gc->label);
    // TODO: Проверка на существование цвета данного имени
    assert(gc->label);
    lua_pushstring(l, gc->selected_name);
    lua_settable(l, -3);
   
    char *dump = L_table_dump2allocated_str(l);
    if (dump) {
        trace("gui_combo_save: dump '%s'\n", dump);
        FILE *file = fopen(lua_fname, "w");
        if (file) {
            fprintf(file, "%s", dump);
            fclose(file);
        }
        free(dump);
    }

_cleanup:
    lua_close(l);
}

void gui_combo_shutdown(struct GuiCombo *gc) {
    assert(gc);
    if (gc->selected) {
        free(gc->selected);
        gc->selected = NULL;
    }
    if (gc->labels) {
        for (int i = 0; i < gc->items_num; ++i) {
            free(gc->labels[i]);
        }
        free(gc->labels);
    }
    if (gc->items) {
        free(gc->items);
        gc->items = NULL;
    }
}

const void *gui_combo(struct GuiCombo *gc, bool *changed) {
    assert(gc);
    const char *preview_value = NULL;

    if (changed)
        *changed = false;

    int prev_selected = -1;

    for (int i = 0; i < gc->items_num; i++) {
        if (gc->selected[i]) {
            preview_value = gc->labels[i];
            prev_selected = i;
            break;
        }
    }

    if (igBeginCombo(gc->label, preview_value, 0)) {
        for (int i = 0; i < gc->items_num; i++) {
            if (igSelectable_BoolPtr(
                gc->labels[i], 
                &gc->selected[i], 
                0, (ImVec2){}
            )) {
                if (i != prev_selected && changed) {
                    if (gc->on_change) 
                        gc->on_change(gc, prev_selected, i);
                    *changed = true;
                }
                /*gc->color_name = gui_combo_colors_names[i];*/
                gc->selected_name = gc->labels[i];
                for (int j = 0; j < gc->items_num; j++) {
                    if (i != j)
                        gc->selected[j] = false;
                }
            }
        }
        igEndCombo();
    }

    for (int j = 0; j < gc->items_num; j++) {
        if (gc->selected[j]) {
            gc->selected_item = &((char*)gc->items)[j * gc->item_sz];
        }
    }

    return gc->selected_item;
}

