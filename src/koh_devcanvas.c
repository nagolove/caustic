// vim: set colorcolumn=85
#include "koh_devcanvas.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "raymath.h"
#include "koh_raylib_api.h"
#include "koh_logger.h"

// Отрисовка идёт через хостовый бекенд raylib_api (R), а не сырой raylib —
// иначе вывод не попадает в тот же таргет, что и остальной рендер игры.
// Ввод/математика (мышь, экран<->мир) остаются на реальном raylib.
static raylib_api R = {};

enum {
    // Магия и версия формата файла .dcnv
    DCNV_MAGIC   = 0x564e4344,  // 'D','C','N','V' в little-endian
    DCNV_VERSION = 1,
    // Начальные ёмкости динамических массивов
    STROKES_CAP0 = 64,
    POINTS_CAP0  = 32,
};

// Порог добавления точки в экранных пикселях: точки ближе порога (с учётом
// зума) не добавляются, чтобы не плодить лишние вершины.
#define POINT_MIN_SCREEN_DIST  3.0f
// Толщина штриха по умолчанию, экранные пиксели (постоянна при любом зуме)
#define DEFAULT_WIDTH          3.75f

// Один штрих — полилиния с кешем ограничивающего прямоугольника (bbox).
// bbox используется для отсечения по видимой области камеры.
typedef struct Stroke {
    Vector2    *pts;
    int         num, cap;
    Rectangle   bbox;
    Color       color;
    float       width;
} Stroke;

struct DevCanvas {
    Camera2D   *cam;
    char       *fname;      // владеющая копия имени файла, может быть NULL
    int         draw_btn;
    bool        is_enabled;     // режим рисования (обработка ввода)
    bool        is_visible;     // отрисовка штрихов (F10)

    Color       color;      // цвет новых штрихов
    float       width;      // толщина новых штрихов

    Stroke     *strokes;
    int         num, cap;

    Stroke      cur;        // штрих, рисуемый прямо сейчас
    bool        drawing;

    bool        selecting;      // тянется рамка удаления (Shift+ЛКМ)
    Vector2     sel_a, sel_b;   // углы рамки в мировых координатах

    Stroke     *trash;          // последний удалённый рамкой батч
    int         trash_num;      // для восстановления (Ctrl+Shift+Z)
};

// {{{ Вспомогательные функции штриха

static void stroke_reset(Stroke *s) {
    s->num = 0;
    s->bbox = (Rectangle){ 0 };
}

static void stroke_free(Stroke *s) {
    free(s->pts);
    *s = (Stroke){ 0 };
}

// Добавить точку в штрих, расширив массив и bbox.
static void stroke_push(Stroke *s, Vector2 p) {
    if (s->num == s->cap) {
        s->cap = s->cap ? s->cap * 2 : POINTS_CAP0;
        s->pts = realloc(s->pts, sizeof(s->pts[0]) * s->cap);
    }

    if (s->num == 0) {
        s->bbox = (Rectangle){ p.x, p.y, 0, 0 };
    } else {
        // Расширить bbox до новой точки
        float x0 = fminf(s->bbox.x, p.x);
        float y0 = fminf(s->bbox.y, p.y);
        float x1 = fmaxf(s->bbox.x + s->bbox.width,  p.x);
        float y1 = fmaxf(s->bbox.y + s->bbox.height, p.y);
        s->bbox = (Rectangle){ x0, y0, x1 - x0, y1 - y0 };
    }

    s->pts[s->num++] = p;
}
// }}}

// {{{ Рамка удаления (Shift+ЛКМ)

// Нормализованный прямоугольник по двум точкам (min + модуль стороны)
static Rectangle rect_from_pts(Vector2 a, Vector2 b) {
    float x = fminf(a.x, b.x), y = fminf(a.y, b.y);
    return (Rectangle){ x, y, fabsf(a.x - b.x), fabsf(a.y - b.y) };
}

// true, если хотя бы одна точка штриха внутри r (ластик-поведение)
static bool stroke_hits_rect(const Stroke *s, Rectangle r) {
    for (int i = 0; i < s->num; i++)
        if (CheckCollisionPointRec(s->pts[i], r))
            return true;
    return false;
}

