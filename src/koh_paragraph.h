// vim: fdm=marker
#pragma once

#define PCRE2_CODE_UNIT_WIDTH   8

#include "pcre2.h"
#include "raylib.h"
#include "koh_strbuf.h"
#include "koh_common.h"

// с какой позиции какой цвет начинается
/*
typedef struct ParagraphColorPosition {
    i32     pos;
    Color   c;
} ParagraphColorPosition;
*/

enum {
    MAX_PARAGRAPH_COLOR_POSITION = 1024,
    MAX_PARAGRAPH_LINES = 256,
};

typedef enum ParagraphAlign {
    PARAGRAPH_ALIGN_LEFT = 0,
    PARAGRAPH_ALIGN_CENTER,
    PARAGRAPH_ALIGN_RIGHT,
} ParagraphAlign;

typedef struct Paragraph {
    // с какой позиции 
    i32     positions[MAX_PARAGRAPH_COLOR_POSITION];
    // какой цвет начинается
    Color   colors[MAX_PARAGRAPH_COLOR_POSITION];
    i32     color_positions_num;

    // выравнивание per-line (индекс — номер строки
    // в b_tlines, без учёта рамки)
    ParagraphAlign aligns[MAX_PARAGRAPH_LINES];

    StrBuf  b_lines, b_tlines;
    Color   color_text, color_background;

    //pcre2_code          *rx_color;
    //pcre2_match_data    *rx_match;

                    // внутренний флаг
    bool            is_sdf;
    Shader          sh_sdf;
    Texture2D       tex_sdf;
    RenderTexture2D rt_cache;

    // Обводка SDF-текста. outline_width в единицах SDF-дистанции
    // (~0.05..0.25 разумный диапазон); <= 0 отключает обводку.
    Color           outline_color;
    float           outline_width;
    // Кэш локаций юниформов шейдера обводки
    int             loc_outline_color, loc_outline_width;
                    // внутрениий флаг
    bool            is_cached, 
                    // включить/отключить кеширование
                    use_cache, 
                    // использовать встроенные в текст команды для цвета 
                    // и выравнивания
                    is_cmd;

    Vector2 measure;
    bool    is_builded, is_visible;
    Font    fnt;
    u32     flags;
} Paragraph;

// AI: Добавить модальное поведение для параграфа - используя встроенный InputBinder - параграф может создавать хоткеи
// Как лучше - использовать встроенный биндер или внешний? Добавить update для параграфа.
// Задача - для паузы в игре - запускается параграф. И он-же ловит нажатия клавиш. 
// Во внешнем коде обработка - выход в меню или снятие с паузы.
// Так-же параграф может использоваться в koh-hexia для справки по игре. Снизу будет подпись "нажми X для закрытия" 
// Норм использование?
// Как описывать конфигурацию биндов? Покажи пример. 
// Если добавить флаг bool is_binder в опции ParagraphOpts и опциональные указатели на InputKbMouseDrawer,
// InputGamepadDrader туда-же
//


typedef enum ParagraphFlags {
    // XXX: Сделать окантовку двумя режимами - псевдографика и пиксельная рамка
    PARAGRAPH_BORDER_PSEUDO = 0b010,
    PARAGRAPH_BORDER_NONE   = 0b100,
} ParagraphFlags;

typedef struct ParagraphOpts {
    const char  *ttf_fname;
    i32         base_size;
    bool        use_caching;
    u32         flags;
    // удобная альтернатива flags |= PARAGRAPH_BORDER_NONE
    bool        no_border;
} ParagraphOpts;

__attribute__((__format__ (__printf__, 2, 3)))
void paragraph_add(Paragraph *prgh, const char *fmt, ...);

//Vector2 paragraph_align_center(Paragraph *prgh);
void paragraph_build(Paragraph *prgh);

void paragraph_draw(Paragraph *prgh, Vector2 pos);
void paragraph_draw2(Paragraph *prgh, Vector2 pos);
void paragraph_draw_center(Paragraph *prgh);

Vector2 paragraph_get_size(Paragraph *prgh);
// Разбивает по переводу строки
void paragraph_set(Paragraph *prgh, const char *txt);
void paragraph_add_break(Paragraph *prgh);
void paragraph_clear(Paragraph *prgh);

void paragraph_init(Paragraph *prgh, Font fnt);
void paragraph_init2(Paragraph *prgh, const ParagraphOpts *opts);
void paragraph_shutdown(Paragraph *prgh);

// Обводка SDF-текста. width — толщина в единицах SDF-дистанции
// (~0.05..0.25); width <= 0 отключает обводку. Работает только в
// SDF-режиме (paragraph_init2). После вызова сбрасывает кэш рендера.
void paragraph_set_outline(Paragraph *prgh, Color color, float width);

extern Color paragraph_default_color_background;
extern Color paragraph_default_color_text;
