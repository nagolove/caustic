// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_sprite_loader2.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "koh_common.h"
#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_lua.h"
#include "koh_visual_tools.h"
#include "lauxlib.h"
#include "lua.h"
#include "raylib.h"
#include "koh_gui_combo.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "koh_layered_sprite.h"

/*
enum LastSelected {
    LAST_SELECTED_NONE       = 0,
    LAST_SELECTED_IMAGE      = 1,
    LAST_SELECTED_ASE_EXPORT = 2,
};
*/

struct Grid {
    struct GuiColorCombo        line_color_combo;
    Color                       line_color;
    bool                        visible;
    int                         step;
};

// TODO: Загрузка нескольких спрайтов.
// Значит есть список спрайтов
//
// // TODO: Загрузка нескольких спрайтов.
// Значит есть список спрайтов в ListBox()
// Как экспортировать в lua файл?
// Сперва построить дерево, потом перевести в текст?
// Файл представляет собой "return {}" код возвращающий таблицу с таблицами
// Работать с одним или несколькими файлами?
// Как быть с текущей позицией курсора мыши?

typedef struct Stage_SpriteLoader {
    Stage                       parent;

    CameraAutomat               cam_automat;

    // TODO: Выкинуть на хуй
    // Писать интейфейс для конкретных типов - сау, пехота, строение и т.д.
    // Сделать рефакторинг так, что-бы ничего не упало.
    //MetaLoader                  *metaloader;
    struct VisualTool           tool_visual;

    struct FilesSearchResult    fsr_images;
    struct FilesSearchResult    fsr_meta;
    struct FilesSearchResult    fsr_ase_exported;

    // Массивы, в которых один элемент - выбранный.
    // Соотносятся с переменными fsr_*
    bool                        *images_selected, 
                                *meta_selected,
                                *ase_exported_selected;

    Texture                     *textures;
    RenderTexture2D             *rt_textures;
    int                         textures_num;

        /*
                                // XXX: За что отвечает переменная?
                                meta_loaded_index; 
                                */

    struct LayeredSprite        *textures_ase_exported;
    int                         textures_ase_exported_num;
    int                         selected_sprite_layer;

    /*enum LastSelected           last_selected;*/
    RenderTexture2D             active_sprite;

    Camera2D                    cam;

    struct GuiCombo             toolmode_combo;
    struct GuiColorCombo        tool_color_combo;

    struct Grid                 grid;

    lua_State                   *l_cfg;
    int                         ref_cfg_tbl, ref_ase_exported_tbl;
} Stage_SpriteLoader;

static char regex_pattern_exclude_images[512] = "";
static char regex_pattern_exclude_ase_exported[512] = "";
static const char *path_assets = "assets";
static const char *path_meta = "assets/meta";
static const char *path_meta_pattern = ".*\\.lua$";
static char regex_pattern_images[128] = ".*\\.png$";
static char regex_pattern_ase_exported[128] = ".*\\.aseprite\\.lua$";
static const char *cfg_fname = "sprite_loader.lua";
//static const float camera_dscale_value = 0.01;
static const float tbl_tex_size = 700.;
static const int font_size_scaled = 40;

static void search_images(
    Stage_SpriteLoader *st, 
    const char *regex_pattern_file,
    const char *regex_pattern_exclude
);
static void search_images_shutdown(Stage_SpriteLoader *st);

static void search_ase_exported(
    Stage_SpriteLoader *st, 
    const char *regex_pattern_file,
    const char *regex_pattern_exclude
);

static const char *get_selected_image(Stage_SpriteLoader *st) {
    assert(st);
    static char name_buf[512] = {};
    const size_t sz = sizeof(name_buf) - 1;
    for (int i = 0; i < st->fsr_images.num; i++) {
        if (st->images_selected[i]) {
            strncpy(name_buf, st->fsr_images.names[i], sz);
            return name_buf;
        }
    }
    return NULL;
}

static const char *get_selected_ase_exported(
    Stage_SpriteLoader *st, int *selected_index
) {
    assert(st);
    static char name_buf[512] = {};
    const size_t sz = sizeof(name_buf) - 1;
    for (int i = 0; i < st->fsr_ase_exported.num; i++) {
        if (st->ase_exported_selected[i]) {
            strncpy(name_buf, st->fsr_ase_exported.names[i], sz);
            if (selected_index)
                *selected_index = i;
            return name_buf;
        }
    }
    return NULL;
}

static void toolmode_combo_changed(struct GuiCombo *gc, int prev, int new) {
    assert(prev >= 0);
    assert(prev < gc->items_num);
    assert(new >= 0);
    assert(new < gc->items_num);
    trace(
        "toolmode_combo_changed: from %s to %s\n",
        gc->labels[prev], gc->labels[new]
    );
}

static void stage_sprite_loader_update(struct Stage *s) {
    Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;
    assert(st);

    if (!st->active_sprite.id)
        return;

    koh_camera_process_mouse_drag(&(struct CameraProcessDrag) {
        .mouse_btn = MOUSE_BUTTON_MIDDLE,
        .cam = &st->cam
    });

    /*
    Хорошая перемотка работает с инерцией, как в браузере при листании страниц.
    При большой скорости вращения колесика - быстрый зум.
    При небольших изменениях - точное масштабирование.
    */

    /*
    koh_camera_process_mouse_scale_wheel(&(struct CameraProcessScale) {
        .cam = &st->cam,
        .dscale_value = camera_dscale_value,
        .modifier_key_down = KEY_LEFT_SHIFT,
        .boost_modifier_key_down = KEY_LEFT_CONTROL,
    });
    */

    cam_auto_update(&st->cam_automat);

    visual_tool_update(&st->tool_visual, &st->cam);
    /*koh_try_ray_cursor();*/
}

static void grid_draw(struct Stage_SpriteLoader *st, Rectangle dst) {
    assert(st);
    if (!st->grid.visible)
        return;

    float line_thick = 1.;
    if (st->grid.step == 1)
        line_thick = 0.5;

    for (int x = 0; x < dst.width + st->grid.step; x += st->grid.step) {
        DrawLineEx((Vector2) {
                dst.x + x,
                dst.y
            }, (Vector2) {
                dst.x + x,
                dst.height
            },
            line_thick,
            st->grid.line_color
        );
    }
    for (int y = 0; y < dst.height + st->grid.step; y += st->grid.step) {
        DrawLineEx((Vector2) {
                dst.x,
                dst.y + y
            }, (Vector2) {
                dst.width,
                dst.y + y
            },
            line_thick,
            st->grid.line_color
        );
    }
}

static void active_sprite_draw(Stage_SpriteLoader *st) {
    assert(st);
    if (!st->active_sprite.id)
        return;
    Texture *tex = &st->active_sprite.texture;
    Rectangle src = { 0, 0, tex->width, tex->height, };
    Rectangle dst = { 0, 0, tex->width * 1.f, tex->height * 1.f, };
    Vector2 origin = {};
    /*ClearBackground(GRAY);*/
    DrawTexturePro(*tex, src,  dst, origin, 0.f, WHITE);
    float line_thick = 5.;
    line_thick /= st->cam.zoom;
    DrawRectangleLinesEx(dst, line_thick, BLACK);
    grid_draw(st, dst);
    /*trace("active_sprite_draw:\n");*/
}

