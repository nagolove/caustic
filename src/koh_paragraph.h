// vim: fdm=marker
#pragma once

#include "raylib.h"

#define KOH_PARAGRAPH_INITIAL_LINES 25

typedef struct Paragraph {
    Color   color_text, color_background;
    int     linescap, linesnum, transformed_linesnum;
    char    **lines, **transformed_lines;
    Vector2 measure;
    bool    builded;
    Font    fnt;
} Paragraph;

void paragraph_add(Paragraph *prgh, const char *fmt, ...);
Vector2 paragraph_align_center(Paragraph *prgh);
void paragraph_build(Paragraph *prgh, Font fnt);
void paragraph_draw(Paragraph *prgh, Vector2 pos);
void paragraph_draw2(Paragraph *prgh, Vector2 pos, float angle);
Vector2 paragraph_get_size(Paragraph *prgh);
void paragraph_init(Paragraph *prgh);
void paragraph_set(Paragraph *prgh, const char *txt);
void paragraph_shutdown(Paragraph *prgh);

extern Color paragraph_default_color_background;
extern Color paragraph_default_color_text;
