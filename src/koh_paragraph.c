// vim: fdm=marker
#include "koh_paragraph.h"

#include "koh_logger.h"
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
        if (val > 255) {
            trace("paragraph: color value > 255\n");
            return false;
        }
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

    // 1) Префикс "\color=" (или "\@color=" для совместимости)
    if (*p != '\\') return false;
    p++;

    if (*p == '@') p++;

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
    if (*p != '}') {
        trace("paragraph: color bad syntax at '%.20s'\n", p);
        return false;
    }
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

// Парсит команду \@align=left|center|right
// Возвращает true при успехе, *out_next — после команды
static bool parse_align_command(
    const char *s, ParagraphAlign *out_align,
    const char **out_next
) {
    const char *p = s;
    if (*p != '\\') return false;
    p++;
    if (*p == '@') p++;
    if (strncmp(p, "align=", 6) != 0) return false;
    p += 6;

    if (strncmp(p, "left", 4) == 0) {
        *out_align = PARAGRAPH_ALIGN_LEFT;
        *out_next = p + 4;
    } else if (strncmp(p, "center", 6) == 0) {
        *out_align = PARAGRAPH_ALIGN_CENTER;
        *out_next = p + 6;
    } else if (strncmp(p, "right", 5) == 0) {
        *out_align = PARAGRAPH_ALIGN_RIGHT;
        *out_next = p + 5;
    } else {
        return false;
    }
    return true;
}

// Парсит команду \@reset
static bool parse_reset_command(
    const char *s, const char **out_next
) {
    const char *p = s;
    if (*p != '\\') return false;
    p++;
    if (*p == '@') p++;
    if (strncmp(p, "reset", 5) != 0) return false;
    p += 5;
    *out_next = p;
    return true;
}