static void stage_sprite_loader_draw(struct Stage *s) {
    Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;
    /*BeginDrawing();*/
    ClearBackground(GRAY);
    BeginMode2D(st->cam);
    active_sprite_draw(st);
    visual_tool_draw(&st->tool_visual, &st->cam);
    EndMode2D();
    /*EndDrawing();*/
}

static void load_string(
    Stage_SpriteLoader *st,
    const char *str_name,
    //const char *str_default,
    char *str_dest,
    size_t max_dest_sz
) {
    assert(st);
    assert(str_name);
    assert(str_dest);
    assert(max_dest_sz > 0);
    lua_pushstring(st->l_cfg, str_name);
    lua_gettable(st->l_cfg, -2);
    const char *str = lua_tostring(st->l_cfg, -1);
    trace("load_string: str %s\n", str);
    if (str) 
        strncpy(str_dest, str, max_dest_sz);
    else {
        /*
        if (str_default) {
            strncpy(str_dest, str_default, max_dest_sz);
        } else {
            trace("load_string: str == NULL, resulting zero string\n");
            memset(str_dest, 0, max_dest_sz);
        }
        */
        trace("load_string: str == NULL, resulting zero string\n");
    }
    lua_pop(st->l_cfg, 1);
    trace("load_string: [%s]\n", L_stack_dump(st->l_cfg));
}

static void patterns_load(Stage_SpriteLoader *st) {
    assert(st);
    trace("patterns_load:\n");

    if (luaL_dofile(st->l_cfg, cfg_fname) != LUA_OK) {
        trace(
            "patterns_load: could not load config with '%s'\n",
            lua_tostring(st->l_cfg, -1)
        );
        lua_pop(st->l_cfg, 1);
        return;
    }

    lua_pushvalue(st->l_cfg, -1);
    st->ref_cfg_tbl = luaL_ref(st->l_cfg, LUA_REGISTRYINDEX);
    trace("patterns_load: [%s]\n", L_stack_dump(st->l_cfg));

    load_string(
        st, "regex_pattern_exclude_images",
        regex_pattern_exclude_images, sizeof(regex_pattern_exclude_images) - 1
    );
    load_string(
        st, "regex_pattern_images", 
        regex_pattern_images, sizeof(regex_pattern_images) - 1
    );
    load_string(
        st, "regex_pattern_exclude_ase_exported", 
        regex_pattern_exclude_ase_exported,
        sizeof(regex_pattern_exclude_ase_exported) - 1
    );

    lua_settop(st->l_cfg, 0);
    trace("patterns_load: [%s]\n", L_stack_dump(st->l_cfg));
}

static void patterns_save(Stage_SpriteLoader *st) {
    assert(st);
    trace("patterns_save:\n");

    assert(st->l_cfg);
    lua_State *l = st->l_cfg;
    lua_settop(l, 0); // На всякий случай

    if (luaL_loadfile(l, cfg_fname) != LUA_OK) {
        trace(
            "patterns_save: could not load file '%s' with '%s'\n",
            cfg_fname, lua_tostring(l, -1)
        );
        goto _cleanup;
    }
    lua_call(l, 0, LUA_MULTRET);
    if (lua_type(l, lua_gettop(l)) != LUA_TTABLE) {
        trace(
            "patterns_save: could not call code with '%s'\n",
            lua_tostring(l, -1)
        );
        goto _cleanup;
    }

    lua_pushstring(l, "regex_pattern_exclude_images");
    lua_pushstring(l, regex_pattern_exclude_images);
    lua_settable(l, -3);

    lua_pushstring(l, "regex_pattern_images");
    lua_pushstring(l, regex_pattern_images);
    lua_settable(l, -3);

    lua_pushstring(l, "regex_pattern_exclude_ase_exported");
    lua_pushstring(l, regex_pattern_exclude_ase_exported);
    lua_settable(l, -3);

    char *dump_str = L_table_serpent_alloc(l, NULL);
    if (dump_str) {
        FILE *file = fopen(cfg_fname, "w");
        if (file) {
            fwrite(dump_str, strlen(dump_str), 1, file);
            fclose(file);
        } else {
            trace(
                "patterns_save: could not open file with %s\n",
                strerror(errno)
            );
        }
        free(dump_str);
    } else
        trace("patterns_save: not dumped\n");

_cleanup:
    lua_settop(st->l_cfg, 0);
}

static void search_meta_shutdown(Stage_SpriteLoader *st) {
    assert(st);
    if (st->meta_selected) {
        free(st->meta_selected);
        st->meta_selected = NULL;
    }
    koh_search_files_shutdown(&st->fsr_meta);
}

static void textures_unload_images(Stage_SpriteLoader *st) {
    assert(st);

    if (st->textures) {
        SetTraceLogLevel(LOG_WARNING);
        for (int i = 0; i < st->textures_num; i++) {
            if (st->textures[i].id)
                UnloadTexture(st->textures[i]);
        }
        SetTraceLogLevel(LOG_ALL);
        free(st->textures);
        st->textures_num = 0;
        st->textures = NULL;
    }

    if (st->rt_textures) {
        SetTraceLogLevel(LOG_WARNING);
        for (int i = 0; i < st->textures_num; i++) {
            if (st->rt_textures[i].id)
                UnloadRenderTexture(st->rt_textures[i]);
        }
        SetTraceLogLevel(LOG_ALL);
        free(st->rt_textures);
        st->rt_textures = NULL;
    }
}

static void textures_unload_ase_exported(Stage_SpriteLoader *st) {
    assert(st);
    if (!st->textures_ase_exported) {
        return;
    }

    for (int i = 0; i < st->textures_ase_exported_num; i++) {
        layered_sprite_shutdown(&st->textures_ase_exported[i]);
    }
    free(st->textures_ase_exported);
    st->textures_ase_exported_num = 0;
    st->textures_ase_exported = NULL;
}

static void textures_load_images(Stage_SpriteLoader *st) {
    assert(st);

    textures_unload_images(st);

    const int num = st->fsr_images.num;

    size_t sz = num * sizeof(st->textures[0]),
           sz_rt = num * sizeof(st->rt_textures[0]);
    if (!st->textures && st->fsr_images.num) {
        st->textures = calloc(1, sz);
        st->rt_textures = calloc(1, sz_rt);
    }

    //assert(st->textures_num >= st->fsr_images.num);
    
    if (st->textures) {
        memset(st->textures, 0, sz);
        memset(st->rt_textures, 0, sz_rt);

        SetTraceLogLevel(LOG_WARNING);
        trace(
            "textures_load_images: fsr_images->num %d\n",
            st->fsr_images.num
        );
        for (int j = 0; j < st->fsr_images.num; ++j) {
            //trace("textures_load_images: '%s'\n", st->fsr_images.names[j]);

            // XXX: Нормально, если текстура не будет загружена?
            // Что будет отображаться?
            if (koh_is_fname_image_ext(st->fsr_images.names[j])) {
                st->textures[j] = LoadTexture(st->fsr_images.names[j]);
                st->rt_textures[j] = LoadRenderTexture(
                    tbl_tex_size, tbl_tex_size
                );

                /*
                    TODO:
                        Создать отдельный поток
                        Передать в поток массив имен для загрузки
                        В потоке последовательно загружать картинки

                        Понадобяться-ли атомарные переменные?

                        В основном потоке цикл ожидания
                        Если есть готовая картинка, то загрузить её в текстуру
                        Когда картинки закончаться пропустить цикл ожидания,
                        установить флаг конца загрузки
                 */

                /*LoadImage(st->fsr_images.names[j]);*/
            }
        }
        SetTraceLogLevel(LOG_ALL);
    }

    st->textures_num = st->fsr_images.num;
}

