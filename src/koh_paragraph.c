// vim: fdm=marker
#include "paragraph.h"

#include "common.h"
#include "console.h"
#include "raylib.h"
#include "raymath.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <string.h>

void paragraph_init(Paragraph *prgh) {
    assert(prgh);
    memset(prgh, 0, sizeof(Paragraph));
    prgh->color = BLACK;
}

void paragraph_shutdown(Paragraph *prgh) {
    assert(prgh);
    for(int i = 0; i < prgh->linesnum; i++) {
        free(prgh->lines[i]);
    }
    free(prgh->lines);
    prgh->lines = NULL;
    for(int i = 0; i < prgh->transformed_linesnum; i++) {
        if (prgh->transformed_lines[i])
            free(prgh->transformed_lines[i]);
    }
    free(prgh->transformed_lines);
    prgh->transformed_lines = NULL;
}

void paragraph_add(Paragraph *prgh, const char *fmt, ...) {
    assert(prgh);
    assert(fmt);

    char buf[MAX_LINE];
    //printf("%d", (int)sizeof(buf));
    //memset(buf, 0, sizeof(char) * MAX_LINE);
    memset(buf, 0, sizeof(buf));

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (prgh->linesnum == prgh->linescap) {
        prgh->linescap += PARAGRAPH_INITIAL_LINES;
        prgh->lines = realloc(prgh->lines, sizeof(char*) * prgh->linescap);
        assert(prgh->lines);
    }

    prgh->lines[prgh->linesnum++] = strndup(buf, sizeof(buf));
}

void paragraph_build(Paragraph *prgh, Font fnt) {
    assert(prgh);

    prgh->fnt = fnt;
    // Количество строчек + верхняя + нижняя
    prgh->transformed_linesnum = prgh->linesnum + 2;
    prgh->transformed_lines = calloc(
            prgh->transformed_linesnum,
            sizeof(char*)
    );

    int longest = 0;
    for(int i = 0; i < prgh->linesnum; i++) {
        //int len = strlen(prgh->lines[i]);
        int len = u8_codeptlen(prgh->lines[i]);
        if (len > longest) 
            longest = len;
    }

    //XXX * 2
    char line[MAX_LINE * 2] = {0};
    for(int i = 0; i < longest; i++) {
        strcat(line, "─");
    }

    //printf("line '%s'\n", line);

    const int memsize = longest * 4;
    for(int i = 0; i < prgh->transformed_linesnum; i++) {
        //XXX * 3
        prgh->transformed_lines[i] = calloc(memsize, sizeof(char));
    }

    snprintf(prgh->transformed_lines[0], memsize, "┌%s┐", line);
    int j = 1;
    for(int i = 0; i < prgh->linesnum; i++,j++) {
        char spaces[MAX_LINE * 2] = {0};
        //int spacesnum = longest - strlen(prgh->lines[i]);
        int spacesnum = longest - u8_codeptlen(prgh->lines[i]);
        for(int j = 0; j < spacesnum; j++) {
            spaces[j] = ' ';
        }
        snprintf(
            prgh->transformed_lines[j],
            memsize,
            "│%s%s│",
            prgh->lines[i],
            spaces
        );
    }
    snprintf(
        prgh->transformed_lines[prgh->linesnum + 1], memsize, "└%s┘", line
    );

    prgh->measure = MeasureTextEx(
        prgh->fnt, prgh->transformed_lines[0], prgh->fnt.baseSize, 0
    );

    prgh->builded = true;
}

void paragraph_draw(Paragraph *prgh, Vector2 pos) {
    assert(prgh);

    if (!prgh->builded) {
        perror("paragraph_draw: not builded");
        exit(EXIT_FAILURE);
    }

    Vector2 coord = pos;
    Rectangle background = {
        .x = pos.x,
        .y = pos.y,
        .width = prgh->measure.x,
        .height = prgh->transformed_linesnum * prgh->fnt.baseSize,
    };
    DrawRectangleRec(background, (Color) { 255 / 2, 255 / 2, 255 / 2, 255});
    for(int i = 0; i < prgh->transformed_linesnum; ++i) {
        DrawTextEx(
            prgh->fnt,
            prgh->transformed_lines[i],
            coord,
            prgh->fnt.baseSize,
            0,
            prgh->color
        );
        coord.y += prgh->fnt.baseSize;
    }
}

Vector2 paragraph_align_center(Paragraph *prgh) {
    assert(prgh);
    int w = GetScreenWidth(), h = GetScreenHeight();
    return (Vector2) {
        .x = (w - prgh->measure.x) / 2.,
        .y = (h - prgh->transformed_linesnum * prgh->fnt.baseSize) / 2.,
    };
}
