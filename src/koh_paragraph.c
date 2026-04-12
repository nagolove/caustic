// vim: fdm=marker
#include "koh_paragraph.h"

#include "koh_common.h"
#include "koh_raylib_api.h"
#include "raylib.h"
#include "rlgl.h"
#include "sdf.fs.h"
#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "koh_routine.h"
//#include "koh_console.h"

static raylib_api R = {};
static const Vector2 z = {};
Color paragraph_default_color_background = (Color){
    255 / 2 + 10,
    255 / 2 + 10,
    255 / 2 + 10,
    255
};
Color paragraph_default_color_text = BLACK;

static void init_cmd(Paragraph *prgh) {
    assert(prgh);

    prgh->is_cmd = true;

    /*
    i32     errnumner = 0;
    size_t  erroffset = 0;
    u32     flags = 0;

    const char *pattern_color =
        "\\\\color\\s*=\\s*\\{"
        "([0-9]{1,3})\\s*,\\s*"
        "([0-9]{1,3})\\s*,\\s*"
        "([0-9]{1,3})\\s*,\\s*"
        "([0-9]{1,3})"
        "\\}";

    prgh->rx_color = pcre2_compile(
            (const u8*)pattern_color, 
            PCRE2_ZERO_TERMINATED, flags, 
            &errnumner, &erroffset, 
            NULL
            );

    assert(prgh->rx_color);
    if (!prgh->rx_color) {
        printf(
            "paragraph_init: could not compile pattern '%s' with '%s'\n",
            pattern_color, pcre_code_str(errnumner)
        );
        koh_fatal();
    }
    */

    prgh->color_positions_num = 0;
    prgh->positions[0] = 0;
    prgh->colors[0] = paragraph_default_color_text;
}

static void skip_spaces(const char **pp) {
    const char *p = *pp;
    while (*p && isspace((unsigned char)*p)) p++;
    *pp = p;
}

static bool parse_u8_0_255(const char **pp, int *out) {
    const char *p = *pp;

    if (!isdigit((unsigned char)*p))
        return false;

    int val = 0;
    while (isdigit((unsigned char)*p)) {
        val = val * 10 + (*p - '0');
        if (val > 255) return false;   // выходим за диапазон
        p++;
    }

    *out = val;
    *pp = p;
    return true;
}

// s указывает на начало последовательности "\@color={...}"
// out_color — сюда кладём результат
// out_next  — куда мы дошли (указатель на символ сразу после '}')
// возвращает true при успехе
bool parse_color_command(
    const char *s, Color *out_color, const char **out_next
) {
    const char *p = s;

    // 1) Префикс "\@color="
    if (*p != '\\') return false;
    p++;

    if (*p != '@') return false;
    p++;

    if (strncmp(p, "color=", 6) != 0)
        return false;
    p += 6;

    // 2) Опциональные пробелы
    skip_spaces(&p);

    // 3) Открывающая фигурная скобка
    if (*p != '{') return false;
    p++;

    skip_spaces(&p);

    int r, g, b, a;

    // 4) Читаем R
    if (!parse_u8_0_255(&p, &r)) return false;
    skip_spaces(&p);
    if (*p != ',') return false;
    p++;
    skip_spaces(&p);

    // 5) Читаем G
    if (!parse_u8_0_255(&p, &g)) return false;
    skip_spaces(&p);
    if (*p != ',') return false;
    p++;
    skip_spaces(&p);

    // 6) Читаем B
    if (!parse_u8_0_255(&p, &b)) return false;
    skip_spaces(&p);
    if (*p != ',') return false;
    p++;
    skip_spaces(&p);

    // 7) Читаем A
    if (!parse_u8_0_255(&p, &a)) return false;
    skip_spaces(&p);

    // 8) Закрывающая скобка
    if (*p != '}') return false;
    p++;    // теперь p указывает на символ после '}'

    // 9) Успех: заполняем Color и out_next
    if (out_color) {
        out_color->r = (unsigned char)r;
        out_color->g = (unsigned char)g;
        out_color->b = (unsigned char)b;
        out_color->a = (unsigned char)a;
    }

    if (out_next)
        *out_next = p;

    return true;
}