static void textures_load_ase_exported(Stage_SpriteLoader *st) {
    assert(st);
    trace("textures_load_ase_exported:\n");

    textures_unload_ase_exported(st);

    if (!st->textures_ase_exported && st->fsr_ase_exported.num) {
        st->textures_ase_exported = calloc(
            st->fsr_ase_exported.num, sizeof(st->textures_ase_exported[0])
        );
    }

    //assert(st->textures_ase_exported_num >= st->fsr_ase_exported.num);

    if (st->textures_ase_exported) {
        //SetTraceLogLevel(LOG_WARNING);
        const struct FilesSearchResult *fsr = &st->fsr_ase_exported;
        for (int i = 0; i < fsr->num; ++i) {
            layered_sprite_init(
                &st->textures_ase_exported[i], st->l_cfg,
                st->ref_ase_exported_tbl, fsr->names[i]
            );
        }
        //SetTraceLogLevel(LOG_ALL);
    }

    st->textures_ase_exported_num = st->fsr_ase_exported.num;
}

static void search_ase_exported_shutdown(Stage_SpriteLoader *st) {
    koh_search_files_shutdown(&st->fsr_ase_exported);
    if (st->ase_exported_selected) {
        free(st->ase_exported_selected);
        st->ase_exported_selected = NULL;
    }
}

static void stage_sprite_loader_shutdown(struct Stage *s) {
    Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;

    cam_auto_shutdown(&st->cam_automat);
    visual_tool_shutdown(&st->tool_visual);

    /*
    metaloader_write(st->metaloader);
    if (st->metaloader) {
        metaloader_free(st->metaloader);
        st->metaloader = NULL;
    }
    */

    patterns_save(st);
    search_images_shutdown(st);
    search_meta_shutdown(st);
    search_ase_exported_shutdown(st);

    if (st->l_cfg) {
        lua_close(st->l_cfg);
        st->l_cfg = NULL;
    }
    gui_color_combo_shutdown(&st->grid.line_color_combo);
    gui_color_combo_shutdown(&st->tool_color_combo);
    gui_combo_shutdown(&st->toolmode_combo);
    textures_unload_images(st);
    textures_unload_ase_exported(st);
}

static void stage_sprite_loader_enter(Stage_SpriteLoader *st) {
    trace("stage_sprite_loader_enter:\n");

    search_images(st, regex_pattern_images, regex_pattern_exclude_images);
    search_ase_exported(
        st, regex_pattern_images, regex_pattern_exclude_images
    );
    // Сцена работает только один раз
    st->parent.enter = NULL;
}

static void stage_sprite_loader_leave(struct Stage *s) {
    trace("stage_sprite_loader_leave:\n");
}

static void meta_search_files(Stage_SpriteLoader *st) {
    assert(st);
    koh_search_files_shutdown(&st->fsr_meta);
    st->fsr_meta = koh_search_files(&(struct FilesSearchSetup) {
        .path = path_meta, 
        .regex_pattern = path_meta_pattern,
        .deep = -1,
    });
    if (st->fsr_meta.num) {
        if (st->meta_selected) {
            free(st->meta_selected);
            st->meta_selected = NULL;
        }
        st->meta_selected = calloc(
            st->fsr_meta.num, sizeof(st->meta_selected[0])
        );
        assert(st->meta_selected);
    }
}

static void toolmode_init(Stage_SpriteLoader *st) {
    const char* labels[4] = {
        "rectangle",
        "rectangle oriented",
        "sector",
        "polyline",
    };
    int labels_num = sizeof(labels) / sizeof(labels[0]);
    const enum MetaLoaderType items[] = {
        MLT_RECTANGLE,
        MLT_RECTANGLE_ORIENTED,
        MLT_SECTOR,
        MLT_POLYLINE,
    };
    int items_num = sizeof(items) / sizeof(items[0]);
    assert(labels_num == items_num);

    st->toolmode_combo = gui_combo_init(
        (const char**)labels, (const void*)items,
        sizeof(items[0]), items_num
    );
    st->toolmode_combo.on_change = toolmode_combo_changed;
    st->toolmode_combo.label = "tool mode";

    //st->toolmode_combo.selected[1] = true; // rectangle

    // XXX: Загрузка не работает из-за table_dump2allocated_str()
    //gui_combo_load(&st->toolmode_combo, cfg_fname);

    //st->toolmode = MLT_RECTANGLE;
    st->tool_visual.mode = VIS_TOOL_RECTANGLE;
}

static void color_combos_init(Stage_SpriteLoader *st) {
    st->grid.line_color_combo = gui_color_combo_init("line color");
    gui_color_combo_load(&st->grid.line_color_combo, cfg_fname);
    st->grid.line_color = st->grid.line_color_combo.color;

    st->tool_color_combo = gui_color_combo_init("tool line color");
    gui_color_combo_load(&st->tool_color_combo, cfg_fname);
    //struct ToolCommonOpts *common = &st->tool_rect_opts.common;
    struct ToolCommonOpts *common = &st->tool_visual.t_rect_opts.common;
    common->line_color = st->tool_color_combo.color;
}

static void stage_sprite_loader_init(struct Stage *s) {
    Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;
    trace("stage_sprite_loader_init:\n");

    visual_tool_init(&st->tool_visual);

    st->grid.step = 1;
    st->grid.visible = false;

    /*st->meta_loaded_index = -1;*/

    st->cam.zoom = 1.;

    /*
    st->metaloader = metaloader_new(&(struct MetaLoaderSetup) {
        .verbose = true,
    });
    */

    st->l_cfg = luaL_newstate();
    luaL_openlibs(st->l_cfg);

    patterns_load(st);

    meta_search_files(st);
    color_combos_init(st);
    toolmode_init(st);

    /*
    if (!luaL_dostring(sc_get_state(), "types.fpsmeter_stat()")) {
        trace("stage_sprite_loader_init: could call script stuff\n");
    }
    */

    cam_auto_init(&st->cam_automat, &st->cam);
}

static int ase_export_version(lua_State *l) {
    const char *field_name = "caustic_aseprite_export_ver";
    lua_pushstring(l, field_name);
    lua_gettable(l, -2);
    if (lua_type(l, -1) != LUA_TNUMBER) {
        trace(
            "_ase_exported_load_lua_code: could not get %s field\n",
            field_name
         );
        return -1;
    }

    int version = lua_tonumber(l, -1);
    const int version_min = 1, version_max = 2;
    if (version < 1) {
        trace(
            "check_ase_version: caustic_aseprite_export_ver == %d"
            ", but supported %d-%d\n",
            version, version_min, version_max
        );
        return -1;
    }
    lua_pop(l, 1);
    return version;
}

