// vim: fdm=marker
#pragma once

#include "raylib.h"

#define PARAGRAPH_INITIAL_LINES 25

typedef struct Paragraph {
    Color   color;
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
Vector2 paragraph_get_size(Paragraph *prgh);
void paragraph_init(Paragraph *prgh);
void paragraph_set(Paragraph *prgh, const char *txt);
void paragraph_shutdown(Paragraph *prgh);
