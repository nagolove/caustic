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
#include "sdf.fs.h"

#include <string.h>

static const Vector2 z = {};
Color paragraph_default_color_background = (Color){
    255 / 2 + 10,
    255 / 2 + 10,
    255 / 2 + 10,
    255
};
Color paragraph_default_color_text = BLACK;

void paragraph_init(Paragraph *prgh, Font fnt) {
    assert(prgh);
    memset(prgh, 0, sizeof(Paragraph));
    prgh->color_text = paragraph_default_color_text;
    prgh->color_background = paragraph_default_color_background;
    prgh->is_visible = true;
    prgh->fnt = fnt;

    prgh->b_lines = strbuf_init(NULL);
    prgh->b_tlines = strbuf_init(NULL);
    prgh->use_cache = false;
}

void paragraph_init2(Paragraph *prgh, const ParagraphOpts *_opts) {
    assert(prgh);

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

    memset(prgh, 0, sizeof(Paragraph));
    prgh->color_text = paragraph_default_color_text;
    prgh->color_background = paragraph_default_color_background;
    prgh->is_visible = true;
    //prgh->fnt = fnt;
    
    struct Common *cmn = koh_cmn();
    assert(cmn);

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
    prgh->use_cache = true;

    prgh->b_lines = strbuf_init(NULL);
    prgh->b_tlines = strbuf_init(NULL);

    i32     errnumner = 0;
    size_t  erroffset = 0;
    u32     flags = 0;

    const char *pattern_start =
        "\\\\color\\s*=\\s*\\{"
            "([0-9]{1,3})\\s*,\\s*"
            "([0-9]{1,3})\\s*,\\s*"
            "([0-9]{1,3})\\s*,\\s*"
            "([0-9]{1,3})"
        "\\}";

    prgh->rx_code = pcre2_compile(
        (const u8*)pattern_start, 
        PCRE2_ZERO_TERMINATED, flags, 
        &errnumner, &erroffset, 
        NULL
    );

    assert(prgh->rx_code);
    if (!prgh->rx_code) {
        printf(
            "paragraph_init: could not compile pattern '%s' with '%s'\n",
            pattern_start, pcre_code_str(errnumner)
        );
        koh_fatal();
    }
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

void paragraph_build(Paragraph *prgh) {
    assert(prgh);

    /*
    // */

    strbuf_clear(&prgh->b_tlines);

    i32 longest = 0;
    for (i32 i = 0; i < prgh->b_lines.num; i++) {
        i32 len = u8_codeptlen(prgh->b_lines.s[i]);
        if (len > longest) longest = len;
    }

    char line[(longest + 1) * 4];
    memset(line, 0, sizeof(line));

    const char *dash = "─"; // UTF - 3 chars len
    size_t dash_len = strlen(dash);

    char *pline = line;
    for(int i = 0; i < longest; i++) {
        strcpy(pline, dash);
        pline += dash_len;
    }

    strbuf_addf(&prgh->b_tlines, "┌%s┐", line);

    for(int i = 0; i < prgh->b_lines.num; i++) {
        const char *cur_line = prgh->b_lines.s[i];
        //size_t cur_line_len = strlen(cur_line);

        // в этот буфер полетят строчки очишенные от команд
        //char cur_line_buf[cur_line_len] = {};

        /*
        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(
            prgh->rx_code, NULL
        );

        int rc = pcre2_match(
            prgh->rx_code,
            (const u8*)cur_line,
            cur_line_len,
            0,          // start offset
            0,          // options
            match_data,
            NULL
        );
        */

        /*
    if (rc < 0) {
        // нет совпадений или ошибка
        pcre2_match_data_free(match_data);
        // ...
    }
    */


        /*
PCRE2_SIZE *ov = pcre2_get_ovector_pointer(match_data);

// group 1..4 — это r,g,b,a
for (int g = 1; g <= 4; ++g) {
    PCRE2_SIZE start = ov[2 * g];
    PCRE2_SIZE end   = ov[2 * g + 1];
    PCRE2_SIZE len   = end - start;

    // ВАЖНО: subject — это исходный буфер, берем оттуда
    char tmp[4] = {0}; // max "255" + '\0'
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;

    memcpy(tmp, subject + start, len);
    tmp[len] = '\0';

    int value = atoi(tmp);
    // value — число из группы
}
*/

        //pcre2_match_data_free(match_data);

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
    for(int i = 0; i < prgh->b_tlines.num; ++i) {
        const char *line = prgh->b_tlines.s[i];
        if (prgh->is_sdf) 
            BeginShaderMode(prgh->sh_sdf);
        i32 bs = prgh->fnt.baseSize;
        Color color_text = prgh->color_text;
        DrawTextPro(prgh->fnt, line, p, z, angle, bs, 0, color_text);
        if (prgh->is_sdf)
            EndShaderMode();            
        p.y += prgh->fnt.baseSize;
    }
}

// TODO: Повернутый текст работает некорректно
void paragraph_draw2(Paragraph *prgh, Vector2 pos) {
    assert(prgh);

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
