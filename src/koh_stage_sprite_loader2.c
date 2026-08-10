// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_sprite_loader2.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "koh_common.h"
#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_raylib_api.h"

static raylib_api R = {};
#include "koh_lua.h"
#include "koh_visual_tools.h"
#include "koh_devcanvas.h"
#include "lauxlib.h"
#include "lua.h"
#include "raylib.h"
#include "koh_gui_combo.h"
#include "box2d/box2d.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "koh_layered_sprite.h"
#include "koh_camera.h"

struct Grid {
    struct GuiColorCombo        line_color_combo;
    Color                       line_color;
    bool                        visible;
    int                         step;
};

// Локальная модель формы для редактора геометрии. Не зависит от
// hexia/hm_geom.h — кодоген печатает лишь совместимый текст.
// Координаты хранятся в ЮНИТАХ box2d (не в пикселях).
enum { GEOM_SET_MAX = 64, GEOM_EDIT_MAX_VERTS = 8 };
enum { GEOM_CATEGORIES_MAX = 8, GEOM_CAT_NAME_MAX = 16, GEOM_TAG_MAX = 16 };

enum GeomShapeKind { GK_CIRCLE, GK_POLY, GK_POINT };

typedef struct GeomShape {
    int     kind;                             // enum GeomShapeKind
    Vector2 center_u;                         // GK_CIRCLE
    float   radius_u;                         // GK_CIRCLE
    Vector2 verts_u[GEOM_EDIT_MAX_VERTS];     // GK_POLY (выпуклый)
    int     vcount;                           // GK_POLY
    int     category;                         // индекс в geom_categories, 0 = дефолт
    Vector2 point_u;                          // GK_POINT (позиция якоря)
    char    tag[GEOM_TAG_MAX];                // GK_POINT (имя якоря)
} GeomShape;