// Разбирает строку. Выделяет цвета.
void parse_line_commands(const char *line) {
    printf("parse_line:\n\n");

    const char *p = line;
    while (*p) {
        if (*p == '\\' && p[1] == '@') {
            Color c;
            const char *next;
            if (parse_color_command(p, &c, &next)) {
                printf("COLOR: %d,%d,%d,%d\n", c.r, c.g, c.b, c.a);
                p = next;
                continue;
            }
            // если не color, можно пробовать \@align=..., \@reset и т.д.
        }

        // иначе — обычный текст или другой синтаксис
        putchar(*p);
        p++;
    }

    printf("parse_line:\n\n");
}

static void _paragraph_init(Paragraph *prgh) {
    assert(prgh);
    memset(prgh, 0, sizeof(Paragraph));
    R = raylib_api_get();
    prgh->color_text = paragraph_default_color_text;
    prgh->color_background = paragraph_default_color_background;
    prgh->is_visible = true;

    prgh->b_lines = strbuf_init(NULL);
    prgh->b_tlines = strbuf_init(NULL);
    prgh->use_cache = true;
    init_cmd(prgh);
}

void paragraph_init(Paragraph *prgh, Font fnt) {
    _paragraph_init(prgh);
    prgh->fnt = fnt;
    prgh->is_sdf = false;
}

static void init_sdf(Paragraph *prgh,  ParagraphOpts opts) {
    struct Common *cmn = koh_cmn();
    assert(cmn);

    assert(opts.base_size >= 0);
    assert(opts.ttf_fname);

    // В dummy-режиме шрифты не загружаются
    if (raylib_api_is_dummy()) {
        prgh->is_sdf = true;
        prgh->fnt.baseSize = opts.base_size;
        prgh->fnt.glyphCount = 0;
        prgh->use_cache = opts.use_caching;
        prgh->flags = opts.flags;
        return;
    }

    if (!cmn->font_chars) {
        printf(
            "paragraph_init2: "
            "use koh_common_init() before loading font\n"
        );
        koh_fatal();
    }

    i32 fileSize = 0;
    u8  *fileData = R.LoadFileData(
        opts.ttf_fname, &fileSize
    );
    assert(fileData);

    i32 glyph_count = 0;
    prgh->fnt.glyphs = R.LoadFontData(
        fileData, fileSize, opts.base_size,
        cmn->font_chars, cmn->font_chars_num, FONT_SDF,
        &glyph_count
    );

    Image atlas = R.GenImageFontAtlas(
        prgh->fnt.glyphs, &prgh->fnt.recs,
        glyph_count, opts.base_size, 0, 1
    );
    prgh->tex_sdf = R.LoadTextureFromImage(atlas);
    R.UnloadImage(atlas);

    R.UnloadFileData(fileData);

    prgh->sh_sdf = R.LoadShaderFromMemory(
        NULL, fs_code_sdf
    );
    R.SetTextureFilter(
        prgh->tex_sdf, TEXTURE_FILTER_BILINEAR
    );

    prgh->is_sdf = true;
    prgh->fnt.texture = prgh->tex_sdf;
    prgh->fnt.baseSize = opts.base_size;
    prgh->fnt.glyphCount = glyph_count;
    prgh->use_cache = opts.use_caching;
    prgh->flags = opts.flags;
}

void paragraph_init2(Paragraph *prgh, const ParagraphOpts *_opts) {
    assert(prgh);
    _paragraph_init(prgh);

    ParagraphOpts opts = {
        .ttf_fname = NULL,
        .base_size = 16,
    };

    if (_opts)
        opts = *_opts;

    if (!opts.ttf_fname) {
        printf("paragraph_init2: ttf_fname is null\n");
        koh_fatal();
    }
    assert(opts.base_size > 0);

    init_sdf(prgh, opts);
    init_cmd(prgh);
}

void paragraph_shutdown(Paragraph *prgh) {
    assert(prgh);

    // Освободить данные шрифта (glyphs, recs),
    // выделенные в init_sdf через LoadFontData/GenImageFontAtlas.
    // Только для SDF — дефолтный шрифт освобождается в CloseWindow.
    if (prgh->is_sdf) {
        if (prgh->fnt.glyphs) {
            R.UnloadFontData(
                prgh->fnt.glyphs,
                prgh->fnt.glyphCount
            );
        }
        free(prgh->fnt.recs);
    }

    if (prgh->tex_sdf.id != 0) {
        R.UnloadTexture(prgh->tex_sdf);
    }
    if (prgh->sh_sdf.id != 0) {
        R.UnloadShader(prgh->sh_sdf);
    }
    if (prgh->rt_cache.id != 0) {
        R.UnloadRenderTexture(prgh->rt_cache);
    }

    strbuf_shutdown(&prgh->b_lines);
    strbuf_shutdown(&prgh->b_tlines);

    memset(prgh, 0, sizeof(*prgh));
}

