// vim: fdm=marker
#include "koh_paragraph.h"

#include "koh_common.h"
//#include "koh_console.h"
#include "koh_logger.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Color paragraph_default_color_background = (Color){
    255 / 2, 255 / 2, 255 / 2, 255
};
Color paragraph_default_color_text = BLACK;

void paragraph_init(Paragraph *prgh, Font fnt) {
    assert(prgh);
    memset(prgh, 0, sizeof(Paragraph));
    prgh->color_text = paragraph_default_color_text;
    prgh->color_background = paragraph_default_color_background;
    prgh->visible = true;
    prgh->fnt = fnt;
}

void paragraph_shutdown(Paragraph *prgh) {
    assert(prgh);
    if (prgh->lines) {
        for(int i = 0; i < prgh->linesnum; i++) {
            if (prgh->lines[i])
                free(prgh->lines[i]);
        }
        free(prgh->lines);
        prgh->lines = NULL;
    }
    if (prgh->transformed_lines) {
        for(int i = 0; i < prgh->transformed_linesnum; i++) {
            if (prgh->transformed_lines[i])
                free(prgh->transformed_lines[i]);
        }
        free(prgh->transformed_lines);
        prgh->transformed_lines = NULL;
    }
    memset(prgh, 0, sizeof(*prgh));
}

void paragraph_add(Paragraph *prgh, const char *fmt, ...) {
    assert(prgh);
    assert(fmt);

    char buf[256];
    //printf("%d", (int)sizeof(buf));
    //memset(buf, 0, sizeof(char) * MAX_LINE);
    memset(buf, 0, sizeof(buf));

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (prgh->linesnum == prgh->linescap) {
        prgh->linescap += KOH_PARAGRAPH_INITIAL_LINES;
        prgh->lines = realloc(prgh->lines, sizeof(char*) * prgh->linescap);
        assert(prgh->lines);
    }

    prgh->lines[prgh->linesnum++] = strndup(buf, sizeof(buf));
}

void paragraph_build(Paragraph *prgh) {
    assert(prgh);

    //prgh->fnt = fnt;

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
    char line[256 * 2] = {0};
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
        char spaces[256 * 2] = {0};
        //int spacesnum = longest - strlen(prgh->lines[i]);
        int spacesnum = longest - u8_codeptlen(prgh->lines[i]);
        for(int _j = 0; j < spacesnum; j++) {
            spaces[_j] = ' ';
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

// TODO: Повернутый текст работает некорректно
void paragraph_draw2(Paragraph *prgh, Vector2 pos, float angle) {
    assert(prgh);

    if (!prgh->visible)
        return;

    if (!prgh->builded) {
        //perror("paragraph_draw: not builded");
        //exit(EXIT_FAILURE);
    }

    Vector2 coord = pos;
    Rectangle background = {
        .x = pos.x,
        .y = pos.y,
        .width = prgh->measure.x,
        .height = prgh->transformed_linesnum * prgh->fnt.baseSize,
    };

    DrawRectanglePro(
        background, 
        Vector2Zero(),
        angle,
        prgh->color_background
    );

    for(int i = 0; i < prgh->transformed_linesnum; ++i) {

        /*
        DrawTextEx(
            prgh->fnt,
            prgh->transformed_lines[i],
            coord,
            prgh->fnt.baseSize,
            0,
            prgh->color
        );
        */

        DrawTextPro(
            prgh->fnt,
            prgh->transformed_lines[i],
            coord,
            Vector2Zero(),
            angle,
            prgh->fnt.baseSize,
            0,
            prgh->color_text
        );

        coord.y += prgh->fnt.baseSize;
    }
}

void paragraph_draw(Paragraph *prgh, Vector2 pos) {
    paragraph_draw2(prgh, pos, 0.);
}

Vector2 paragraph_align_center(Paragraph *prgh) {
    assert(prgh);
    int w = GetScreenWidth(), h = GetScreenHeight();
    return (Vector2) {
        .x = (w - prgh->measure.x) / 2.,
        .y = (h - prgh->transformed_linesnum * prgh->fnt.baseSize) / 2.,
    };
}

Vector2 paragraph_get_size(Paragraph *prgh) {
    assert(prgh);
    if (!prgh->builded) {
        perror("paragraph_draw: not builded");
        exit(EXIT_FAILURE);
    }
    return (Vector2) {
        prgh->measure.x,
        prgh->transformed_linesnum * prgh->fnt.baseSize
    };
}

void paragraph_set(Paragraph *prgh, const char *txt) {
    assert(prgh);
    assert(txt);

    trace("paragraph_set:\n");

    char line[256] = {0};
    const char *txt_ptr = txt;
    char *line_ptr = line;
    int line_len = 0;
    while (*txt_ptr) {
        if (*txt_ptr == '\n') {
            trace("paragraph_set: line '%s'\n", line);
            paragraph_add(prgh, line);
            memset(line, 0, sizeof(line));
            line_ptr = line;
            line_len = 0;
        } else {
            // TODO: Автоформатирование под заранее определенную максимальную 
            // ширину текста
            if (line_len < sizeof(line)) {
                *line_ptr++ = *txt_ptr;
                line_len++;
            }
        }
        txt_ptr++;
    }
}