typedef struct Stage_SpriteLoader {
    Stage                       parent;

    CameraAutomat               cam_automat;
    CameraProcessor             cam_processor;

    struct VisualTool           tool_visual;

    // Слой векторной разметки поверх спрайта (мировые координаты камеры).
    DevCanvas                   *dc;
    bool                        dc_enabled;

    // Редактор геометрии: набор форм составного тела.
    GeomShape                   geom_set[GEOM_SET_MAX];
    int                         geom_set_num;
    char                        geom_name[64];
    float                       px_per_unit;    // пиксели -> юниты
    Vector2                     geom_origin;    // центр тела в пикселях

    // Имена категорий коллизий (произвольные строки оператора).
    // Индекс имени = значение GeomShape.category, экспортируется в код.
    char                        geom_categories[GEOM_CATEGORIES_MAX][GEOM_CAT_NAME_MAX];
    int                         geom_categories_num;

    struct FilesSearchResult    fsr_images;
    struct FilesSearchResult    fsr_meta;
    struct FilesSearchResult    fsr_ase_exported;

    // Текстовые наборы геометрии (assets/geom/*.geom) для комбобокса.
    struct FilesSearchResult    fsr_geom;
    int                         geom_selected_idx; // индекс в fsr_geom.names, -1 = пусто

    // Массивы, в которых один элемент — выбранный.
    // Соотносятся с переменными fsr_*
    bool                        *images_selected,
                                *meta_selected,
                                *ase_exported_selected;

    Texture                     *textures;
    RenderTexture2D             *rt_textures;
    int                         textures_num;

    struct LayeredSprite        *textures_ase_exported;
    int                         textures_ase_exported_num;
    int                         selected_sprite_layer;

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
// Каталог и паттерн текстовых наборов геометрии редактора (*.geom).
static const char *path_geom = "assets/geom";
static const char *path_geom_pattern = "\\.geom$";
static char regex_pattern_images[128] = ".*\\.png$";
static char regex_pattern_ase_exported[128] = ".*\\.aseprite\\.lua$";
static const char *cfg_fname = "sprite_loader.lua";
static const char *devcanvas_fname = "sprite_loader.dcnv";
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

// --- Редактор геометрии ------------------------------------------------

// Пиксели спрайта -> юниты. Y инвертируется: экран вниз, box2d вверх.
static Vector2 geom_px2unit(Stage_SpriteLoader *st, Vector2 p) {
    float k = st->px_per_unit != 0.f ? st->px_per_unit : 1.f;
    return (Vector2) {
        (p.x - st->geom_origin.x) / k,
        -(p.y - st->geom_origin.y) / k,
    };
}

// Юниты -> пиксели спрайта (для отрисовки набора поверх спрайта).
static Vector2 geom_unit2px(Stage_SpriteLoader *st, Vector2 u) {
    float k = st->px_per_unit != 0.f ? st->px_per_unit : 1.f;
    return (Vector2) {
        st->geom_origin.x + u.x * k,
        st->geom_origin.y - u.y * k,
    };
}

// Добавить форму в набор по результату активного инструмента.
static void geom_add_from_tool(Stage_SpriteLoader *st) {
    if (st->geom_set_num >= GEOM_SET_MAX)
        return;
    struct VisualTool *vt = &st->tool_visual;
    float k = st->px_per_unit != 0.f ? st->px_per_unit : 1.f;
    GeomShape g = {};

    switch (vt->mode) {
    case VIS_TOOL_CIRCLE:
        if (!vt->t_circle.exist)
            return;
        g.kind = GK_CIRCLE;
        g.center_u = geom_px2unit(st, vt->t_circle.center);
        g.radius_u = vt->t_circle.radius / k;
        break;
    case VIS_TOOL_RECTANGLE: {
        if (!vt->t_recta.exist)
            return;
        Rectangle r = vt->t_recta.rect;
        Vector2 corners[4] = {
            { r.x, r.y },
            { r.x + r.width, r.y },
            { r.x + r.width, r.y + r.height },
            { r.x, r.y + r.height },
        };
        g.kind = GK_POLY;
        g.vcount = 4;
        for (int i = 0; i < 4; i++)
            g.verts_u[i] = geom_px2unit(st, corners[i]);
        break;
    }
    case VIS_TOOL_POLYLINE: {
        int n = vt->t_pl.points_num;
        if (!vt->t_pl.points || n < 3)
            return;
        if (n > GEOM_EDIT_MAX_VERTS)
            n = GEOM_EDIT_MAX_VERTS;
        g.kind = GK_POLY;
        g.vcount = n;
        for (int i = 0; i < n; i++)
            g.verts_u[i] = geom_px2unit(st, vt->t_pl.points[i]);
        break;
    }
    default:
        return;
    }

    st->geom_set[st->geom_set_num++] = g;
}

// Печать набора как static const ShapeGeom[] (стиль serialize_arr):
// в stdout и в буфер обмена.
static void geom_codegen(Stage_SpriteLoader *st) {
    char buf[8192];
    int off = 0;
    #define GEOM_APPEND(...) \
        off += snprintf(buf + off, sizeof(buf) - off, __VA_ARGS__)

    // Печать поля .category с комментарием-именем (только если != 0).
    #define GEOM_APPEND_CATEGORY(g) \
        do { \
            if ((g)->category != 0) { \
                const char *_nm = ((g)->category < st->geom_categories_num) \
                    ? st->geom_categories[(g)->category] : "?"; \
                GEOM_APPEND( \
                    " .category = %d, /* \"%s\" */", (g)->category, _nm); \
            } \
        } while (0)

    GEOM_APPEND("static const ShapeGeom %s[] = {\n", st->geom_name);
    for (int i = 0; i < st->geom_set_num && off < (int)sizeof(buf); i++) {
        GeomShape *g = &st->geom_set[i];
        if (g->kind == GK_CIRCLE) {
            GEOM_APPEND(
                "    { .kind = GEOM_CIRCLE, .circle_u = { "
                ".center = {%gf, %gf}, .radius = %gf },",
                g->center_u.x, g->center_u.y, g->radius_u
            );
            GEOM_APPEND_CATEGORY(g);
            GEOM_APPEND(" },\n");
        } else if (g->kind == GK_POINT) {
            GEOM_APPEND(
                "    { .kind = GEOM_POINT, .tag = \"%s\", "
                ".point_u = {%gf, %gf} },\n",
                g->tag, g->point_u.x, g->point_u.y
            );
        } else {
            GEOM_APPEND(
                "    { .kind = GEOM_POLY, .vcount = %d, .verts_u = {\n",
                g->vcount
            );
            for (int j = 0; j < g->vcount; j++)
                GEOM_APPEND(
                    "        {%gf, %gf},\n",
                    g->verts_u[j].x, g->verts_u[j].y
                );
            GEOM_APPEND("    },");
            GEOM_APPEND_CATEGORY(g);
            GEOM_APPEND(" },\n");
        }
    }
    GEOM_APPEND("    { .kind = GEOM_NULL },\n};\n");
    #undef GEOM_APPEND_CATEGORY
    #undef GEOM_APPEND

    printf("%s", buf);
    SetClipboardText(buf);
}

// Отрисовка добавленных форм поверх спрайта. Вызывать внутри
// BeginMode2D(st->cam).
static void geom_set_draw(Stage_SpriteLoader *st) {
    for (int i = 0; i < st->geom_set_num; i++) {
        GeomShape *g = &st->geom_set[i];
        if (g->kind == GK_CIRCLE) {
            Vector2 c = geom_unit2px(st, g->center_u);
            DrawCircleLinesV(c, g->radius_u * st->px_per_unit, GREEN);
        } else if (g->kind == GK_POINT) {
            // Якорь: крестик + подпись тегом.
            Vector2 p = geom_unit2px(st, g->point_u);
            const float r = 6.f;
            DrawLineV((Vector2){ p.x - r, p.y }, (Vector2){ p.x + r, p.y }, YELLOW);
            DrawLineV((Vector2){ p.x, p.y - r }, (Vector2){ p.x, p.y + r }, YELLOW);
            DrawCircleLinesV(p, r, YELLOW);
            DrawText(g->tag, (int)(p.x + r + 2), (int)(p.y - r), 10, YELLOW);
        } else {
            for (int j = 0; j < g->vcount; j++) {
                Vector2 a = geom_unit2px(st, g->verts_u[j]);
                Vector2 b = geom_unit2px(st, g->verts_u[(j + 1) % g->vcount]);
                DrawLineV(a, b, GREEN);
            }
        }
    }
}

// Добавить якорь-точку в набор по позиции курсора (мировые координаты).
static void geom_add_point(Stage_SpriteLoader *st, const char *tag) {
    if (st->geom_set_num >= GEOM_SET_MAX)
        return;
    Vector2 mp_scr = GetMousePosition();
    Vector2 mp_px = GetScreenToWorld2D(mp_scr, st->cam);
    GeomShape g = { .kind = GK_POINT };
    g.point_u = geom_px2unit(st, mp_px);
    strncpy(g.tag, tag, GEOM_TAG_MAX - 1);
    st->geom_set[st->geom_set_num++] = g;
}

// --- Персистентность набора в текстовый файл .geom --------------------

// Сохранить текущий набор геометрии в текстовый файл path.
// Формат строчный (см. grammar в шапке функции загрузки).
static bool geom_save_to_file(Stage_SpriteLoader *st, const char *path) {
    assert(st);
    assert(path);

    if (!DirectoryExists(path_geom)) {
        // MakeDirectory: 0 при успехе.
        if (MakeDirectory(path_geom) != 0) {
            trace("geom_save_to_file: cannot create dir '%s'\n", path_geom);
            return false;
        }
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        trace("geom_save_to_file: fopen '%s': %s\n", path, strerror(errno));
        return false;
    }

    fprintf(f, "# caustic geom set v1\n");
    fprintf(f, "name %s\n", st->geom_name);
    fprintf(f, "px_per_unit %g\n", st->px_per_unit);
    fprintf(f, "origin %.6f %.6f\n", st->geom_origin.x, st->geom_origin.y);

    for (int c = 0; c < st->geom_categories_num; c++)
        fprintf(f, "category %s\n", st->geom_categories[c]);

    for (int i = 0; i < st->geom_set_num; i++) {
        GeomShape *g = &st->geom_set[i];
        switch (g->kind) {
        case GK_CIRCLE:
            fprintf(
                f, "circle %d %.6f %.6f %.6f\n",
                g->category, g->center_u.x, g->center_u.y, g->radius_u
            );
            break;
        case GK_POLY:
            fprintf(f, "poly %d %d", g->category, g->vcount);
            for (int k = 0; k < g->vcount; k++)
                fprintf(f, " %.6f %.6f", g->verts_u[k].x, g->verts_u[k].y);
            fprintf(f, "\n");
            break;
        case GK_POINT:
            fprintf(
                f, "point %.6f %.6f %s\n",
                g->point_u.x, g->point_u.y, g->tag
            );
            break;
        default:
            break;
        }
    }

    fclose(f);
    trace("geom_save_to_file: saved '%s'\n", path);
    return true;
}

// Загрузить набор геометрии из текстового файла path.
// Грамматика (по строке; '#' и пустые — пропуск):
//   name <str> | px_per_unit <f> | origin <fx> <fy>
//   category <name>                     (в порядке индексов, первая = 0)
//   circle <cat> <cx> <cy> <r>
//   poly   <cat> <n> <x0> <y0> ...      (n пар)
//   point  <x> <y> <tag>
// Парсинг во ВРЕМЕННЫЕ буферы; применение к st только при успехе.
static bool geom_load_from_file(Stage_SpriteLoader *st, const char *path) {
    assert(st);
    assert(path);

    char *text = LoadFileText(path);
    if (!text) {
        trace("geom_load_from_file: cannot read '%s'\n", path);
        return false;
    }

    GeomShape set[GEOM_SET_MAX];
    int set_num = 0;
    char cats[GEOM_CATEGORIES_MAX][GEOM_CAT_NAME_MAX];
    int cats_num = 0;
    char name[64] = "";
    float ppu = st->px_per_unit;
    Vector2 origin = { 0, 0 };

    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {

        // Пропуск пустых строк и комментариев.
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0' || *line == '#')
            continue;

        char kw[16] = "";
        if (sscanf(line, "%15s", kw) != 1)
            continue;

        if (!strcmp(kw, "name")) {
            // Остаток строки после ключевого слова.
            const char *rest = line + 4;
            while (*rest == ' ') rest++;
            strncpy(name, rest, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        } else if (!strcmp(kw, "px_per_unit")) {
            sscanf(line, "px_per_unit %f", &ppu);
        } else if (!strcmp(kw, "origin")) {
            sscanf(line, "origin %f %f", &origin.x, &origin.y);
        } else if (!strcmp(kw, "category")) {
            if (cats_num < GEOM_CATEGORIES_MAX) {
                char nm[GEOM_CAT_NAME_MAX] = "";
                if (sscanf(line, "category %15s", nm) == 1) {
                    strncpy(cats[cats_num], nm, GEOM_CAT_NAME_MAX - 1);
                    cats[cats_num][GEOM_CAT_NAME_MAX - 1] = '\0';
                    cats_num++;
                }
            } else
                trace("geom_load_from_file: too many categories\n");
        } else if (!strcmp(kw, "circle")) {
            int cat = 0;
            float cx = 0, cy = 0, r = 0;
            if (sscanf(line, "circle %d %f %f %f", &cat, &cx, &cy, &r) == 4
                && set_num < GEOM_SET_MAX) {
                set[set_num] = (GeomShape){
                    .kind = GK_CIRCLE, .category = cat,
                    .center_u = { cx, cy }, .radius_u = r,
                };
                set_num++;
            } else if (set_num >= GEOM_SET_MAX)
                trace("geom_load_from_file: too many shapes\n");
        } else if (!strcmp(kw, "poly")) {
            int cat = 0, n = 0, off = 0;
            if (sscanf(line, "poly %d %d%n", &cat, &n, &off) == 2
                && set_num < GEOM_SET_MAX) {
                if (n > GEOM_EDIT_MAX_VERTS) {
                    trace("geom_load_from_file: poly clamp %d->%d\n",
                        n, GEOM_EDIT_MAX_VERTS);
                    n = GEOM_EDIT_MAX_VERTS;
                }
                GeomShape g = { .kind = GK_POLY, .category = cat, .vcount = n };
                const char *p = line + off;
                int k = 0;
                for (; k < n; k++) {
                    float x = 0, y = 0;
                    int consumed = 0;
                    if (sscanf(p, " %f %f%n", &x, &y, &consumed) != 2)
                        break;
                    g.verts_u[k] = (Vector2){ x, y };
                    p += consumed;
                }
                if (k == n) {
                    set[set_num++] = g;
                } else
                    trace("geom_load_from_file: poly bad verts\n");
            } else if (set_num >= GEOM_SET_MAX)
                trace("geom_load_from_file: too many shapes\n");
        } else if (!strcmp(kw, "point")) {
            float px = 0, py = 0;
            char tag[GEOM_TAG_MAX] = "";
            if (sscanf(line, "point %f %f %15s", &px, &py, tag) == 3
                && set_num < GEOM_SET_MAX) {
                GeomShape g = { .kind = GK_POINT, .point_u = { px, py } };
                strncpy(g.tag, tag, GEOM_TAG_MAX - 1);
                set[set_num++] = g;
            } else if (set_num >= GEOM_SET_MAX)
                trace("geom_load_from_file: too many shapes\n");
        } else {
            trace("geom_load_from_file: unknown keyword '%s'\n", kw);
        }
    }

    UnloadFileText(text);

    // Гарантировать наличие дефолтной категории.
    if (cats_num == 0) {
        strncpy(cats[0], "default", GEOM_CAT_NAME_MAX - 1);
        cats[0][GEOM_CAT_NAME_MAX - 1] = '\0';
        cats_num = 1;
    }

    // Применение к состоянию редактора (только теперь).
    memcpy(st->geom_set, set, sizeof(GeomShape) * set_num);
    st->geom_set_num = set_num;
    memcpy(st->geom_categories, cats, sizeof(cats[0]) * cats_num);
    st->geom_categories_num = cats_num;
    if (name[0])
        strncpy(st->geom_name, name, sizeof(st->geom_name) - 1);
    if (ppu > 0.f)
        st->px_per_unit = ppu;
    st->geom_origin = origin;

    // Финальный clamp категорий у шейпов.
    for (int i = 0; i < st->geom_set_num; i++)
        if (st->geom_set[i].category >= st->geom_categories_num
            || st->geom_set[i].category < 0)
            st->geom_set[i].category = 0;

    trace("geom_load_from_file: loaded '%s' (%d shapes)\n", path, set_num);
    return true;
}

// Обновить список .geom-файлов для комбобокса.
static void geom_files_search(Stage_SpriteLoader *st) {
    assert(st);
    koh_search_files_shutdown(&st->fsr_geom);
    st->fsr_geom = koh_search_files(&(struct FilesSearchSetup) {
        .path = path_geom,
        .regex_pattern = path_geom_pattern,
        .deep = -1,
    });
    if (st->geom_selected_idx >= st->fsr_geom.num)
        st->geom_selected_idx = st->fsr_geom.num ? 0 : -1;
}

// Показывать превью выпуклой оболочки полилинии (что даст box2d).
static bool show_hull_preview = true;

// Пересчитывает каждый кадр выпуклую оболочку текущей полилинии тем же
// b2ComputeHull, что и hm_geom.c, и рисует её поверх — видно потерю
// вогнутости и переупорядочивание вершин ещё в редакторе.
// Вызывать внутри BeginMode2D(st->cam) (точки — мировые пиксели).
static void polyline_hull_preview_draw(Stage_SpriteLoader *st) {
    assert(st);
    struct VisualTool *vt = &st->tool_visual;
    if (vt->mode != VIS_TOOL_POLYLINE)
        return;
    int n = vt->t_pl.points_num;
    if (!vt->t_pl.points || n < 3)
        return;

    // Копируем точки в b2Vec2 явно (не кастуем массив Vector2).
    if (n > B2_MAX_POLYGON_VERTICES)
        n = B2_MAX_POLYGON_VERTICES;
    b2Vec2 pts[B2_MAX_POLYGON_VERTICES];
    for (int i = 0; i < n; i++)
        pts[i] = (b2Vec2){ vt->t_pl.points[i].x, vt->t_pl.points[i].y };

    b2Hull hull = b2ComputeHull(pts, n);
    if (hull.count < 3)
        return;

    float thick = 2.f / st->cam.zoom;
    for (int j = 0; j < hull.count; j++) {
        b2Vec2 a = hull.points[j];
        b2Vec2 b = hull.points[(j + 1) % hull.count];
        DrawLineEx(
            (Vector2){ a.x, a.y }, (Vector2){ b.x, b.y }, thick, ORANGE
        );
    }
}

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

    // DevCanvas обрабатывает ввод ВНЕ BeginMode2D.
    devcanvas_set_enabled(st->dc, st->dc_enabled);
    devcanvas_update(st->dc);

    if (!st->active_sprite.id)
        return;

    camp_update(&st->cam_processor);
    cam_auto_update(&st->cam_automat);

    // В режиме разметки инструменты выделения отключены, чтобы не
    // конфликтовать за левую кнопку мыши.
    if (!st->dc_enabled)
        visual_tool_update(&st->tool_visual, &st->cam);
}

static void grid_draw(struct Stage_SpriteLoader *st, Rectangle dst) {
    assert(st);
    if (!st->grid.visible)
        return;

    float line_thick = 1.;
    if (st->grid.step == 1)
        line_thick = 0.5;

    for (int x = 0; x < dst.width + st->grid.step; x += st->grid.step) {
        DrawLineEx(
            (Vector2) { dst.x + x, dst.y },
            (Vector2) { dst.x + x, dst.height },
            line_thick, st->grid.line_color
        );
    }
    for (int y = 0; y < dst.height + st->grid.step; y += st->grid.step) {
        DrawLineEx(
            (Vector2) { dst.x, dst.y + y },
            (Vector2) { dst.width, dst.y + y },
            line_thick, st->grid.line_color
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
    DrawTexturePro(*tex, src, dst, origin, 0.f, WHITE);
    float line_thick = 5.;
    line_thick /= st->cam.zoom;
    DrawRectangleLinesEx(dst, line_thick, BLACK);
    grid_draw(st, dst);
}

static void stage_sprite_loader_draw(struct Stage *s) {
    Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;
    ClearBackground(GRAY);
    BeginMode2D(st->cam);
    active_sprite_draw(st);
    geom_set_draw(st);
    visual_tool_draw(&st->tool_visual, &st->cam);
    if (show_hull_preview)
        polyline_hull_preview_draw(st);
    devcanvas_render(st->dc);
    EndMode2D();
}

static void load_string(
    Stage_SpriteLoader *st,
    const char *str_name,
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
    if (str)
        strncpy(str_dest, str, max_dest_sz);
    else
        trace("load_string: str == NULL, resulting zero string\n");
    lua_pop(st->l_cfg, 1);
}

static void patterns_load(Stage_SpriteLoader *st) {
    assert(st);

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
}

static void patterns_save(Stage_SpriteLoader *st) {
    assert(st);
    assert(st->l_cfg);
    lua_State *l = st->l_cfg;
    lua_settop(l, 0);

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
}

static void textures_unload_ase_exported(Stage_SpriteLoader *st) {
    assert(st);
    if (!st->textures_ase_exported)
        return;

    for (int i = 0; i < st->textures_ase_exported_num; i++)
        layered_sprite_shutdown(&st->textures_ase_exported[i]);
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
        assert(st->textures);
        assert(st->rt_textures);
    }

    if (st->textures) {
        memset(st->textures, 0, sz);
        memset(st->rt_textures, 0, sz_rt);

        SetTraceLogLevel(LOG_WARNING);
        for (int j = 0; j < st->fsr_images.num; ++j) {
            if (koh_is_fname_image_ext(st->fsr_images.names[j])) {
                st->textures[j] = LoadTexture(st->fsr_images.names[j]);
                st->rt_textures[j] = LoadRenderTexture(
                    tbl_tex_size, tbl_tex_size
                );
            }
        }
        SetTraceLogLevel(LOG_ALL);
    }

    st->textures_num = st->fsr_images.num;
}

static void textures_load_ase_exported(Stage_SpriteLoader *st) {
    assert(st);

    textures_unload_ase_exported(st);

    if (!st->textures_ase_exported && st->fsr_ase_exported.num) {
        st->textures_ase_exported = calloc(
            st->fsr_ase_exported.num, sizeof(st->textures_ase_exported[0])
        );
        assert(st->textures_ase_exported);
    }

    if (st->textures_ase_exported) {
        const struct FilesSearchResult *fsr = &st->fsr_ase_exported;
        for (int i = 0; i < fsr->num; ++i) {
            layered_sprite_init(
                &st->textures_ase_exported[i], st->l_cfg,
                st->ref_ase_exported_tbl, fsr->names[i]
            );
        }
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
    camp_shutdown(&st->cam_processor);
    visual_tool_shutdown(&st->tool_visual);

    if (st->dc) {
        devcanvas_save(st->dc, NULL);
        devcanvas_free(st->dc);
        st->dc = NULL;
    }

    patterns_save(st);
    search_images_shutdown(st);
    search_meta_shutdown(st);
    search_ase_exported_shutdown(st);
    koh_search_files_shutdown(&st->fsr_geom);

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
    R = raylib_api_get();

    search_images(st, regex_pattern_images, regex_pattern_exclude_images);
    search_ase_exported(
        st, regex_pattern_ase_exported, regex_pattern_exclude_ase_exported
    );
    // Сцена входит только один раз
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
    const enum MetaLoaderType items[] = {
        MLT_RECTANGLE,
        MLT_RECTANGLE_ORIENTED,
        MLT_SECTOR,
        MLT_POLYLINE,
    };
    int items_num = sizeof(items) / sizeof(items[0]);
    _Static_assert(
        sizeof(labels) / sizeof(labels[0]) ==
        sizeof(items) / sizeof(items[0]),
        "internal arrays not mapped"
    );

    st->toolmode_combo = gui_combo_init(
        (const char**)labels, (const void*)items,
        sizeof(items[0]), items_num
    );
    st->toolmode_combo.on_change = toolmode_combo_changed;
    st->toolmode_combo.label = "tool mode";

    st->tool_visual.mode = VIS_TOOL_RECTANGLE;
}

static void color_combos_init(Stage_SpriteLoader *st) {
    st->grid.line_color_combo = gui_color_combo_init("line color");
    gui_color_combo_load(&st->grid.line_color_combo, cfg_fname);
    st->grid.line_color = st->grid.line_color_combo.color;

    st->tool_color_combo = gui_color_combo_init("tool line color");
    gui_color_combo_load(&st->tool_color_combo, cfg_fname);
    struct ToolCommonOpts *common = &st->tool_visual.t_rect_opts.common;
    common->line_color = st->tool_color_combo.color;
}

static void stage_sprite_loader_init(struct Stage *s) {
    Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;
    trace("stage_sprite_loader_init:\n");

    visual_tool_init(&st->tool_visual);
    // Стартовый цвет линий совпадает с индексом 0 combo (BLACK).
    visual_tool_set_line_color(&st->tool_visual, BLACK);
    // Полилиния рисуется замкнутой (будущий полигон box2d).
    st->tool_visual.t_pl_draw_opts.closed = true;

    st->grid.step = 1;
    st->grid.visible = false;

    st->cam.zoom = 1.;

    st->dc = devcanvas_new((DevCanvasOpts) {
        .cam = &st->cam,
        .fname = devcanvas_fname,
    });

    strncpy(st->geom_name, "turret_default", sizeof(st->geom_name) - 1);
    st->px_per_unit = 24.f;
    st->geom_origin = Vector2Zero();
    st->geom_set_num = 0;

    // Категория 0 всегда "default" (дефолтный фильтр box2d).
    strncpy(st->geom_categories[0], "default", GEOM_CAT_NAME_MAX - 1);
    st->geom_categories_num = 1;

    st->l_cfg = luaL_newstate();
    luaL_openlibs(st->l_cfg);

    patterns_load(st);

    meta_search_files(st);
    st->geom_selected_idx = -1;
    geom_files_search(st);
    color_combos_init(st);
    toolmode_init(st);

    cam_auto_init(&st->cam_automat, &st->cam);
    camp_init(&st->cam_processor, (CameraProcessorOpts){
        .cam = &st->cam,
        .mouse_btn_move = MOUSE_BUTTON_MIDDLE,
    });
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

    int export_version = ase_export_version(l);
    if (export_version == -1)
        return false;

    lua_pushstring(l, ase_exported_fname);
    lua_pushvalue(l, -2);
    lua_settable(l, -4);

    lua_pop(l, 1);

    return true;
}

static void ase_exported_load_lua_code(Stage_SpriteLoader *st) {
    assert(st);
    assert(st->l_cfg);
    lua_State *l = st->l_cfg;

    lua_createtable(l, 0, 0);
    lua_pushvalue(l, -1);
    st->ref_ase_exported_tbl = luaL_ref(l, LUA_REGISTRYINDEX);

    const struct FilesSearchResult *fsr = &st->fsr_ase_exported;
    for (int i = 0; i < fsr->num; ++i) {
        if (!_ase_exported_load_lua_code(l, fsr->names[i], i))
            goto _cleanup;
    }

_cleanup:
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
        .regex_pattern = regex_pattern_file,
        .deep = -1
    });
    koh_search_files_exclude_pcre(&st->fsr_ase_exported, regex_pattern_exclude);

    if (st->fsr_ase_exported.num) {
        free(st->ase_exported_selected);
        st->ase_exported_selected = calloc(
            st->fsr_ase_exported.num, sizeof(st->ase_exported_selected[0])
        );
        assert(st->ase_exported_selected);
    }

    ase_exported_load_lua_code(st);
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

    if (st->fsr_images.num) {
        free(st->images_selected);
        st->images_selected = calloc(
            st->fsr_images.num, sizeof(st->images_selected[0])
        );
        assert(st->images_selected);
    }

    textures_load_images(st);
}

static void active_sprite_load(
    Stage_SpriteLoader *st, const RenderTexture2D *tex_src
) {
    if (st->active_sprite.id)
        UnloadRenderTexture(st->active_sprite);

    const RenderTexture2D *tex_rt_aux = tex_src;
    RenderTexture2D tex_rt_active = LoadRenderTexture(
        tex_rt_aux->texture.width, tex_rt_aux->texture.height
    );
    BeginMode2D((Camera2D) { .zoom = 1., });
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
    BeginMode2D((Camera2D) { .zoom = 1., });
    BeginTextureMode(tex_rt_active);
    ClearBackground(BLANK);
    Rectangle dst = { 0., 0., tex_aux.width, tex_aux.height };
    Rectangle src = { 0., 0., tex_aux.width, -tex_aux.height };
    DrawTexturePro(tex_aux, src, dst, Vector2Zero(), 0, WHITE);
    EndMode2D();
    EndTextureMode();
    UnloadTexture(tex_aux);

    st->active_sprite = tex_rt_active;
}

static void toolmode_combo(Stage_SpriteLoader *st) {
    assert(st);
    bool mode_changed = false;
    const char *prev_mode = visual_mode2str(st->tool_visual.mode);
    const enum MetaLoaderType* value = gui_combo(
            &st->toolmode_combo, &mode_changed
    );
    // Только при реальном изменении: иначе комбобокс перетирал бы
    // режим каждый кадр, ломая радио-кнопки в geometry editor
    // (у комбобокса нет VIS_TOOL_CIRCLE, дефолт схлопывался в rect).
    if (value && mode_changed)
        st->tool_visual.mode = visual_tool_mode2metaloader_type(*value);
    if (mode_changed) {
        trace(
            "toolmode_combo: changed value from %s to %s\n",
            prev_mode, visual_mode2str(st->tool_visual.mode)
        );
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

static void gui_meta_button_new_meta_file(
    Stage_SpriteLoader *st, char *meta_fname_new
) {
    if (igButton("new meta file", (ImVec2){})) {
        char fname_full[256] = {};
        size_t left;
        left = sizeof(fname_full) - strlen(fname_full) - 1;
        strncat(fname_full, path_meta, left);
        left = sizeof(fname_full) - strlen(fname_full) - 1;
        strncat(fname_full, "/", left);
        left = sizeof(fname_full) - strlen(fname_full) - 1;
        strncat(fname_full, meta_fname_new, left);
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
                    meta_fname_new, strerror(errno)
                );
            } else
                meta_search_files(st);
        }
    }
}