// Удалить все штрихи, задетые рамкой r. Удалённые уходят в корзину
// (для восстановления), затирая её прошлое содержимое.
static void delete_in_rect(DevCanvas *dc, Rectangle r) {
    for (int i = 0; i < dc->trash_num; i++)   // очистить прошлую корзину
        stroke_free(&dc->trash[i]);
    dc->trash_num = 0;

    int w = 0, t = 0;
    for (int i = 0; i < dc->num; i++) {
        if (stroke_hits_rect(&dc->strokes[i], r)) {
            dc->trash = realloc(dc->trash, sizeof(dc->trash[0]) * (t + 1));
            dc->trash[t++] = dc->strokes[i];    // владение → корзина
        } else
            dc->strokes[w++] = dc->strokes[i];
    }
    dc->num = w;
    dc->trash_num = t;
}

// Вернуть последний удалённый рамкой батч в конец массива штрихов
void devcanvas_restore(DevCanvas *dc) {
    for (int i = 0; i < dc->trash_num; i++) {
        if (dc->num == dc->cap) {
            dc->cap = dc->cap ? dc->cap * 2 : STROKES_CAP0;
            dc->strokes = realloc(dc->strokes,
                                  sizeof(dc->strokes[0]) * dc->cap);
        }
        dc->strokes[dc->num++] = dc->trash[i];  // владение → strokes
    }
    dc->trash_num = 0;
}
// }}}

// Мировой прямоугольник, видимый через камеру
static Rectangle view_world_rect(const Camera2D *cam) {
    Vector2 tl = GetScreenToWorld2D((Vector2){ 0, 0 }, *cam);
    Vector2 br = GetScreenToWorld2D(
        (Vector2){ GetScreenWidth(), GetScreenHeight() }, *cam);
    return (Rectangle){ tl.x, tl.y, br.x - tl.x, br.y - tl.y };
}

DevCanvas *devcanvas_new(DevCanvasOpts opts) {
    if (!opts.cam) {
        trace("devcanvas_new: opts.cam == NULL\n");
        return NULL;
    }

    R = raylib_api_get();

    DevCanvas *dc = calloc(1, sizeof(*dc));
    dc->cam = opts.cam;
    // Копируем имя файла во владение — источник может быть временным буфером
    dc->fname = opts.fname ? strdup(opts.fname) : NULL;
    dc->draw_btn = opts.draw_btn ? opts.draw_btn : MOUSE_BUTTON_LEFT;
    dc->is_enabled = true;
    dc->is_visible = true;      // пометки видны по умолчанию
    // Прозрачный цвет трактуем как «не задан» => чёрный
    dc->color = (opts.color.a == 0) ? BLACK : opts.color;
    dc->width = (opts.width > 0) ? opts.width : DEFAULT_WIDTH;

    dc->cap = STROKES_CAP0;
    dc->strokes = calloc(dc->cap, sizeof(dc->strokes[0]));

    if (dc->fname)
        devcanvas_load(dc, dc->fname);

    return dc;
}

void devcanvas_free(DevCanvas *dc) {
    if (!dc)
        return;
    for (int i = 0; i < dc->num; i++)
        stroke_free(&dc->strokes[i]);
    free(dc->strokes);
    for (int i = 0; i < dc->trash_num; i++)
        stroke_free(&dc->trash[i]);
    free(dc->trash);
    stroke_free(&dc->cur);
    free(dc->fname);
    free(dc);
}

// Зафиксировать текущий штрих в массиве холста.
static void commit_current(DevCanvas *dc) {
    if (dc->cur.num < 2) {       // одиночная точка — не штрих
        stroke_reset(&dc->cur);
        return;
    }

    if (dc->num == dc->cap) {
        dc->cap *= 2;
        dc->strokes = realloc(dc->strokes, sizeof(dc->strokes[0]) * dc->cap);
    }
    // Переносим владение массивом точек в холст
    dc->strokes[dc->num++] = dc->cur;
    dc->cur = (Stroke){ 0 };
}