static bool _ase_exported_load_lua_code(
    lua_State *l, const char *ase_exported_fname, int i
) {
    trace(
        "_ase_exported_load_lua_code: ase_exported_fname %s\n",
        ase_exported_fname
     );
    if (luaL_loadfile(l, ase_exported_fname) != LUA_OK) {
        trace(
            "_ase_exported_load_lua_code: "
            "could not load file '%s' with '%s'\n",
            ase_exported_fname, lua_tostring(l, -1)
        );
        return false;
    }
    lua_call(l, 0, LUA_MULTRET);
    if (lua_type(l, lua_gettop(l)) != LUA_TTABLE) {
        trace(
            "_ase_exported_load_lua_code: could not call code with '%s'\n",
            lua_tostring(l, -1)
        );
        return false;
    }

    trace("ase_exported_load_lua_code: [%s]\n", L_stack_dump(l));
    
    int export_version = ase_export_version(l);
    if (export_version == -1)
        return false;

    trace(
        "_ase_exported_load_lua_code: after check_version [%s]\n",
        L_stack_dump(l)
    );

    lua_pushstring(l, ase_exported_fname);
    lua_pushvalue(l, -2);
    lua_settable(l, -4); // Почему -4 ??

    trace(
        "_ase_exported_load_lua_code: after settable [%s]\n",
        L_stack_dump(l)
    );

    lua_pop(l, 1);

    return true;
}

static void ase_exported_load_lua_code(Stage_SpriteLoader *st) {
    assert(st);
    assert(st->l_cfg);
    trace("ase_exported_load_lua_code:\n");
    lua_State *l = st->l_cfg;

    /*trace("ase_exported_load_lua_code: [%s]\n", stack_dump(l));*/
    lua_createtable(l, 0, 0);
    /*trace("ase_exported_load_lua_code: [%s]\n", stack_dump(l));*/
    lua_pushvalue(l, -1);
    st->ref_ase_exported_tbl = luaL_ref(l, LUA_REGISTRYINDEX);
    /*trace("ase_exported_load_lua_code: [%s]\n", stack_dump(l));*/

    const struct FilesSearchResult *fsr = &st->fsr_ase_exported;
    for (int i = 0; i < fsr->num; ++i) {
        if (!_ase_exported_load_lua_code(l,fsr->names[i], i))
            goto _cleanup;
    }

_cleanup:
    trace("ase_exported_load_lua_code: [%s]\n", L_stack_dump(l));
    lua_settop(l, 0);
}

static void search_ase_exported(
    Stage_SpriteLoader *st, 
    const char *regex_pattern_file,
    const char *regex_pattern_exclude
) {
    assert(st);
    assert(regex_pattern_file);
    assert(regex_pattern_exclude);

    koh_search_files_shutdown(&st->fsr_ase_exported);
    st->fsr_ase_exported = koh_search_files(&(struct FilesSearchSetup) {
        .path = path_assets,
        .regex_pattern = regex_pattern_ase_exported,
        .deep = -1
    });
    koh_search_files_exclude_pcre(&st->fsr_ase_exported, regex_pattern_exclude);

    if (st->fsr_ase_exported.num)
        st->ase_exported_selected = calloc(
            st->fsr_ase_exported.num, sizeof(st->ase_exported_selected[0])
        );

    ase_exported_load_lua_code(st);

    lua_State *l = st->l_cfg;
    lua_settop(l, 0);
    lua_rawgeti(l, LUA_REGISTRYINDEX, st->ref_ase_exported_tbl);
    char *dump_str = L_table_serpent_alloc(l, NULL);
    if (dump_str) {
        trace("search_ase_exported: dump_str %s\n", dump_str);
        free(dump_str);
    }

    textures_load_ase_exported(st);
}

static void search_images(
    Stage_SpriteLoader *st, 
    const char *regex_pattern_file,
    const char *regex_pattern_exclude
) {
    assert(st);
    assert(regex_pattern_file);
    assert(regex_pattern_exclude);

    koh_search_files_shutdown(&st->fsr_images);
    st->fsr_images = koh_search_files(&(struct FilesSearchSetup) {
        .path = path_assets,
        .regex_pattern = regex_pattern_file,
        .deep = -1
    });
    /*koh_search_files_exclude_pcre(&st->fsr_images, regex_pattern_exclude);*/

    if (st->fsr_images.num)
        st->images_selected = calloc(
            st->fsr_images.num, sizeof(st->images_selected[0])
        );

    textures_load_images(st);
}

static void active_sprite_load(
    Stage_SpriteLoader *st, const RenderTexture2D *tex_src
) {
    //trace("sprite_load_ase_export: %s\n", fname);
    if (st->active_sprite.id)
        UnloadRenderTexture(st->active_sprite);

    //RenderTexture2D *tex_rt_aux = &st->textures_ase_exported[i].tex_combined;
    const RenderTexture2D *tex_rt_aux = tex_src;
    RenderTexture2D tex_rt_active = LoadRenderTexture(
        tex_rt_aux->texture.width, tex_rt_aux->texture.height
    );
    BeginMode2D((Camera2D) {
        .zoom = 1.,
    });
    BeginTextureMode(tex_rt_active);
    ClearBackground(BLANK);
    Rectangle dst = {
        0., 0., tex_rt_aux->texture.width, tex_rt_aux->texture.height 
    };
    Rectangle src = {
        0., 0., tex_rt_aux->texture.width, -tex_rt_aux->texture.height 
    };
    DrawTexturePro(tex_rt_aux->texture, src, dst, Vector2Zero(), 0., WHITE);
    EndMode2D();
    EndTextureMode();

    st->active_sprite = tex_rt_active;
}

static void sprite_load_image(Stage_SpriteLoader *st, const char *fname) {
    trace("sprite_load_image: %s\n", fname);
    if (st->active_sprite.id)
        UnloadRenderTexture(st->active_sprite);

    Texture tex_aux = LoadTexture(fname);
    RenderTexture2D tex_rt_active = LoadRenderTexture(
        tex_aux.width, tex_aux.height
    );
    BeginMode2D((Camera2D) {
        .zoom = 1.,
    });
    BeginTextureMode(tex_rt_active);
    ClearBackground(BLANK);
    Rectangle dst = {
        0., 0., tex_aux.width, tex_aux.height 
        /*0, 0, 400, 400*/
    };
    Rectangle src = {
        0., 0., tex_aux.width, -tex_aux.height 
    };
    DrawTexturePro(tex_aux, src, dst, Vector2Zero(), 0, WHITE);
    EndMode2D();
    EndTextureMode();
    UnloadTexture(tex_aux);

    st->active_sprite = tex_rt_active;
}

/*
static void check_selected(Stage_SpriteLoader *st) {
    for (int i = 0; i < st->fsr_meta.num; i++) {
        const char *name = st->fsr_meta.names[i];
        const ImVec2 zero = {};
        if (igSelectable_BoolPtr(name, &st->meta_selected[i], 0, zero)) {
            for (int j = 0; j < st->fsr_meta.num; j++) {
                if (j != i)
                    st->meta_selected[j] = false;
            }
        }
    }
}
*/

/*
static void gui_meta_listbox(Stage_SpriteLoader *st) {
    assert(st);
    // Список выбираемых файлов
    //trace("gui_meta_listbox: st->meta_selected '%p'\n", st->meta_selected);
    if (igBeginListBox("meta files", (ImVec2){})) {
        if (st->meta_selected) {
            check_selected(st);
        }
        igEndListBox();
    }
}
*/

static void toolmode_combo(Stage_SpriteLoader *st) {
    assert(st);
    bool mode_changed = false;
    const char *prev_mode = visual_mode2str(st->tool_visual.mode);
    const enum MetaLoaderType* value = gui_combo(
            &st->toolmode_combo, &mode_changed
    );
    //enum MetaLoaderType prev_value = st->toolmode;
    if (value)
        st->tool_visual.mode = visual_tool_mode2metaloader_type(*value);
    if (mode_changed) {
        trace(
            "toolmode_combo: changed value from %s to %s\n",
            prev_mode,
            visual_mode2str(st->tool_visual.mode)
        );

        // XXX: Загрузка не работает из-за table_dump2allocated_str()
        //gui_combo_save(&st->toolmode_combo, cfg_fname);
    }
}

