// vim: fdm=marker
#pragma once

#include "raylib.h"

#define PARAGRAPH_INITIAL_LINES 25

typedef struct Paragraph {
    Color   color;
    int     linescap;
    int     linesnum, transformed_linesnum;
    char    **lines;
    char    **transformed_lines;
    Vector2 measure;
    bool    builded;
    Font    fnt;
} Paragraph;

void paragraph_init(Paragraph *prgh);
void paragraph_shutdown(Paragraph *prgh);
void paragraph_add(Paragraph *prgh, const char *fmt, ...);
void paragraph_build(Paragraph *prgh, Font fnt);
void paragraph_draw(Paragraph *prgh, Vector2 pos);
Vector2 paragraph_align_center(Paragraph *prgh);