void devcanvas_update(DevCanvas *dc) {
    // Видимость (F10) переключается всегда — даже вне режима рисования
    if (IsKeyPressed(KEY_F10))
        dc->is_visible = !dc->is_visible;

    if (!dc->is_enabled)
        return;

    // Палитра: F1..F4 — чёрный/белый/красный/зелёный
    if (IsKeyPressed(KEY_F1)) dc->color = BLACK;
    if (IsKeyPressed(KEY_F2)) dc->color = WHITE;
    if (IsKeyPressed(KEY_F3)) dc->color = RED;
    if (IsKeyPressed(KEY_F4)) dc->color = GREEN;

    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    // Ctrl+Z — отменить последний штрих; Ctrl+Shift+Z — вернуть последний
    // удалённый рамкой батч
    if (ctrl && IsKeyPressed(KEY_Z))
        shift ? devcanvas_restore(dc) : devcanvas_undo(dc);

    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, *dc->cam);

    if (IsMouseButtonPressed(dc->draw_btn)) {
        // Режим (рамка/штрих) фиксируется на нажатии
        if (shift) {
            dc->selecting = true;
            dc->sel_a = dc->sel_b = world;
        } else {
            dc->drawing = true;
            dc->cur.color = dc->color;
            dc->cur.width = dc->width;
            stroke_reset(&dc->cur);
            stroke_push(&dc->cur, world);
        }
    } else if (dc->selecting && IsMouseButtonDown(dc->draw_btn)) {
        dc->sel_b = world;
    } else if (dc->selecting && IsMouseButtonReleased(dc->draw_btn)) {
        dc->selecting = false;
        delete_in_rect(dc, rect_from_pts(dc->sel_a, dc->sel_b));
    } else if (dc->drawing && IsMouseButtonDown(dc->draw_btn)) {
        // Добавляем точку только при заметном сдвиге (в экранных пикселях)
        Vector2 last = dc->cur.pts[dc->cur.num - 1];
        Vector2 last_scr = GetWorldToScreen2D(last, *dc->cam);
        if (Vector2Distance(last_scr, mouse) >= POINT_MIN_SCREEN_DIST)
            stroke_push(&dc->cur, world);
    } else if (dc->drawing && IsMouseButtonReleased(dc->draw_btn)) {
        dc->drawing = false;
        commit_current(dc);
    }
}

// Нарисовать один штрих. s->width задан в экранных пикселях, в мир
// переводим делением на зум — толщина одинакова при любом масштабе.
static void draw_stroke(const Stroke *s, float zoom) {
    float w = s->width / (zoom > 0.f ? zoom : 1.f);
    for (int i = 1; i < s->num; i++)
        R.DrawLineEx(s->pts[i - 1], s->pts[i], w, s->color);
}

void devcanvas_render(DevCanvas *dc) {
    if (!dc->is_visible)
        return;

    Rectangle view = view_world_rect(dc->cam);
    float zoom = dc->cam->zoom;

    for (int i = 0; i < dc->num; i++) {
        const Stroke *s = &dc->strokes[i];
        if (CheckCollisionRecs(s->bbox, view))
            draw_stroke(s, zoom);
    }
    // Текущий штрих рисуем без отсечения — он всегда под курсором
    if (dc->drawing && dc->cur.num > 1)
        draw_stroke(&dc->cur, zoom);

    // Рамка удаления (Shift+ЛКМ): полупрозрачная заливка + контур
    if (dc->selecting) {
        Rectangle r = rect_from_pts(dc->sel_a, dc->sel_b);
        float t = 1.5f / (zoom > 0.f ? zoom : 1.f);
        R.DrawRectangleV((Vector2){ r.x, r.y },
                         (Vector2){ r.width, r.height },
                         (Color){ 255, 0, 0, 40 });
        R.DrawRectangleLinesEx(r, t, (Color){ 255, 0, 0, 200 });
    }
}

void devcanvas_set_enabled(DevCanvas *dc, bool enabled) {
    // Вход в режим рисования принудительно включает видимость —
    // чтобы не рисовать «вслепую»
    if (enabled && !dc->is_enabled)
        dc->is_visible = true;
    dc->is_enabled = enabled;
    if (!enabled && dc->drawing) {   // прервать незавершённый штрих
        dc->drawing = false;
        commit_current(dc);
    }
}