static void color_combos(Stage_SpriteLoader *st) {
    assert(st);
    bool color_changed;

    color_changed = false;
    st->grid.line_color = gui_color_combo(
        &st->grid.line_color_combo, &color_changed
    );
    if (color_changed)
        gui_color_combo_save(&st->grid.line_color_combo, cfg_fname);

    color_changed = false;
    st->tool_visual.t_rect_opts.common.line_color = gui_color_combo(
        &st->tool_color_combo, &color_changed
    );
    if (color_changed)
        gui_color_combo_save(&st->tool_color_combo, cfg_fname);
}

// XXX: Что делает функция?
static int gui_subtree(Stage_SpriteLoader *st, const char *fname_noext) {
    ImGuiTreeNodeFlags node_flags = 0;

    /*
    struct MetaLoaderObjects2 objects = metaloader_objects_get2(
        st->metaloader, fname_noext
    );
    */
    struct MetaLoaderObjects2 objects = {};
    assert(objects.objs);

    for (int j = 0; j < objects.num; j++) {
        char *str_repr = NULL;
        bool str_repr_allocated = false;

        if (!objects.objs[j])
            continue;

        switch (objects.objs[j]->type) {
            case MLT_RECTANGLE: {
                struct MetaLoaderRectangle *obj_rect;
                obj_rect = (struct MetaLoaderRectangle*)objects.objs[j];
                str_repr = (char*)rect2str(obj_rect->rect);
                break;
            }
            case MLT_RECTANGLE_ORIENTED:
                break;
            case MLT_POLYLINE: {
                struct MetaLoaderPolyline *obj_pl;
                obj_pl = (struct MetaLoaderPolyline*)objects.objs[j];
                str_repr = points2table_allocated(obj_pl->points, obj_pl->num);
                str_repr_allocated = true;
                break;
            }
            case MLT_SECTOR:
                break;
        }

        bool node_open = igTreeNodeEx_Ptr(
            (void*)(uintptr_t)j, node_flags, "%s - %s", 
            objects.names[j], str_repr
        );

        if (igIsItemClicked(ImGuiMouseButton_Left)) {
            trace(
                "gui_subtree: item '%s' clicked\n",
                objects.names[j]
            );
        }

        if (node_open) {
            static char buf[128] = {};
            strncpy(buf, objects.names[j], sizeof(buf) - 1);
            
            ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_CharsNoBlank;
            if (igInputTextEx(
                "object name", NULL, 
                buf, sizeof(buf),
                (ImVec2) {250., 0.}, input_flags,
                NULL, NULL)) {

                // TODO: Если строка пустая?
                //Rectangle rect = { 0., 0., 11., 11., };
                //metaloader_set(st->metaloader, rect, fname_noext, buf);

                /*
                metaloader_set_rename_fmt(
                    st->metaloader, fname_noext, objects.names[j], "%s", buf
                );
                */

            }

            igSameLine(0., 5.);
            if (igButton("from selected", (ImVec2){}) && 
                st->tool_visual.t_recta.exist) {

                /*
                Rectangle new_rect = st->tool_visual.t_recta.rect;
                metaloader_set(
                    st->metaloader, new_rect, fname_noext, objects.names[j]
                );
                */

            }
            igSameLine(0., 5.);
            if (igButton("to selected", (ImVec2){})) {

                /*
                const Rectangle *new_selection = metaloader_get_fmt(
                    st->metaloader, fname_noext, "%s", objects.names[j]
                );
                */
                const Rectangle *new_selection = NULL;

                if (new_selection) {
                    st->tool_visual.t_recta.exist = true;
                    st->tool_visual.t_recta.rect = *new_selection;
                }
            }

            // igPushID_Int() для создания уникальных идентификаторов "REMOVE"
            igPushID_Int(j);
            ImVec4 color = {
                1., 0., 0., 1.
            };
            igPushStyleColor_Vec4(ImGuiCol_Button, color);
            igSameLine(0., 5.);
            if (igButton("REMOVE", (ImVec2){})) {
                
                /*
                metaloader_remove_fmt(
                    st->metaloader, fname_noext, "%s", objects.names[j]
                );
                */

            }
            igPopStyleColor(1);
            igPopID();

            igTreePop();
        }

        if (str_repr_allocated)
            free(str_repr);
    }

    int objects_num = objects.num;
    metaloader_objects_shutdown2(&objects);
    return objects_num;
}

static void paste_stuff2lua(
    Stage_SpriteLoader *st, char *object_name, const char *fname_noext
) {
    /*
    const char *mode_str = visual_mode2str(st->tool_visual.mode);
    trace("paste_stuff2lua: mode %s\n", mode_str);
    // */

    switch (st->tool_visual.mode) {
        case VIS_TOOL_RECTANGLE: {
            if (st->tool_visual.t_recta.exist) {
                /*trace("paste_stuff2lua: write rectangle\n");*/

                /*
                Rectangle new_rect = st->tool_visual.t_recta.rect;
                metaloader_set(
                    st->metaloader, new_rect, fname_noext, object_name
                );
                */

                memset(object_name, 0, sizeof(object_name) - 1);
            }
            break;
        }
        case VIS_TOOL_SECTOR:
            trace("paste_stuff2lua: write sector\n");
            break;
        case VIS_TOOL_POLYLINE:
            if (st->tool_visual.t_pl.exist) {

                /*trace("paste_stuff2lua: write polyline\n");*/

                /*
                struct ToolPolyline *pl = &st->tool_visual.t_pl;
                for (int i = 0; i < pl->points_num; i++) {
                    trace(
                        "paste_stuff2lua: {%f, %f}\n",
                        pl->points[i].x,
                        pl->points[i].y
                    );
                }
                // */

                /*
                metaloader_set_fmt2_polyline(
                    st->metaloader, 
                    pl->points, pl->points_num,
                    fname_noext, object_name
                );
                */

            }
        break;
        case VIS_TOOL_RECTANGLE_ORIENTED:
            trace("paste_stuff2lua: write oriented rectangle\n");
            break;
        default:
            trace("paste_stuff2lua: unsupported st->tool_visual.mode\n");
    }

}

static void gui_meta_tree(Stage_SpriteLoader *st) {
    assert(st);
    
    /*struct MetaLoaderFilesList filelist = metaloader_fileslist(st->metaloader);*/
    struct MetaLoaderFilesList filelist = {};

    for (int i = 0; i < filelist.num; ++i) {
        const char *fname_noext = filelist.fnames[i];
        assert(fname_noext);

        ImGuiTreeNodeFlags tree_flags = 0;

        if (igTreeNodeEx_Ptr(
                (void*)(uintptr_t)i,
                tree_flags, "%s", 
                fname_noext
        )) {

            {
                static char object_name[64] = {};
                ImGuiInputTextFlags input_flags = 0;
                igInputText(
                    "object name",
                    object_name, sizeof(object_name) - 1,
                    input_flags, 
                    NULL, NULL
                );

                igSameLine(0., 5.);

                // TODO: Добавить сюда полилинию
                const char *btn_label = "new from selected";
                if (igButton(btn_label, (ImVec2){})) {
                    if (strlen(object_name) > 0)
                        paste_stuff2lua(st, object_name, fname_noext);

                }

                gui_subtree(st, fname_noext) ;
            }
            igTreePop();
        }
    }    
    metaloader_fileslist_shutdown(&filelist);
}

