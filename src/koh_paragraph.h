// vim: fdm=marker
#pragma once

#include "raylib.h"
#include "koh_strbuf.h"
#include "koh_common.h"

typedef struct Paragraph {
    StrBuf  b_lines, b_tlines;
    Color   color_text, color_background;

    //int     linescap, linesnum, transformed_linesnum;
    //char    **lines, **transformed_lines;

    Vector2 measure;
    bool    builded, visible;
    Font    fnt;
} Paragraph;

__attribute__((__format__ (__printf__, 2, 3)))
void paragraph_add(Paragraph *prgh, const char *fmt, ...);
Vector2 paragraph_align_center(Paragraph *prgh);
void paragraph_build(Paragraph *prgh);
void paragraph_draw(Paragraph *prgh, Vector2 pos);
void paragraph_draw2(Paragraph *prgh, Vector2 pos, float angle);
Vector2 paragraph_get_size(Paragraph *prgh);
void paragraph_set(Paragraph *prgh, const char *txt);
void paragraph_add_break(Paragraph *prgh);

void paragraph_init(Paragraph *prgh, Font fnt);
void paragraph_shutdown(Paragraph *prgh);

extern Color paragraph_default_color_background;
extern Color paragraph_default_color_text;