void devcanvas_set_visible(DevCanvas *dc, bool visible) { dc->is_visible = visible; }
bool devcanvas_visible(DevCanvas *dc) { return dc->is_visible; }

void devcanvas_set_color(DevCanvas *dc, Color color) { dc->color = color; }
void devcanvas_set_width(DevCanvas *dc, float width) { dc->width = width; }

bool devcanvas_undo(DevCanvas *dc) {
    if (dc->num == 0)
        return false;
    stroke_free(&dc->strokes[--dc->num]);
    return true;
}

void devcanvas_clear(DevCanvas *dc) {
    for (int i = 0; i < dc->num; i++)
        stroke_free(&dc->strokes[i]);
    dc->num = 0;
}

// {{{ Сериализация
//
// Бинарный формат .dcnv:
//   u32 magic, u32 version, u32 num_strokes
//   на штрих: u8 r,g,b,a; f32 width; u32 num_pts; Vector2 pts[num_pts]
// bbox не пишем — восстанавливаем при загрузке.

bool devcanvas_save(DevCanvas *dc, const char *fname) {
    if (!fname)
        fname = dc->fname;
    if (!fname) {
        trace("devcanvas_save: имя файла не задано\n");
        return false;
    }

    FILE *f = fopen(fname, "wb");
    if (!f) {
        trace("devcanvas_save: не открыть '%s'\n", fname);
        return false;
    }

    uint32_t hdr[3] = { DCNV_MAGIC, DCNV_VERSION, (uint32_t)dc->num };
    fwrite(hdr, sizeof(hdr[0]), 3, f);

    for (int i = 0; i < dc->num; i++) {
        const Stroke *s = &dc->strokes[i];
        uint8_t rgba[4] = { s->color.r, s->color.g, s->color.b, s->color.a };
        uint32_t cnt = (uint32_t)s->num;
        fwrite(rgba, 1, 4, f);
        fwrite(&s->width, sizeof(s->width), 1, f);
        fwrite(&cnt, sizeof(cnt), 1, f);
        fwrite(s->pts, sizeof(s->pts[0]), s->num, f);
    }

    fclose(f);
    trace("devcanvas_save: '%s', штрихов %d\n", fname, dc->num);
    return true;
}

bool devcanvas_load(DevCanvas *dc, const char *fname) {
    if (!fname)
        fname = dc->fname;
    if (!fname)
        return false;

    FILE *f = fopen(fname, "rb");
    if (!f)
        return false;       // нет файла — не ошибка при первом запуске

    uint32_t hdr[3] = { 0 };
    bool ok = fread(hdr, sizeof(hdr[0]), 3, f) == 3;
    if (!ok || hdr[0] != DCNV_MAGIC || hdr[1] != DCNV_VERSION) {
        trace("devcanvas_load: неверный формат '%s'\n", fname);
        fclose(f);
        return false;
    }

    devcanvas_clear(dc);
    uint32_t num = hdr[2];

    for (uint32_t i = 0; i < num; i++) {
        uint8_t rgba[4];
        float width;
        uint32_t cnt;
        if (fread(rgba, 1, 4, f) != 4 ||
            fread(&width, sizeof(width), 1, f) != 1 ||
            fread(&cnt, sizeof(cnt), 1, f) != 1) {
            trace("devcanvas_load: обрыв заголовка штриха %u\n", i);
            break;
        }

        Stroke s = {
            .color = (Color){ rgba[0], rgba[1], rgba[2], rgba[3] },
            .width = width,
        };
        for (uint32_t j = 0; j < cnt; j++) {
            Vector2 p;
            if (fread(&p, sizeof(p), 1, f) != 1) {
                trace("devcanvas_load: обрыв точек штриха %u\n", i);
                cnt = 0;
                break;
            }
            stroke_push(&s, p);
        }

        if (s.num >= 2) {
            if (dc->num == dc->cap) {
                dc->cap *= 2;
                dc->strokes =
                    realloc(dc->strokes, sizeof(dc->strokes[0]) * dc->cap);
            }
            dc->strokes[dc->num++] = s;
        } else {
            stroke_free(&s);
        }
    }

    fclose(f);
    trace("devcanvas_load: '%s', штрихов %d\n", fname, dc->num);
    return true;
}
// }}}