static void gui_meta_ribbonframe_opts(Stage_SpriteLoader *st) {
    if (igSliderInt("grid step", &st->grid.step, 2, 32, "%d", 0)) {
        st->tool_visual.t_recta_opts.common.mouse_button_bind = -1;
        st->tool_visual.t_recta_opts.snap_size = st->grid.step;
        rectanglea_update_opts(
            &st->tool_visual.t_recta, &st->tool_visual.t_recta_opts
        );
    }

    if (st->grid.step == 1)
        st->grid.visible = false;
    igCheckbox("draw grid", &st->grid.visible);
    static bool snap_selection = false;
    igCheckbox("snap selection", &snap_selection);

    igCheckbox(
        "rectangle axises", &st->tool_visual.t_rect_draw_opts.draw_axises
    );

    st->tool_visual.t_recta_opts.common.mouse_button_bind = -1;

    st->tool_visual.t_recta_opts.snap_size = st->grid.step;
    st->tool_visual.t_recta_opts.snap = snap_selection;
    rectanglea_update_opts(
        &st->tool_visual.t_recta, &st->tool_visual.t_recta_opts
    );

    st->tool_visual.t_pl_opts.common.snap_size = st->grid.step;
    st->tool_visual.t_pl_opts.common.snap = snap_selection;
    polyline_update_opts(
        &st->tool_visual.t_pl, &st->tool_visual.t_pl_opts
    );
}

#ifdef USE_C2X_HACKS
// This string functions get from MUSL clibrary
size_t strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? p-s : n;
}

size_t strlcpy(char *d, const char *s, size_t n)
{
	const char *d0 = d;
	if (!n--) goto finish;
	for (; n && (*d=*s); n--, s++, d++);
	*d = 0;
finish:
	return d-d0 + strlen(s);
}

size_t strlcat(char *d, const char *s, size_t n)
{
	size_t l = strnlen(d, n);
	if (l == n) return l + strlen(s);
	return l + strlcpy(d+l, s, n-l);
}
#endif

static void gui_meta_button_new_meta_file(
    Stage_SpriteLoader *st,  char *meta_fname_new
) {
    if (igButton("new meta file", (ImVec2){})) {
        char fname_full[256] = {};
        strlcat(fname_full, path_meta, sizeof(fname_full) - 1);
        strlcat(fname_full, "/", sizeof(fname_full) - 1);
        strlcat(fname_full, meta_fname_new, sizeof(fname_full) - 1);
        trace("gui_meta: saving '%s'\n", fname_full);
        FILE *newfile = fopen(fname_full, "w");
        if (newfile) {
            const char *luacode =
                "return {\n"
                " g = {0,0,1,1,}\n"
                "}\n";
            fprintf(newfile, "%s", luacode);
            if (fclose(newfile)) {
                trace(
                    "gui_meta: could not close file '%s' with %s\n",
                    meta_fname_new,
                    strerror(errno)
                );
            } else
                meta_search_files(st);
        }

        //if (!metaloader_load_s(st->metaloader, meta_fname_new, luacode)) {
            //trace(
                //"gui_meta: failed to metaloader_load_s() "
                //"with pseudo-fname '%s'\n",
                //meta_fname_new
            //);
        //}
    }
}

// Предупреждать о несохраненный файлах или делать их резервные копии, которые
// при следующем запуске будут отражены в интерфейсе загрузчика файлов.
static void gui_meta(Stage_SpriteLoader *st) {
    assert(st);

    /*static int copy_id = 0;*/
    bool opened = true;
    ImGuiWindowFlags flags = 0;
    igBegin("meta loader", &opened, flags);

    //gui_meta_listbox(st);

    // TODO: Сделать только один файл загружаемым
    // XXX: Как хранить загружаемый файл?
    // Хранить в виде луа-машины и текста?
    // Делать serpent.dump() в строку текста
    if (igButton("load meta file(s)", (ImVec2){})) {
        if (st->meta_selected) {
            for (int i = 0; i < st->fsr_meta.num; i++) 
                if (st->meta_selected[i]) {

                    /*
                    metaloader_load_f(st->metaloader, st->fsr_meta.names[i]);
                    */

                    /*st->meta_loaded_index = i;*/
                }

            /*metaloader_print_all(st->metaloader);*/
        }
    }

    static char meta_fname_new[32] = {};
    ImGuiInputTextFlags input_flags = 0;
    igInputText(
        "new file name",
        meta_fname_new, sizeof(meta_fname_new) - 1,
        input_flags, NULL, NULL
    );

    gui_meta_button_new_meta_file(st, meta_fname_new);

    igSeparator();

    if (igButton("reset camera", (ImVec2){})) {
        st->cam.offset = Vector2Zero();
        st->cam.zoom = 1.;
    }
    igSameLine(0., 5.);
    if (igButton("reset tools", (ImVec2){})) {
        visual_tool_reset_all(&st->tool_visual);
    }

    gui_meta_ribbonframe_opts(st);

    color_combos(st);
    toolmode_combo(st);
    gui_meta_tree(st);
    if (igButton("save meta file(s)", (ImVec2){})) {
        /*metaloader_write(st->metaloader);*/
    }

    if (st->active_sprite.id) {
        Vector2 sz = { 
            st->active_sprite.texture.width,
            st->active_sprite.texture.height 
        };
        igText("texture size %s\n", Vector2_tostr(sz));
        igSameLine(0., -1.f);
        //char buf[32] = {};
        //snprintf(buf, sizeof(buf) - 1, "copy##%d", copy_id++);
        //if (igSmallButton(buf)) {
        if (igSmallButton("copy##1")) {
            SetClipboardText(Vector2_tostr(sz));
        }
    }

    igText("camera %s", camera2str(st->cam, true));
    igText("mouse position %s", Vector2_tostr(GetMousePosition()));
    // FIXME: Когда происходит сброс редактируемого файла?
    // После сохранения?
    // После выбора и загрузки другого метафайла

    /*
    if (st->meta_loaded_index != -1) {
        igText("file '%s' loaded", st->fsr_meta.names[st->meta_loaded_index]);
    } else
        igText("file not loaded");
    */

    if (st->tool_visual.t_recta.exist) {
        igText("selected region %s", rect2str(st->tool_visual.t_recta.rect));

        igSameLine(0., -1.);

        /*char buf[32] = {};*/
        /*snprintf(buf, sizeof(buf) - 1, "copy##%d", copy_id++);*/
        /*if (igSmallButton(buf)) {*/
        if (igSmallButton("copy##hui")) {
            const char *text = rect2str(st->tool_visual.t_recta.rect);
            trace("gui_meta: text '%s'\n", text);
            SetClipboardText(text);
        }
    }

    // */
    igEnd();
}