// DevCanvas: переключатель режима разметки и подсказки по клавишам.
static void gui_devcanvas(Stage_SpriteLoader *st) {
    igSeparator();
    igCheckbox("devcanvas", &st->dc_enabled);
    igSameLine(0., 10.);
    if (igSmallButton("clear canvas"))
        devcanvas_clear(st->dc);
    if (st->dc_enabled)
        igText("ЛКМ — рисовать, F1..F4 — цвет, Ctrl+Z — отмена");
    else
        igText("разметка выключена (инструменты выделения активны)");
}

static void gui_meta(Stage_SpriteLoader *st) {
    assert(st);

    bool opened = true;
    ImGuiWindowFlags flags = 0;
    igBegin("meta loader", &opened, flags);

    if (igButton("load meta file(s)", (ImVec2){})) {
        // Загрузка выбранных метафайлов (в разработке).
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

    if (st->active_sprite.id) {
        Vector2 sz = {
            st->active_sprite.texture.width,
            st->active_sprite.texture.height
        };
        igText("texture size %s\n", Vector2_tostr(sz));
        igSameLine(0., -1.f);
        if (igSmallButton("copy##1"))
            SetClipboardText(Vector2_tostr(sz));
    }

    igText("camera %s", camera2str(st->cam, true));
    igText("mouse position %s", Vector2_tostr(GetMousePosition()));

    if (st->tool_visual.t_recta.exist) {
        igText("selected region %s", rect2str(st->tool_visual.t_recta.rect));
        igSameLine(0., -1.);
        if (igSmallButton("copy##region")) {
            const char *text = rect2str(st->tool_visual.t_recta.rect);
            SetClipboardText(text);
        }
    }

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

    if (igBeginTable("ase_exported_assets", 3, table_flags, outer_size, 0.)) {
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
            }

            struct LayeredSprite *ls = &st->textures_ase_exported[i];
            assert(ls);

            igTableSetColumnIndex(1);
            R.rlImGuiImage(&ls->tex_combined.texture);

            igTableSetColumnIndex(2);
            igText("%d", ls->version);
        }

        igEndTable();
    }
}