void paragraph_add(Paragraph *prgh, const char *fmt, ...) {
    assert(prgh);
    assert(fmt);

    prgh->is_builded = false;
    prgh->is_cached = false;

    va_list args;
    va_start(args, fmt);

    //va_list copy;
    //va_copy(copy, args);
    char buf[1024] = {};
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    //va_end(copy);
    va_end(args);

    if (strlen(buf) == 0) {
        return;
    }

    //strbuf_add_va(&prgh->b_lines, fmt, args);  // пробросили список аргументов
    strbuf_add(&prgh->b_lines, buf);
}

/*
static void process_colors(
    Paragraph *prgh,
    const char *cur_line, size_t cur_line_len,
    char *cur_line_buf
) {
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(
        prgh->rx_color, NULL
    );

    size_t offset = 0;
    const char *pcur_line = cur_line;

    while (offset < cur_line_len) {

        int rc = pcre2_match(
            prgh->rx_color, (const u8*)cur_line,
            cur_line_len,
            offset,          // start offset
            0,          // options
            match_data,
            NULL
        );

        printf("paragraph_build: rc %d\n", rc);

        if (rc >= 0) {
            const size_t *ov = pcre2_get_ovector_pointer(match_data);

            // весь паттерн, нулевой вектор
            const char *pstart = cur_line + ov[2 * 0],
                       *pend   = cur_line + ov[2 * 0 + 1];

            //offset = pstart - cur_line;
            offset = pend - cur_line;
            printf("process_colors: offset %zu\n", offset);

            printf("process_colors: '%s'\n", cur_line);
            {
                char buf[cur_line_len + 1] = {};
                strcpy(buf, cur_line);
                buf[ov[2 * 0 + 1]] = '|';
                printf("process_colors: '%s'\n", buf);
            }
            printf(
                "process_colors: start %zu, end %zu \n",
                ov[2 * 0], ov[2 * 0 + 1]
            );

            char *pcur_line_buf = cur_line_buf;
            while (pcur_line < pstart) {
                *pcur_line_buf++ = *pcur_line;

                printf("process_colors: *pcur_line '%c'\n", *pcur_line);
                pcur_line++;
            }

            printf("paragraph_build: cur_line_buf partial '%s' \n", cur_line_buf);
            pcur_line = pend;
            while (*pcur_line) {
                *pcur_line_buf++ = *pcur_line++;
            }
            printf("paragraph_build: cur_line_buf '%s' \n", cur_line_buf);

            // group 1..4 — это r,g,b,a
            for (int g = 1; g <= 4; ++g) {
                size_t start = ov[2 * g];
                size_t end   = ov[2 * g + 1];
                size_t len   = end - start;

                // ВАЖНО: subject — это исходный буфер, берем оттуда
                char tmp[4] = {0}; // max "255" + '\0'
                if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;

                memcpy(tmp, cur_line + start, len);
                tmp[len] = '\0';

                int value = atoi(tmp);
                // value — число из группы
                printf("paragraph_build: value %d\n", value);
            }
        } else 
            break;


    }

    pcre2_match_data_free(match_data);
    printf("process_colors: '%s' \n\n\n", cur_line_buf);
}
*/


/*
static void process_colors(
    Paragraph *prgh,
    const char *cur_line, size_t cur_line_len,
    char *cur_line_buf
) {
}
*/