static void gui_ase_exported_table(Stage_SpriteLoader *st) {
    assert(st);

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody |
        ImGuiTableFlags_ScrollY;

    ImVec2 outer_size = {0., 0.};

    if (igBeginTable("assets", 3, table_flags, outer_size, 0.)) {

        igTableSetupColumn("file name", 0, 0, 0);
        igTableSetupColumn("preview", 0, 0, 1);
        igTableSetupColumn("version", 0, 0, 2);
        igTableHeadersRow();

        for (int i = 0; i < st->fsr_ase_exported.num; i++) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            igTableSetColumnIndex(0);
            if (igSelectable_BoolPtr(
                st->fsr_ase_exported.names[i], &st->ase_exported_selected[i], 
                ImGuiSelectableFlags_SpanAllColumns, (ImVec2){0, 0}
            )) {
                for (int j = 0; j < st->fsr_ase_exported.num; j++)
                    if (j != i)
                        st->ase_exported_selected[j] = false;

                /*st->last_selected = LAST_SELECTED_ASE_EXPORT;*/

                /*
                trace("gui_ase_exported_table: trash_menu\n");
                //if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    //igOpenPopup_Str("trash_menu", ImGuiPopupFlags_MouseButtonRight);
                    igOpenPopup_Str("trash_menu", 0);
                //}

                if (igBeginPopup("trash_menu", 0)) {
                    trace("gui_ase_exported_table: popup stuff here\n");
                    static bool move2trash = true;
                    if (igSelectable_Bool(
                        "move file to trash", move2trash, 
                        0,
                        (ImVec2){})) {
                        trace(
                            "gui_ase_exported_table: move file to trash\n"
                        );
                    }
                    igEndPopup();
                }
                */

            }

            struct LayeredSprite *ls = &st->textures_ase_exported[i];
            assert(ls);

            igTableSetColumnIndex(1);
            rlImGuiImage(&ls->tex_combined.texture);

            igTableSetColumnIndex(2);
            igText("%d", ls->version);

        }

        igEndTable();
    }
}

static void draw_image(Stage_SpriteLoader *st, int i) {
    Texture2D tex = st->textures[i];

    /*BeginDrawing();*/
    BeginTextureMode(st->rt_textures[i]);
    ClearBackground(BLANK);
    BeginMode2D((Camera2D) { .zoom = 1. });

    float w = tex.width, h = tex.height;
    Rectangle src = {0., 0, w, h},
              dst = {0, 0., w, h};
    bool scaled = false;

    if (w > tbl_tex_size || h > tbl_tex_size) {
        float sx, sy;

        if (w > h) {
            sx = tbl_tex_size;
            sy = h * tbl_tex_size / w;
        } else {
            sx = w * tbl_tex_size / h;
            sy = tbl_tex_size;
        }

        dst.width = sx, dst.height = sy;
        scaled = true;
    }

    DrawTexturePro(tex, src, dst, Vector2Zero(), 0.f, WHITE);
    if (scaled) {
        DrawText("SCALED", 0, 0, font_size_scaled, RED);
    }

    EndMode2D();
    EndTextureMode();
    /*EndDrawing();*/


    /*rlImGuiImage(&st->textures[i]);*/
    /*rlImGuiImage(&st->rt_caption.texture);*/
    /*rlImGuiImageRenderTexture(&st->rt_caption);*/

    rlImGuiImageRenderTexture(&st->rt_textures[i]);

    /*rlImGuiImage(&st->rt_textures[i].texture);*/
}

static void gui_images_table(Stage_SpriteLoader *st) {
    assert(st);

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody |
        ImGuiTableFlags_ScrollY;

    ImVec2 outer_size = {0., 0.};

    if (igBeginTable("assets", 2, table_flags, outer_size, 0.)) {

        igTableSetupColumn("file name", 0, 0, 0);
        igTableSetupColumn("preview", 0, 0, 1);
        igTableHeadersRow();

        for (int i = 0; i < st->fsr_images.num; i++) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            igTableSetColumnIndex(0);
            if (igSelectable_BoolPtr(
                st->fsr_images.names[i], &st->images_selected[i], 
                ImGuiSelectableFlags_SpanAllColumns, (ImVec2){0, 0}
            )) {
                for (int j = 0; j < st->fsr_images.num; j++)
                    if (j != i)
                        st->images_selected[j] = false;

                /*st->last_selected = LAST_SELECTED_IMAGE;*/

                /*
                trace("gui_images_table: trash_menu\n");
                //if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    //igOpenPopup_Str("trash_menu", ImGuiPopupFlags_MouseButtonRight);
                    igOpenPopup_Str("trash_menu", 0);
                //}

                if (igBeginPopup("trash_menu", 0)) {
                    trace("gui_images_table: popup stuff here\n");
                    static bool move2trash = true;
                    if (igSelectable_Bool(
                        "move file to trash", move2trash, 
                        0,
                        (ImVec2){})) {
                        trace(
                            "gui_images_table: move file to trash\n"
                        );
                    }
                    igEndPopup();
                }
                */

            }

            igTableSetColumnIndex(1);

            draw_image(st, i);
        }

        igEndTable();
    }
}

static void gui_left_loader_group(Stage_SpriteLoader *st) {
    igBeginGroup();
    // TODO: использовать inotifier
    if (igButton("refresh files", (ImVec2){})) {
        search_images(
            st, regex_pattern_images, regex_pattern_exclude_images
        );
        search_ase_exported(
            st, regex_pattern_ase_exported,
            regex_pattern_exclude_ase_exported
        );
    }

    if (igButton("load image", (ImVec2){})) {

        sprite_load_image(st, get_selected_image(st));

        /*
        switch (st->last_selected) {
            case LAST_SELECTED_IMAGE: 
                sprite_load_image(st, get_selected_image(st));
                break;
            case LAST_SELECTED_ASE_EXPORT: {
                int index = -1;
                const char *fname = get_selected_ase_exported(st, &index);
                trace(
                    "gui_left_loader_group: fname '%s', index %d\n", 
                    fname, index
                );
                assert(index != -1);
                struct LayeredSprite *spr = &st->textures_ase_exported[index];
                layered_sprite_bake(spr);
                active_sprite_load(st, &spr->tex_combined);
                break;
            }
            default:
                break;
        }
        */

    }
    igEndGroup();
}

static void gui_image_files(Stage_SpriteLoader *st) {
    if (igInputText(
            "file name regex pattern",
            regex_pattern_images, sizeof(regex_pattern_images), 0, 0, NULL
        )) {
        patterns_save(st);
    }

    if (igInputTextEx(
            "exclude file path regex pattern",
            NULL,
            regex_pattern_exclude_images,
            sizeof(regex_pattern_exclude_images), 
            (ImVec2){700., .0}, 0, NULL, NULL
        )) {
        patterns_save(st);
    }

    igText("items number: %d\n", st->fsr_images.num);
    gui_images_table(st);
}

static void gui_right_loader_group(Stage_SpriteLoader *st) {
    igBeginGroup();
    igSetNextItemOpen(true, ImGuiCond_Once);
    if (igTreeNode_Str("image files")) {
        gui_image_files(st);
        igTreePop();
    } else if (igTreeNode_Str("aseprite lua exported files")) {
        //trace("gui_sprites_loader: place images table here\n");
        gui_ase_exported_table(st);
        igTreePop();
    }
    igEndGroup();
}

