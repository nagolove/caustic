// vim: fdm=marker
#include "koh_paragraph.h"

#include "koh_common.h"
#include "raylib.h"
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

void parse_line(const char *line) {
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

    if (!cmn->font_chars) {
        printf("paragraph_init2: use koh_common_init() before loading font\n");
        koh_fatal();
    }

    i32 fileSize = 0;
    u8  *fileData = LoadFileData(opts.ttf_fname, &fileSize);
    assert(fileData);

    // SDF font generation from TTF font
    // Parameters > font size: 16, no glyphs array provided (0), glyphs count: 0 (defaults to 95)
    prgh->fnt.glyphs = LoadFontData(
        fileData, fileSize, opts.base_size,
        cmn->font_chars, cmn->font_chars_num, FONT_SDF
    );

    // Parameters > glyphs count: 95, font size: opts.base_size, glyphs padding in image: 0 px, pack method: 1 (Skyline algorythm)
    Image atlas = GenImageFontAtlas(
        prgh->fnt.glyphs, &prgh->fnt.recs,
        cmn->font_chars_num, opts.base_size, 0, 1
    );
    prgh->tex_sdf = LoadTextureFromImage(atlas);
    UnloadImage(atlas);

    UnloadFileData(fileData);      // Free memory from loaded file

    prgh->sh_sdf = LoadShaderFromMemory(NULL, fs_code_sdf);
    // Required for SDF font
    SetTextureFilter(prgh->tex_sdf, TEXTURE_FILTER_BILINEAR);    

    prgh->is_sdf = true;
    prgh->fnt.texture = prgh->tex_sdf;
    prgh->fnt.baseSize = opts.base_size;
    prgh->fnt.glyphCount = cmn->font_chars_num;
    prgh->use_cache = opts.use_caching;
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

    if (prgh->tex_sdf.id != 0) {
        UnloadTexture(prgh->tex_sdf);
    }
    if (prgh->sh_sdf.id != 0) {
        UnloadShader(prgh->sh_sdf);
    }
    if (prgh->rt_cache.id != 0) {
        UnloadRenderTexture(prgh->rt_cache);
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

    const char *dash = "─"; // UTF - 3 chars len
    size_t dash_len = strlen(dash);

    char *pline = line;
    for(int i = 0; i < longest; i++) {
        // NOTE: здесь strncpy() только что-бы clang не бухтел
        strncpy(pline, dash, dash_len);
        pline += dash_len;
    }

    strbuf_addf(&prgh->b_tlines, "┌%s┐", line);

    for(int i = 0; i < prgh->b_lines.num; i++) {
        const char *cur_line = prgh->b_lines.s[i];
        // XXX: Сделать инфраструктуру для обработки нескольких типов \команд

        //size_t cur_line_len = strlen(cur_line);

        //parse_line(cur_line);

        // в этот буфер полетит строчка очищенная от команд
        //char cur_line_buf[cur_line_len + 1] = {};

        // нужно найти длиннейшую строку для выравнивания.
        // тогда сперва надо провести обработку цветов

        //process_colors(prgh, cur_line, cur_line_len, cur_line_buf);

        // "-" - просто строка разрыва
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

    i32 new_w = prgh->measure.x,
        new_h = prgh->b_tlines.num * prgh->fnt.baseSize;

    if (!prgh->rt_cache.id) {
        printf("paragraph_build: create rt texture %dx%d\n", new_w, new_h);
        prgh->rt_cache = LoadRenderTexture(new_w, new_h);
        SetTextureFilter(prgh->rt_cache.texture, TEXTURE_FILTER_BILINEAR);
    }

    if (prgh->rt_cache.id) {
        i32 cur_w = prgh->rt_cache.texture.width,
            cur_h = prgh->rt_cache.texture.height;

        // текстура увеличилась, надо пересоздавать
        // кеш текстура только увеличивается в размере
        if (new_w > cur_w || new_h > cur_h) {
            UnloadRenderTexture(prgh->rt_cache);
            prgh->rt_cache = LoadRenderTexture(new_w, new_h);
        }
    }

    prgh->is_builded = true;
}

static void _paragraph_draw2(Paragraph *prgh, Vector2 pos) {
    Rectangle background = {
        .x = pos.x, .y = pos.y,
        .width = prgh->measure.x,
        .height = prgh->b_tlines.num * prgh->fnt.baseSize,
    };

    float angle = 0.f;
    DrawRectanglePro(background, z, angle, prgh->color_background);

    Vector2 p = pos;
        // сколько символов пройдено
    i32 cnt = 0, 
        // позиция в color_positions
        cpos = 0;
    for(int i = 0; i < prgh->b_tlines.num; ++i) {
        const char *line = prgh->b_tlines.s[i];

        // на непредвиденный случай
        if (!line)
            continue;

        //printf("_paragraph_draw2: line %s\n", line);

        size_t line_len = strlen(line);

        if (prgh->is_sdf) 
            BeginShaderMode(prgh->sh_sdf);
        i32 bs = prgh->fnt.baseSize;

        ///*
        Color color_text = prgh->color_text;
//#warning "не рисует"
        //printf("_paragraph_draw2: p %s\n", Vector2_tostr(p));
        DrawTextPro(prgh->fnt, line, p, z, angle, bs, 0, color_text);
        // */

        //Color color_text = prgh->colors[cpos];
        //i32 pos = prgh->positions[cpos];
        //const char *slice = line;

        for (i32 j = 0; j < line_len; ++j) {
            if (prgh->positions[cpos]) {
            }
            //DrawTextPro(prgh->fnt, slice, p, z, angle, bs, 0, color_text);
        }

        if (prgh->is_sdf)
            EndShaderMode();            

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

            BeginTextureMode(prgh->rt_cache);
            BeginMode2D((Camera2D) { .zoom = 1.f, });
            ClearBackground(BLANK);
            _paragraph_draw2(prgh, (Vector2) {});
            EndMode2D();
            EndTextureMode();
        }
        //texture_save(prgh->rt_cache.texture, "analiz.paragraph.png");

        Texture2D tex = prgh->rt_cache.texture;
        Rectangle src = {
            0, 0, tex.width, -tex.height,
        }, dst = {
            pos.x, pos.y, 
            tex.width, 
            tex.height,
        };
        DrawTexturePro(tex, src, dst, z, 0.f, WHITE);
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
    int w = GetScreenWidth(), h = GetScreenHeight();
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
        .x = (GetScreenWidth() - sz.x) / 2.,
        .y = (GetScreenHeight() - sz.y) / 2.,
    };
    paragraph_draw(prgh, pos);
}