void paragraph_build(Paragraph *prgh) {
    assert(prgh);

    strbuf_clear(&prgh->b_tlines);

    // поиск длиннейшей строки без учета форматирования
    i32 longest = 0;
    for (i32 i = 0; i < prgh->b_lines.num; i++) {
        i32 len = u8_codeptlen(prgh->b_lines.s[i]);
        if (len > longest) longest = len;
    }

    // нужно найти длиннейшую строку среди cur_line_buf,
    // но тогда нужна еще одна буферизация для поддержки команды выравнивания
    //
    // варианты - использовать еще один StrBuf
    // как часто вызывается paragraph_build()?
    /* сделать 
     strbuf_init((StrBufOpts) {
        // выделить по памяти как в alloc_buf
        .alloc_buf = other_buf,
     });
     */

    char line[(longest + 1) * 4];
    memset(line, 0, sizeof(line));

    bool has_border = !(prgh->flags & PARAGRAPH_BORDER_NONE);

    if (has_border) {
        const char *dash = "─"; // UTF - 3 chars len
        size_t dash_len = strlen(dash);

        char *pline = line;
        for(int i = 0; i < longest; i++) {
            // NOTE: здесь strncpy() только что-бы clang не бухтел
            strncpy(pline, dash, dash_len);
            pline += dash_len;
        }
        strbuf_addf(&prgh->b_tlines, "┌%s┐", line);
    }

    for(int i = 0; i < prgh->b_lines.num; i++) {
        const char *cur_line = prgh->b_lines.s[i];
        // XXX: Сделать инфраструктуру для обработки нескольких типов \команд

        //size_t cur_line_len = strlen(cur_line);

        //parse_line_commands(cur_line);

        // в этот буфер полетит строчка очищенная от команд
        //char cur_line_buf[cur_line_len + 1] = {};

        // нужно найти длиннейшую строку для выравнивания.
        // тогда сперва надо провести обработку цветов

        //process_colors(prgh, cur_line, cur_line_len, cur_line_buf);

        // "-" - просто строка разрыва
        if (strcmp(cur_line, "-") == 0) {
            if (has_border)
                strbuf_addf(&prgh->b_tlines, "├%s┤", line);
            else
                strbuf_addf(&prgh->b_tlines, "%s", line);
        } else {
            char spaces[longest + 1];
            memset(spaces, 0, sizeof(spaces));

            int spacesnum = longest - u8_codeptlen(cur_line);
            for(int _j = 0; _j < spacesnum; _j++) {
                spaces[_j] = ' ';
            }

            if (has_border)
                strbuf_addf(&prgh->b_tlines, "│%s%s│", cur_line, spaces);
            else
                strbuf_addf(&prgh->b_tlines, "%s%s", cur_line, spaces);
        }
    }

    if (has_border)
        strbuf_addf(&prgh->b_tlines, "└%s┘", line);

    if (prgh->b_tlines.num) {
        const char *cur_line = prgh->b_tlines.s[0];
        if (cur_line) {
            i32 base_size = prgh->fnt.baseSize;
            prgh->measure = R.MeasureTextEx(
                prgh->fnt, cur_line, base_size, 0
            );
        }
    }

    i32 new_w = prgh->measure.x,
        new_h = prgh->b_tlines.num * prgh->fnt.baseSize;

    // LoadRenderTexture() внутри вызывает rlDisableFramebuffer(),
    // который сбрасывает GL framebuffer на экран (id=0).
    // Если paragraph_build() вызван внутри чужого
    // BeginTextureMode(), это ломает текущий render target и
    // вызывает вспышку — часть кадра рисуется на экран.
    // Сохраняем и восстанавливаем активный framebuffer.
    unsigned int prev_fbo = R.rlGetActiveFramebuffer();

    if (!prgh->rt_cache.id) {
        printf(
            "paragraph_build: create rt texture %dx%d\n",
            new_w, new_h
        );
        prgh->rt_cache = R.LoadRenderTexture(
            new_w, new_h
        );
        R.SetTextureFilter(
            prgh->rt_cache.texture,
            TEXTURE_FILTER_BILINEAR
        );
    }

    if (prgh->rt_cache.id) {
        i32 cur_w = prgh->rt_cache.texture.width,
            cur_h = prgh->rt_cache.texture.height;

        if (new_w > cur_w || new_h > cur_h) {
            R.UnloadRenderTexture(prgh->rt_cache);
            prgh->rt_cache = R.LoadRenderTexture(
                new_w, new_h
            );
        }
    }

    R.rlEnableFramebuffer(prev_fbo);

    prgh->is_builded = true;
}