static void draw_image(Stage_SpriteLoader *st, int i) {
    Texture2D tex = st->textures[i];

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
    if (scaled)
        DrawText("SCALED", 0, 0, font_size_scaled, RED);

    EndMode2D();
    EndTextureMode();

    R.rlImGuiImageRenderTexture(&st->rt_textures[i]);
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

    if (igBeginTable("image_assets", 2, table_flags, outer_size, 0.)) {
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
            }

            igTableSetColumnIndex(1);
            draw_image(st, i);
        }

        igEndTable();
    }
}

static void gui_left_loader_group(Stage_SpriteLoader *st) {
    igBeginGroup();
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
        const char *selected_image = get_selected_image(st);
        if (selected_image) {
            sprite_load_image(st, selected_image);
        } else {
            int index = -1;
            const char *fname = get_selected_ase_exported(st, &index);
            if (fname && index != -1) {
                struct LayeredSprite *spr =
                    &st->textures_ase_exported[index];
                layered_sprite_bake(spr);
                active_sprite_load(st, &spr->tex_combined);
            }
        }
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
    lua_rawgeti(vm, LUA_REGISTRYINDEX, ref_ase_exported_tbl);
    assert(lua_type(vm, -1) == LUA_TTABLE);
    lua_pushstring(vm, selected_fname);
    lua_gettable(vm, -2);
    assert(lua_type(vm, -1) == LUA_TTABLE && "first");
    lua_pushstring(vm, "layers");
    lua_gettable(vm, -2);
    assert(lua_type(vm, -1) == LUA_TTABLE && "second");
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
    assert(selected_i != -1);

    struct LayeredSprite *ls = &st->textures_ase_exported[selected_i];
    lua_State *vm = st->l_cfg;

    if (igBeginTable("layers", 2, table_flags, outer_size, 0.)) {
        put_layers_table(vm, st->ref_ase_exported_tbl, selected_fname);

        igTableSetupColumn("name", 0, 0, 0);
        igTableSetupColumn("visible", 0, 0, 1);
        igTableHeadersRow();

        lua_pushnil(vm);
        while (lua_next(vm, -2)) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            igTableSetColumnIndex(0);

            lua_pushstring(vm, "name");
            lua_gettable(vm, -2);
            assert(lua_type(vm, -1) == LUA_TSTRING);
            const char *name = lua_tostring(vm, -1);
            lua_pop(vm, 1);

            igText("%s", name);

            bool visible = false;
            const char *visible_str = "visible";
            lua_pushstring(vm, visible_str);
            lua_gettable(vm, -2);
            if (lua_isboolean(vm, -1))
                visible = lua_toboolean(vm, -1);
            else
                trace("gui_sprite_layers: visible is not boolean\n");
            lua_pop(vm, 1);
            bool prev_visible = visible;

            igTableSetColumnIndex(1);

            // igPushID_Str по имени слоя — уникальный id для чекбокса,
            // иначе одинаковые метки "visible" склеиваются в ImGui.
            igPushID_Str(name);
            if (igCheckbox("visible", &visible)) {
                trace(
                    "gui_sprite_layers: name %s, visible %s\n",
                    name, visible ? "true" : "false"
                );
            }
            igPopID();

            lua_pushstring(vm, visible_str);
            lua_pushboolean(vm, visible);
            lua_settable(vm, -3);
            assert(lua_istable(vm, -1));

            if (prev_visible != visible) {
                layered_sprite_bake(ls);
                active_sprite_load(st, &ls->tex_combined);
            }

            lua_pop(vm, 1);
        }

        igEndTable();
    }

    lua_settop(vm, 0);
    igEndGroup();
}

