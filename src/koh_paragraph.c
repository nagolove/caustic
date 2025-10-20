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

#define KOH_PARAGRAPH_INITIAL_LINES 25

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

    prgh->b_lines = strbuf_init(NULL);
    prgh->b_tlines = strbuf_init(NULL);
}

void paragraph_shutdown(Paragraph *prgh) {
    assert(prgh);

    strbuf_shutdown(&prgh->b_lines);
    strbuf_shutdown(&prgh->b_tlines);

    memset(prgh, 0, sizeof(*prgh));
}

void paragraph_add(Paragraph *prgh, const char *fmt, ...) {
    assert(prgh);
    assert(fmt);

    prgh->builded = false;

    va_list args;
    va_start(args, fmt);
    strbuf_add_va(&prgh->b_lines, fmt, args);  // пробросили список аргументов
    va_end(args);
}

void paragraph_build(Paragraph *prgh) {
    assert(prgh);

    strbuf_clear(&prgh->b_tlines);

    i32 longest = 0;
    for (i32 i = 0; i < prgh->b_lines.num; i++) {
        i32 len = u8_codeptlen(prgh->b_lines.s[i]);
        if (len > longest) longest = len;
    }

    char line[(longest + 1) * 3];
    memset(line, 0, sizeof(line));

    const char *dash = "─"; // UTF - 3 chars len
    size_t dash_len = strlen(dash);

    char *pline = line;
    for(int i = 0; i < longest; i++) {
        strcpy(pline, dash);
        pline += dash_len;
    }

    strbuf_addf(&prgh->b_tlines, "┌%s┐", line);

    int j = 1;
    for(int i = 0; i < prgh->b_lines.num; i++, j++) {
        const char *cur_line = prgh->b_lines.s[i];

        if (strcmp(cur_line, "-") == 0)
            strbuf_addf(&prgh->b_tlines, "├%s┤", line);
        else {
            char spaces[longest + 1];
            memset(spaces, 0, sizeof(spaces));

            int spacesnum = longest - u8_codeptlen(cur_line);
            for(int _j = 0; _j < spacesnum; _j++) {
                spaces[_j] = ' ';
            }

            strbuf_addf(&prgh->b_tlines, "│%s%s│", cur_line, spaces);
        }
    }


    strbuf_addf(&prgh->b_tlines, "└%s┘", line);

    prgh->measure = MeasureTextEx(
        prgh->fnt, prgh->b_tlines.s[0], prgh->fnt.baseSize, 0
    );

    prgh->builded = true;
}

// TODO: Повернутый текст работает некорректно
void paragraph_draw2(Paragraph *prgh, Vector2 pos, float angle) {
    assert(prgh);

    if (!prgh->visible)
        return;

    if (!prgh->builded) {
        paragraph_build(prgh);
    }

    Vector2 coord = pos;
    Rectangle background = {
        .x = pos.x,
        .y = pos.y,
        .width = prgh->measure.x,
        .height = prgh->b_tlines.num * prgh->fnt.baseSize,
    };

    DrawRectanglePro(
        background, 
        Vector2Zero(),
        angle,
        prgh->color_background
    );

    for(int i = 0; i < prgh->b_tlines.num; ++i) {
        DrawTextPro(
            prgh->fnt, prgh->b_tlines.s[i],
            coord, Vector2Zero(),
            angle, prgh->fnt.baseSize,
            0, prgh->color_text
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
        .y = (h - prgh->b_tlines.num * prgh->fnt.baseSize) / 2.,
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
        prgh->b_tlines.num * prgh->fnt.baseSize
    };
}

void paragraph_set(Paragraph *prgh, const char *txt) {
    assert(prgh);
    assert(txt);

    strbuf_clear(&prgh->b_lines);

    char line[256] = {0};
    const char *txt_ptr = txt;
    char *line_ptr = line;
    int line_len = 0;
    while (*txt_ptr) {
        if (*txt_ptr == '\n') {
            trace("paragraph_set: line '%s'\n", line);
            paragraph_add(prgh, "%s", line);
            memset(line, 0, sizeof(line));
            line_ptr = line;
            line_len = 0;
        } else {
            // TODO: Автоформатирование под заранее определенную максимальную 
            // ширину текста
            if (line_len < (int)sizeof(line) - 1) {
                *line_ptr++ = *txt_ptr;
                line_len++;
            }
        }
        txt_ptr++;
    }

    // после цикла — добросить хвост:
    if (line_len > 0) {
        *line_ptr = '\0';
        paragraph_add(prgh, "%s", line);
    }
}


void paragraph_add_break(Paragraph *prgh) {
    prgh->builded = false;
    strbuf_addf(&prgh->b_lines, "-");
}