static void put_layers_table(
    lua_State *vm, int ref_ase_exported_tbl, const char *selected_fname
) {
    assert(vm);
    assert(ref_ase_exported_tbl);
    assert(selected_fname);
    // [.., ]
    lua_rawgeti(vm, LUA_REGISTRYINDEX, ref_ase_exported_tbl);
    assert(lua_type(vm, -1) == LUA_TTABLE);
    // [.., {ref_ase_exported_tbl}]
    lua_pushstring(vm, selected_fname);
    // [.., {ref_ase_exported_tbl}, selected_fname]
    //trace(
        //"gui_sprite_layers: selected_i %d, [%s]\n",
        //selected_i, stack_dump(vm)
    //);
    lua_gettable(vm, -2);
    assert(lua_type(vm, -1) == LUA_TTABLE && "first");
    // [.., {ref_ase_exported_tbl}, {**.aseprite.lua}]
    lua_pushstring(vm, "layers");
    //trace("gui_sprite_layers: [%s]\n", stack_dump(vm));
    // [.., {ref_ase_exported_tbl}, {**.aseprite.lua}, "layers"]
    lua_gettable(vm, -2);
    assert(lua_type(vm, -1) == LUA_TTABLE && "second");
    // [.., {ref_ase_exported_tbl}, {**.aseprite.lua}, {layers}]
}

static void gui_right_layers_group(Stage_SpriteLoader *st) {
    igBeginGroup();

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody |
        ImGuiTableFlags_ScrollY;
    ImVec2 outer_size = {0., 0.};

    int selected_i = -1;
    const char *selected_fname = get_selected_ase_exported(st, &selected_i);
    if (!selected_fname)
        return;
    //assert(selected_fname);
    assert(selected_i != -1);

    struct LayeredSprite *ls = &st->textures_ase_exported[selected_i];
    lua_State *vm = st->l_cfg;

    if (igBeginTable("layers", 2, table_flags, outer_size, 0.)) {
        put_layers_table(vm, st->ref_ase_exported_tbl, selected_fname);

        igTableSetupColumn("name", 0, 0, 0);
        igTableSetupColumn("visible", 0, 0, 1);
        igTableHeadersRow();

        int i = 0;
        lua_pushnil(vm);
        while (lua_next(vm, -2)) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            igTableSetColumnIndex(0);

            lua_pushstring(vm, "name");
            lua_gettable(vm, -2);
            assert(lua_type(vm, -1) == LUA_TSTRING);
            const char *name = lua_tostring(vm, -1);
            //trace("gui_sprite_layers: name '%s'\n", name);
            lua_pop(vm, 1);

            igText("%s", name);
            int top = lua_gettop(vm);

            assert(top == lua_gettop(vm));

            bool visible = false;
            const char *visible_str = "visible";
            lua_pushstring(vm, visible_str);
            lua_gettable(vm, -2);
            if (lua_isboolean(vm, -1)) {
                visible = lua_toboolean(vm, -1);
                /*
                trace(
                    "gui_sprite_layers: visible %s\n",
                    visible ? "true" : "false"
                );
                */
            } else {
                trace("gui_sprite_layers: visible is not boolean\n");
            }
            lua_pop(vm, 1);
            bool prev_visible = visible;

            igTableSetColumnIndex(1);
            //igCheckbox("visible", &visible);

            // Требуется пояснение - зачем нужен igPushID_Str()?
            igPushID_Str(name);
            if (igCheckbox("visible", &visible)) {
                trace(
                    "gui_sprite_layers: name %s, visible %s\n",
                    name, visible ? "true" : "false"
                );
                //visible = true;
            }
            igPopID();
            // */

            lua_pushstring(vm, visible_str);
            lua_pushboolean(vm, visible);
            lua_settable(vm, -3);
            assert(lua_istable(vm, -1));

            if (prev_visible != visible) {
                trace("gui_right_layers_group: rebaking\n");
                layered_sprite_bake(ls);
                active_sprite_load(st, &ls->tex_combined);
            }

            i++;
            lua_pop(vm, 1);
        }

        igEndTable();
    }

    lua_settop(vm, 0);
    igEndGroup();
}

/*
static void gui_left_layers_group(Stage_SpriteLoader *st) {
    igBeginGroup();
    igText("shift layer");
    if (igSmallButton("up")) {
    }
    if (igSmallButton("down")) {
    }
    igEndGroup();
}
*/

static void gui_sprite_layers(Stage_SpriteLoader *st) {
    trace("gui_sprite_layers:\n");
    assert(st);
    bool opened = true;
    ImGuiWindowFlags flags = 0;
    igBegin("sprite layers", &opened, flags);
    gui_right_layers_group(st);
    //igSameLine(0., 10.);
    //gui_left_layers_group(st);
    igEnd();
}

static void gui_sprites_loader(Stage_SpriteLoader *st) {
    bool opened = true;
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoScrollbar;

    igBegin("sprites loader", &opened, flags);
    gui_left_loader_group(st);
    igSameLine(0., 10.);
    gui_right_loader_group(st);
    igEnd();

    /*
    if (st->last_selected == LAST_SELECTED_ASE_EXPORT) {
        gui_sprite_layers(st);
    }
    */

    gui_sprite_layers(st);
}

// TODO: Переделать в функцию вызываемую в цикле для каждого несохраненного
// элемента
// {{{
// XXX: Почему стал возникать данный диалог?
/*
static void gui_modal_save_meta(struct Stage_SpriteLoader *st) {
    ImGuiWindowFlags popup_flags = ImGuiWindowFlags_AlwaysAutoResize;

    static bool done = false;

    if (!done)
        igOpenPopup_Str("unsaved metafiles", 0);
    
    if (igBeginPopupModal("unsaved metafiles", NULL, popup_flags)) {
        static char fname_buf[128] = {};

        if (!strlen(fname_buf))
            sprintf(fname_buf, "assets/meta/%d.lua", rand() % 2000 + 1);

        ImGuiInputTextFlags input_flags = 0;
        if (igInputText(
            "save path", 
            fname_buf, sizeof(fname_buf) - 1,
            input_flags,
            NULL, NULL
        )) {
            trace(
                "gui_modal: chech path '%s' for accessibility\n",
                fname_buf
            );
        }
        igSameLine(0., 0.);
        if (igButton("save file", (ImVec2){})) {
            trace("gui_modal: save file\n");
            igCloseCurrentPopup();
            done = true;
        }
        igEndPopup();
    } else {
        //trace("gui_modal: no popup\n");
    }
}
// }}}
// */

static void stage_sprite_loader_gui_window(struct Stage *s) {
    assert(s);
    struct Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;
    gui_sprites_loader(st);
    gui_meta(st);
    //gui_modal_save_meta(st);
}

Stage *stage_sprite_loader_new2(HotkeyStorage *hk_store) {
    Stage_SpriteLoader *st = calloc(1, sizeof(Stage_SpriteLoader));
    st->parent.data = hk_store;
    st->parent.init = (Stage_callback)stage_sprite_loader_init;
    st->parent.update = (Stage_callback)stage_sprite_loader_update;
    st->parent.draw = (Stage_callback)stage_sprite_loader_draw;
    st->parent.shutdown = (Stage_callback)stage_sprite_loader_shutdown;
    st->parent.enter = (Stage_callback)stage_sprite_loader_enter;
    st->parent.leave = (Stage_callback)stage_sprite_loader_leave;
    st->parent.gui = stage_sprite_loader_gui_window;
    return (Stage*)st;
}

static void search_images_shutdown(Stage_SpriteLoader *st) {
    assert(st);
    if (st->images_selected) {
        free(st->images_selected);
        st->images_selected = NULL;
    }
    koh_search_files_shutdown(&st->fsr_images);
}

