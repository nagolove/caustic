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
};

typedef struct Paragraph {
    // с какой позиции 
    i32     positions[MAX_PARAGRAPH_COLOR_POSITION];
    // какой цвет начинается
    Color   colors[MAX_PARAGRAPH_COLOR_POSITION];
    i32     color_positions_num;

    StrBuf  b_lines, b_tlines;
    Color   color_text, color_background;

    //pcre2_code          *rx_color;
    //pcre2_match_data    *rx_match;

                    // внутренний флаг
    bool            is_sdf;
    Shader          sh_sdf;
    Texture2D       tex_sdf;
    RenderTexture2D rt_cache;
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
} Paragraph;

typedef enum ParagraphFlags {
    // XXX: Сделать окантовку двумя режимами - псевдографика и пиксельная рамка
    PARAGRAPH_FLAGS_BORDER_PSEUDO = 0b01,
} ParagraphFlags;

typedef struct ParagraphOpts {
    const char  *ttf_fname;
    i32         base_size;
    bool        use_caching;
    u32         flags;
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

extern Color paragraph_default_color_background;
extern Color paragraph_default_color_text;
