// vim: fdm=marker
#pragma once

#include "koh_paragraph.h"
#include "koh_rand.h"

#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_structs.h"
#include "chipmunk/chipmunk_types.h"

#include "utf8proc.h"
#include "raylib.h"
#include "raymath.h"

typedef struct Object Object;

typedef struct {
    bool quit;

    int *font_chars;
    int font_chars_num, font_chars_cap;

    /* Ссылка на Луа таблицу вида
        registered_functions = {
            "func_name" = "description",
        }
    */
} Common;

extern Common cmn;

void common_init(void);
void common_shutdown(void);

Color interp_color(Color a, Color b, float t);
Color height_color(float value);

const char *color2str(Color color);
Font load_font_unicode(const char *fname, int size);

void space_shutdown(cpSpace *space);
void space_debug_draw(cpSpace *space, Color color);

void draw_paragraph(
    Font fnt, 
    char **paragraph, 
    int num, 
    Vector2 pos,
    Color color
);

float axis2zerorange(float value);

/*
typedef struct StrBuilder {
    char **lines;
    int linesnum;
} StrBuilder;

void str_builder_init(StrBuilder *sb);
void str_builder_done(StrBuilder *sb);
char *str_builder_append(StrBuilder *sb);
char *str_builder_concat(StrBuilder *sb, int *len);
*/

// не выделяет памяти под строку, статический буфер в функции
const char *uint8t_to_bitstr(uint8_t value);

char *to_bitstr32(uint32_t value);

int u8_codeptlen(const char *str);
void save_scripts(FILE *file);
void bb_draw(cpBB bb, Color color);
Vector2 random_inrect(xorshift32_state *st, Rectangle rect);
Vector2 random_outrect_quad(
    xorshift32_state *st, Vector2 start, int w, int border_width
);
Vector2 bzr_cubic(Vector2 segments4[4], float t);
void register_all_functions(void);
const char *shapefilter2str(cpShapeFilter filter);

void paragraph_paste_collision_filter(Paragraph *pa, cpShapeFilter filter);
cpShape *circle2polyshape(cpSpace *space, cpShape *inshape);
Vector2 camera2screen(Camera2D cam, Vector2 in);
Color color_by_index(int colornum);
void texture_save(Texture2D tex, const char *fname);
void register_all_functions(void);
// some_file.txt, .txt -> some_file
const char *extract_filename(const char *fname, const char *ext);
const char *rect2str(Rectangle rect);
void camera_reset();

// Проверяет на наличие суффикса вида _01 в строке и возвращает статическую
// строку без суффикса
const char * remove_suffix(const char *str);

const char *get_basename(const char *path);

typedef int (*QSortCmpFunc)(void *a, void *b);
typedef void (*QSortSwapFunc)(size_t index1, size_t index2, void *udata);

void koh_qsort_soa(
    void *arr, size_t nmemb, size_t size, 
    QSortCmpFunc cmp, QSortSwapFunc swap, void *udata
);