static void gui_sprite_layers(Stage_SpriteLoader *st) {
    assert(st);
    int dummy;
    if (!get_selected_ase_exported(st, &dummy))
        return;
    bool opened = true;
    ImGuiWindowFlags flags = 0;
    igBegin("sprite layers", &opened, flags);
    gui_right_layers_group(st);
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

    gui_sprite_layers(st);
}

static void gui_geom_tool_radio(Stage_SpriteLoader *st) {
    int mode = st->tool_visual.mode;
    igText("tool:");
    igSameLine(0., 10.);
    if (igRadioButton_Bool("circle", mode == VIS_TOOL_CIRCLE))
        st->tool_visual.mode = VIS_TOOL_CIRCLE;
    igSameLine(0., 10.);
    if (igRadioButton_Bool("rect", mode == VIS_TOOL_RECTANGLE))
        st->tool_visual.mode = VIS_TOOL_RECTANGLE;
    igSameLine(0., 10.);
    if (igRadioButton_Bool("polyline", mode == VIS_TOOL_POLYLINE))
        st->tool_visual.mode = VIS_TOOL_POLYLINE;
}

// Подсказка по кнопкам мыши: общая часть + специфика инструмента.
static void gui_geom_hint(Stage_SpriteLoader *st) {
    igTextDisabled("СКМ — пан камеры, колесо — зум");
    switch (st->tool_visual.mode) {
    case VIS_TOOL_CIRCLE:
        igTextDisabled("ПКМ — центр+радиус, ручка на краю — радиус");
        igTextDisabled("зажать ПКМ на теле круга — двигать центр");
        break;
    case VIS_TOOL_RECTANGLE:
        igTextDisabled("ПКМ — тянуть рамку, ручки углов — размер");
        break;
    case VIS_TOOL_POLYLINE:
        igTextDisabled("ПКМ — добавить точку, зажать ПКМ — тянуть вершину");
        igTextDisabled("Shift+ПКМ на ручке — удалить точку");
        igTextDisabled("Shift+ЛКМ на ручке — двигать всю полилинию");
        igTextDisabled("оранжевый контур — выпуклая оболочка box2d");
        break;
    default:
        break;
    }
}