static void _paragraph_draw2(Paragraph *prgh, Vector2 pos) {
    Rectangle background = {
        .x = pos.x, .y = pos.y,
        .width = prgh->measure.x,
        .height = prgh->b_tlines.num * prgh->fnt.baseSize,
    };

    float angle = 0.f;
    R.DrawRectanglePro(
        background, z, angle, prgh->color_background
    );

    Vector2 p = pos;
    i32 cnt = 0,
        cpos = 0;
    for(int i = 0; i < prgh->b_tlines.num; ++i) {
        const char *line = prgh->b_tlines.s[i];

        if (!line)
            continue;

        size_t line_len = strlen(line);

        if (prgh->is_sdf)
            R.BeginShaderMode(prgh->sh_sdf);
        i32 bs = prgh->fnt.baseSize;

        Color color_text = prgh->color_text;
        R.DrawTextPro(
            prgh->fnt, line, p, z,
            angle, bs, 0, color_text
        );

        for (i32 j = 0; j < line_len; ++j) {
            if (prgh->positions[cpos]) {
            }
        }

        if (prgh->is_sdf)
            R.EndShaderMode();            

        p.y += prgh->fnt.baseSize;
        cnt += line_len;
    }
}

// TODO: Повернутый текст работает некорректно
void paragraph_draw2(Paragraph *prgh, Vector2 pos) {
    assert(prgh);

    // XXX: Нужно ли строить параграф если он не виден?
    // Возможно нужно что-бы получить корректные размеры текста
    if (!prgh->is_visible)
        return;

    if (!prgh->is_builded) {
        paragraph_build(prgh);
    }

    /*
    DrawRectanglePro((Rectangle) {
            .x = pos.x,
            .y = pos.y,
            .width = prgh->measure.x,
            .height = prgh->b_tlines.num * prgh->fnt.baseSize,
        }, z, angle, BROWN
    );
    // */

    if (prgh->use_cache) {
        //DrawCircleV(pos, 400, BLUE);

        if (!prgh->is_cached) {
            prgh->is_cached = true;

            R.BeginTextureMode(prgh->rt_cache);
            R.BeginMode2D(
                (Camera2D) { .zoom = 1.f, }
            );
            R.ClearBackground(BLANK);
            _paragraph_draw2(prgh, (Vector2) {});
            R.EndMode2D();
            R.EndTextureMode();
        }

        Texture2D tex = prgh->rt_cache.texture;
        Rectangle src = {
            0, 0, tex.width, -tex.height,
        }, dst = {
            pos.x, pos.y,
            tex.width,
            tex.height,
        };
        R.DrawTexturePro(
            tex, src, dst, z, 0.f, WHITE
        );
    } else {
        //DrawCircleV(pos, 400, RED);
        _paragraph_draw2(prgh, pos);
    }

}

void paragraph_draw(Paragraph *prgh, Vector2 pos) {
    paragraph_draw2(prgh, pos);
}

Vector2 paragraph_align_center(Paragraph *prgh) {
    assert(prgh);
    assert(prgh->is_builded);
    int w = R.GetScreenWidth();
    int h = R.GetScreenHeight();
    return (Vector2) {
        .x = (w - prgh->measure.x) / 2.,
        .y = (h - prgh->b_tlines.num * prgh->fnt.baseSize) / 2.,
    };
}

Vector2 paragraph_get_size(Paragraph *prgh) {
    assert(prgh);
    if (!prgh->is_builded) {
        perror("paragraph_get_size: not builded");
        koh_fatal();
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
            //trace("paragraph_set: line '%s'\n", line);
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
            } else
                printf("paragraph_set: text line truncated\n");
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
    prgh->is_builded = false;
    prgh->is_cached = false;
    strbuf_addf(&prgh->b_lines, "-");
}

void paragraph_clear(Paragraph *prgh) {
    assert(prgh);
    prgh->is_builded = false;
    prgh->is_cached = false;
    strbuf_clear(&prgh->b_tlines);
    strbuf_clear(&prgh->b_lines);
}


void paragraph_draw_center(Paragraph *prgh) {
    assert(prgh);
    Vector2 sz = paragraph_get_size(prgh);
    Vector2 pos = {
        .x = (R.GetScreenWidth() - sz.x) / 2.,
        .y = (R.GetScreenHeight() - sz.y) / 2.,
    };
    paragraph_draw(prgh, pos);
}