// Разбирает строку: извлекает команды цвета/align/reset,
// заполняет positions[]/colors[] в prgh,
// записывает очищенный текст (без команд) в out_clean.
// global_pos — глобальная позиция символа в параграфе,
// обновляется по ходу парсинга.
// out_align — текущее выравнивание (состояние, перетекает)
static void parse_line_commands(
    Paragraph *prgh, const char *line,
    char *out_clean, i32 *global_pos,
    ParagraphAlign *out_align
) {
    const char *p = line;
    char *dst = out_clean;

    while (*p) {
        if (*p == '\\' && p[1] != '\0') {
            Color c;
            ParagraphAlign al;
            const char *next;

            if (parse_color_command(p, &c, &next)) {
                i32 n = prgh->color_positions_num;
                if (n < MAX_PARAGRAPH_COLOR_POSITION) {
                    prgh->positions[n] = *global_pos;
                    prgh->colors[n] = c;
                    prgh->color_positions_num++;
                }
                p = next;
                if (*p == '\\') p++;
                continue;
            }
            if (parse_reset_command(p, &next)) {
                i32 n = prgh->color_positions_num;
                if (n < MAX_PARAGRAPH_COLOR_POSITION) {
                    prgh->positions[n] = *global_pos;
                    prgh->colors[n] = prgh->color_text;
                    prgh->color_positions_num++;
                }
                p = next;
                if (*p == '\\') p++;
                continue;
            }
            if (parse_align_command(p, &al, &next)) {
                *out_align = al;
                p = next;
                if (*p == '\\') p++;
                continue;
            }
            // ни одна команда не распознана
            trace(
                "paragraph: unknown command at '%.30s'\n",
                p
            );
        }

        // обычный символ — копируем в очищенный буфер
        *dst++ = *p++;
        (*global_pos)++;
    }
    *dst = '\0';
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

// Длина UTF-8 символа │ = 3 байта (E2 94 82)
enum { BORDER_CHAR_BYTES = 3 };

void paragraph_build(Paragraph *prgh) {
    assert(prgh);

    strbuf_clear(&prgh->b_tlines);
    prgh->color_positions_num = 0;
    memset(prgh->aligns, 0, sizeof(prgh->aligns));

    i32 num = prgh->b_lines.num;

    // --- Проход 1: парсим команды, получаем очищенные строки ---
    // Храним очищенные строки во временном StrBuf
    StrBuf clean = strbuf_init(NULL);
    ParagraphAlign cur_align = PARAGRAPH_ALIGN_LEFT;
    i32 global_pos = 0;
    i32 longest = 0;

    for (i32 i = 0; i < num; i++) {
        const char *src = prgh->b_lines.s[i];
        size_t src_len = strlen(src);

        // буфер для очищенной строки (не длиннее оригинала)
        char buf[src_len + 1];
        memset(buf, 0, sizeof(buf));

        parse_line_commands(
            prgh, src, buf, &global_pos, &cur_align
        );

        strbuf_add(&clean, buf);

        i32 clen = u8_codeptlen(buf);
        if (clen > longest) longest = clen;

        // сохраняем align для этой строки
        if (i < MAX_PARAGRAPH_LINES)
            prgh->aligns[i] = cur_align;
    }

    // --- Проход 2: формируем b_tlines с рамкой и padding ---
    char dash_line[(longest + 1) * 4];
    memset(dash_line, 0, sizeof(dash_line));

    bool has_border =
        !(prgh->flags & PARAGRAPH_BORDER_NONE);

    if (has_border) {
        const char *dash = "─";
        size_t dash_len = strlen(dash);
        char *pd = dash_line;
        for (int i = 0; i < longest; i++) {
            strncpy(pd, dash, dash_len);
            pd += dash_len;
        }
        strbuf_addf(&prgh->b_tlines, "┌%s┐", dash_line);
    }

    // Сдвиг цветовых позиций с учётом padding
    i32 pad_shift = 0;
    i32 clean_offset = 0;

    for (i32 i = 0; i < num; i++) {
        const char *cl = clean.s[i];

        // "-" — строка-разделитель
        if (strcmp(cl, "-") == 0) {
            if (has_border)
                strbuf_addf(
                    &prgh->b_tlines, "├%s┤", dash_line
                );
            else
                strbuf_addf(
                    &prgh->b_tlines, "%s", dash_line
                );
            continue;
        }

        i32 text_len = u8_codeptlen(cl);
        i32 pad_total = longest - text_len;
        if (pad_total < 0) pad_total = 0;

        // вычисляем padding в зависимости от align
        i32 pad_left = 0, pad_right = 0;
        ParagraphAlign al = (i < MAX_PARAGRAPH_LINES)
            ? prgh->aligns[i] : PARAGRAPH_ALIGN_LEFT;

        switch (al) {
        case PARAGRAPH_ALIGN_LEFT:
            pad_right = pad_total;
            break;
        case PARAGRAPH_ALIGN_CENTER:
            pad_left = pad_total / 2;
            pad_right = pad_total - pad_left;
            break;
        case PARAGRAPH_ALIGN_RIGHT:
            pad_left = pad_total;
            break;
        }

        // сдвигаем цветовые позиции этой строки
        i32 clean_bytes = (i32)strlen(cl);
        i32 border_extra = has_border
            ? 2 * BORDER_CHAR_BYTES : 0;
        for (i32 j = 0; j < prgh->color_positions_num; j++) {
            i32 pos = prgh->positions[j];
            if (pos >= clean_offset
                && pos < clean_offset + clean_bytes) {
                prgh->positions[j] +=
                    pad_shift + pad_left + border_extra;
            }
        }
        pad_shift += pad_left + pad_right + border_extra;
        clean_offset += clean_bytes;

        char sl[pad_left + 1], sr[pad_right + 1];
        memset(sl, ' ', pad_left);  sl[pad_left] = '\0';
        memset(sr, ' ', pad_right); sr[pad_right] = '\0';

        if (has_border)
            strbuf_addf(
                &prgh->b_tlines,
                "│%s%s%s│", sl, cl, sr
            );
        else
            strbuf_addf(
                &prgh->b_tlines,
                "%s%s%s", sl, cl, sr
            );
    }

    strbuf_shutdown(&clean);

    if (has_border)
        strbuf_addf(&prgh->b_tlines, "└%s┘", dash_line);

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

// Ищет следующую цветовую позицию >= global_pos.
// Возвращает индекс в positions[] или -1.
static i32 find_next_color(
    Paragraph *prgh, i32 global_pos, i32 hint
) {
    for (i32 i = hint; i < prgh->color_positions_num; i++) {
        if (prgh->positions[i] >= global_pos)
            return i;
    }
    return -1;
}

// Рисует подстроку content с переключением цвета.
// global_pos, color_hint, cur_color обновляются.
static void draw_colored_content(
    Paragraph *prgh,
    const char *content, size_t content_len,
    Vector2 draw_pos, i32 bs,
    i32 *global_pos, i32 *color_hint, Color *cur_color
) {
    size_t drawn = 0;
    float x_off = 0.f;

    while (drawn < content_len) {
        i32 ci = find_next_color(
            prgh, *global_pos, *color_hint
        );

        size_t chunk;
        if (ci >= 0) {
            i32 chars_left =
                prgh->positions[ci] - *global_pos;
            if (chars_left <= 0) {
                *cur_color = prgh->colors[ci];
                *color_hint = ci + 1;
                continue;
            }
            chunk = (size_t)chars_left;
            if (chunk > content_len - drawn)
                chunk = content_len - drawn;
        } else {
            chunk = content_len - drawn;
        }

        char tmp[chunk + 1];
        memcpy(tmp, content + drawn, chunk);
        tmp[chunk] = '\0';

        Vector2 dp = { draw_pos.x + x_off, draw_pos.y };
        R.DrawTextPro(
            prgh->fnt, tmp, dp, z,
            0.f, bs, 0, *cur_color
        );

        Vector2 m = R.MeasureTextEx(
            prgh->fnt, tmp, bs, 0
        );
        x_off += m.x;
        drawn += chunk;
        *global_pos += (i32)chunk;

        if (ci >= 0
            && prgh->positions[ci] == *global_pos) {
            *cur_color = prgh->colors[ci];
            *color_hint = ci + 1;
        }
    }
}

// Проверяет, является ли строка рамочной (border).
// Рамочные строки начинаются с ┌, └, ├.
static bool is_border_line(const char *line) {
    // UTF-8: ┌ = E2 94 8C, └ = E2 94 94, ├ = E2 94 9C
    unsigned char c0 = (unsigned char)line[0];
    unsigned char c1 = (unsigned char)line[1];
    if (c0 != 0xE2 || c1 != 0x94) return false;
    unsigned char c2 = (unsigned char)line[2];
    return c2 == 0x8C || c2 == 0x94 || c2 == 0x9C;
}


static void _paragraph_draw2(
    Paragraph *prgh, Vector2 pos
) {
    Rectangle background = {
        .x = pos.x, .y = pos.y,
        .width = prgh->measure.x,
        .height = prgh->b_tlines.num * prgh->fnt.baseSize,
    };

    R.DrawRectanglePro(
        background, z, 0.f, prgh->color_background
    );

    bool has_border =
        !(prgh->flags & PARAGRAPH_BORDER_NONE);
    Vector2 p = pos;
    i32 bs = prgh->fnt.baseSize;
    i32 global_pos = 0;
    i32 color_hint = 0;
    Color cur_color = prgh->color_text;

    // начальный цвет
    if (prgh->color_positions_num > 0
        && prgh->positions[0] == 0) {
        cur_color = prgh->colors[0];
        color_hint = 1;
    }

    for (i32 i = 0; i < prgh->b_tlines.num; ++i) {
        const char *line = prgh->b_tlines.s[i];
        if (!line) {
            p.y += bs;
            continue;
        }

        if (prgh->is_sdf)
            R.BeginShaderMode(prgh->sh_sdf);

        size_t line_len = strlen(line);

        if (has_border && is_border_line(line)) {
            // рамочная строка — рисуем целиком без
            // цветовой разметки
            R.DrawTextPro(
                prgh->fnt, line, p, z,
                0.f, bs, 0, prgh->color_text
            );
        } else if (has_border && line_len > 2 * BORDER_CHAR_BYTES) {
            // контентная строка с рамкой: │content│
            // рисуем левый │
            char lb[BORDER_CHAR_BYTES + 1];
            memcpy(lb, line, BORDER_CHAR_BYTES);
            lb[BORDER_CHAR_BYTES] = '\0';
            R.DrawTextPro(
                prgh->fnt, lb, p, z,
                0.f, bs, 0, prgh->color_text
            );

            Vector2 lm = R.MeasureTextEx(
                prgh->fnt, lb, bs, 0
            );

            // содержимое между │...│
            const char *content =
                line + BORDER_CHAR_BYTES;
            size_t content_len =
                line_len - 2 * BORDER_CHAR_BYTES;

            Vector2 cp = { p.x + lm.x, p.y };
            draw_colored_content(
                prgh, content, content_len, cp, bs,
                &global_pos, &color_hint, &cur_color
            );

            // рисуем правый │
            const char *rb =
                line + line_len - BORDER_CHAR_BYTES;
            // измеряем всё до правого │
            char all_before[line_len - BORDER_CHAR_BYTES + 1];
            memcpy(
                all_before, line,
                line_len - BORDER_CHAR_BYTES
            );
            all_before[line_len - BORDER_CHAR_BYTES] = '\0';
            Vector2 am = R.MeasureTextEx(
                prgh->fnt, all_before, bs, 0
            );
            Vector2 rp = { p.x + am.x, p.y };
            R.DrawTextPro(
                prgh->fnt, rb, rp, z,
                0.f, bs, 0, prgh->color_text
            );
        } else {
            // без рамки — весь текст с цветовой разметкой
            draw_colored_content(
                prgh, line, line_len, p, bs,
                &global_pos, &color_hint, &cur_color
            );
        }

        if (prgh->is_sdf)
            R.EndShaderMode();

        p.y += bs;
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