// Редактор имён категорий коллизий. Индекс имени = GeomShape.category.
// Имена — произвольные строки; смысл (маппинг в b2Filter) задаёт игра.
static void gui_geom_categories(Stage_SpriteLoader *st) {
    igText("collision categories (%d):", st->geom_categories_num);
    for (int i = 0; i < st->geom_categories_num; i++) {
        igPushID_Int(i);
        igSetNextItemWidth(160.f);
        // Категорию 0 ("default") не переименовываем.
        if (i == 0) {
            igText("0: %s", st->geom_categories[0]);
        } else {
            char label[32];
            snprintf(label, sizeof(label), "cat %d", i);
            igInputText(
                label, st->geom_categories[i], GEOM_CAT_NAME_MAX,
                0, NULL, NULL
            );
        }
        igPopID();
    }
    if (st->geom_categories_num < GEOM_CATEGORIES_MAX
        && igButton("add category", (ImVec2){})) {
        int idx = st->geom_categories_num++;
        snprintf(
            st->geom_categories[idx], GEOM_CAT_NAME_MAX, "cat%d", idx
        );
    }
}

static void gui_geom_list(Stage_SpriteLoader *st) {
    igText("shapes (%d):", st->geom_set_num);
    int remove_idx = -1;
    for (int i = 0; i < st->geom_set_num; i++) {
        GeomShape *g = &st->geom_set[i];
        igPushID_Int(i);
        if (g->kind == GK_CIRCLE)
            igText(
                "#%d circle c=(%.2f,%.2f) r=%.2f",
                i, g->center_u.x, g->center_u.y, g->radius_u
            );
        else if (g->kind == GK_POINT)
            igText(
                "#%d point \"%s\" (%.2f,%.2f)",
                i, g->tag, g->point_u.x, g->point_u.y
            );
        else
            igText("#%d poly %dv", i, g->vcount);
        igSameLine(0., 10.);
        if (igSmallButton("x"))
            remove_idx = i;

        // Категория коллизии — только для физических шейпов.
        if (g->kind != GK_POINT) {
            const char *items[GEOM_CATEGORIES_MAX];
            for (int c = 0; c < st->geom_categories_num; c++)
                items[c] = st->geom_categories[c];
            igSetNextItemWidth(160.f);
            igCombo_Str_arr(
                "category", &g->category,
                items, st->geom_categories_num, 0
            );
        }
        igPopID();
    }
    if (remove_idx >= 0) {
        for (int i = remove_idx; i < st->geom_set_num - 1; i++)
            st->geom_set[i] = st->geom_set[i + 1];
        st->geom_set_num--;
    }
}

// Окно «meta loader» по умолчанию скрыто; включается чекбоксом в
// geometry editor. Основной рабочий процесс — geometry editor.
static bool show_metaloader = false;

// Палитра цвета линий инструмента в geometry editor.
enum { TOOL_LINE_COLORS_NUM = 5 };
static const char *tool_line_color_names[TOOL_LINE_COLORS_NUM] = {
    "BLACK", "WHITE", "RED", "BLUE", "GREEN",
};
static const Color tool_line_colors[TOOL_LINE_COLORS_NUM] = {
    BLACK, WHITE, RED, BLUE, GREEN,
};

static void gui_geom_editor(Stage_SpriteLoader *st) {
    assert(st);
    bool opened = true;
    igBegin("geometry editor", &opened, 0);

    igInputText(
        "set name", st->geom_name, sizeof(st->geom_name), 0, NULL, NULL
    );
    igSliderFloat("px_per_unit", &st->px_per_unit, 1.f, 256.f, "%.1f", 0);

    if (igButton("origin = sprite center", (ImVec2){})) {
        if (st->active_sprite.id)
            st->geom_origin = (Vector2) {
                st->active_sprite.texture.width / 2.f,
                st->active_sprite.texture.height / 2.f,
            };
    }
    igSameLine(0., 10.);
    igText("origin: %s", Vector2_tostr(st->geom_origin));

    igSeparator();
    gui_geom_tool_radio(st);
    gui_geom_hint(st);

    static int tool_line_color_idx = 0;
    if (igCombo_Str_arr(
        "tool line color", &tool_line_color_idx,
        tool_line_color_names, TOOL_LINE_COLORS_NUM, 0
    )) {
        visual_tool_set_line_color(
            &st->tool_visual, tool_line_colors[tool_line_color_idx]
        );
    }
    igCheckbox("show_hull_preview", &show_hull_preview);

    if (igButton("add shape from tool", (ImVec2){}))
        geom_add_from_tool(st);
    igSameLine(0., 10.);
    if (igButton("reset tool", (ImVec2){}))
        visual_tool_reset_all(&st->tool_visual);

    // Точка-якорь (дуло и т.п.): ставится по позиции курсора.
    static char point_tag[GEOM_TAG_MAX] = "muzzle";
    igSetNextItemWidth(120.f);
    igInputText("point tag", point_tag, sizeof(point_tag), 0, NULL, NULL);
    igSameLine(0., 10.);
    if (igButton("add point at cursor", (ImVec2){}))
        geom_add_point(st, point_tag);

    igSeparator();
    gui_geom_categories(st);

    igSeparator();
    gui_geom_list(st);

    igSeparator();
    if (igButton("generate C code", (ImVec2){}))
        geom_codegen(st);
    igSameLine(0., 10.);
    if (igButton("clear set", (ImVec2){}))
        st->geom_set_num = 0;

    // --- Текстовые наборы (assets/geom/*.geom) ---
    igSeparator();
    igText("geom files (%s):", path_geom);

    if (st->fsr_geom.num > 0) {
        igSetNextItemWidth(300.f);
        igCombo_Str_arr(
            "saved sets", &st->geom_selected_idx,
            (const char *const *)st->fsr_geom.names, st->fsr_geom.num, 0
        );
    } else {
        igTextDisabled("(no .geom files)");
    }

    if (igButton("refresh list", (ImVec2){}))
        geom_files_search(st);
    igSameLine(0., 10.);

    bool can_load = st->fsr_geom.num > 0 && st->geom_selected_idx >= 0;
    if (!can_load) igBeginDisabled(true);
    if (igButton("load selected", (ImVec2){})) {
        char full[512];
        snprintf(
            full, sizeof(full), "%s/%s",
            path_geom, st->fsr_geom.names[st->geom_selected_idx]
        );
        if (!geom_load_from_file(st, full))
            trace("gui_geom_editor: load failed '%s'\n", full);
    }
    if (!can_load) igEndDisabled();
    igSameLine(0., 10.);

    if (igButton("save set", (ImVec2){})) {
        char full[512];
        snprintf(full, sizeof(full), "%s/%s.geom", path_geom, st->geom_name);
        if (geom_save_to_file(st, full))
            geom_files_search(st);
    }

    gui_devcanvas(st);

    igSeparator();
    igCheckbox("show meta loader", &show_metaloader);

    igEnd();
}

static void stage_sprite_loader_gui_window(struct Stage *s) {
    assert(s);
    struct Stage_SpriteLoader *st = (Stage_SpriteLoader*)s;
    gui_sprites_loader(st);
    if (show_metaloader)
        gui_meta(st);
    gui_geom_editor(st);
}

Stage *stage_sprite_loader_new2(Stage_SpriteLoader2Opts opts) {
    if (opts.regex_pattern_images) {
        size_t sz = sizeof(regex_pattern_images);
        assert(strlen(opts.regex_pattern_images) < sz);
        strncpy(regex_pattern_images, opts.regex_pattern_images, sz);
    }

    Stage_SpriteLoader *st = calloc(1, sizeof(Stage_SpriteLoader));
    if (!st) {
        printf("stage_sprite_loader_new2: bad allocation\n");
        koh_fatal();
    } else {
        st->parent.init = (Stage_callback)stage_sprite_loader_init;
        st->parent.update = (Stage_callback)stage_sprite_loader_update;
        st->parent.draw = (Stage_callback)stage_sprite_loader_draw;
        st->parent.shutdown = (Stage_callback)stage_sprite_loader_shutdown;
        st->parent.enter = (Stage_callback)stage_sprite_loader_enter;
        st->parent.leave = (Stage_callback)stage_sprite_loader_leave;
        st->parent.gui = stage_sprite_loader_gui_window;
    }
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
