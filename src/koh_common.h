// vim: fdm=marker
#pragma once

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"

#include "koh_paragraph.h"
#include "koh_rand.h"
#include "koh_metaloader.h"
#include "koh_visual_tools.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_types.h"
/*#include "enet.h"*/
#include "raylib.h"

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

#define KOH_FORCE_INLINE inline __attribute__((always_inline))
#define KOH_ATTR_FORMAT(f, s) __attribute__((__format__(__printf__, f, s))) 

struct Common {
    bool quit;

    int *font_chars;
    int font_chars_num, font_chars_cap;

    /* Ссылка на Луа таблицу вида
        registered_functions = {
            "func_name" = "description",
        }
    */
};

struct Common *koh_cmn();

void koh_common_init(void);
void koh_common_shutdown(void);

//Color interp_color(Color a, Color b, float t);
//Color height_color(float value);

const char *color2str(Color color);
const char *Color_to_str(Color color);
Font load_font_unicode(const char *fname, int size);

struct SpaceShutdownCtx {
    cpSpace *space;
    bool free_shapes, free_constraints, free_bodies;
    bool print_shapes, print_constraints, print_bodies;
};

void space_shutdown(struct SpaceShutdownCtx ctx);
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
const char *to_bitstr_uint8_t(uint8_t value);
// не выделяет памяти под строку, статический буфер в функции
const char *to_bitstr_uint32_t(uint32_t value);
const char *to_bitstr_uint64_t(uint64_t value);

int u8_codeptlen(const char *str);
void save_scripts(FILE *file);
void bb_draw(cpBB bb, Color color);
Vector2 random_inrect(xorshift32_state *st, Rectangle rect);
Vector2 random_outrect_quad(
    xorshift32_state *st, Vector2 start, int w, int border_width
);
Vector2 bzr_cubic(Vector2 segments4[4], float t);
const char *shapefilter2str(cpShapeFilter filter);

void paragraph_paste_collision_filter(Paragraph *pa, cpShapeFilter filter);
cpShape *circle2polyshape(cpSpace *space, cpShape *inshape);
cpShape *make_circle_polyshape(cpBody *body, float radius, cpTransform *tr);

// Возвращает вектор смещение камеры плюс вектор in 
Vector2 camera2screen(Camera2D cam, Vector2 in);

// Возвращает один из стандартных цветов raylib по числовому индексу.
// Если индекс меньше нуля или больше color_max_index(), то возвращает
// прозрачный цвет - BLANK
Color color_by_index(int colornum);
// Возвращает максимальное значение индекса для прошлой функции
int color_max_index();

void texture_save(Texture2D tex, const char *fname);

// Применение:
// extract_filename("/some/file/path/some_file.txt", ".txt") -> "some_file"
// extract_filename("some_file.txt", ".txt") -> "some_file"
// extract_filename("some_file", ".txt") -> "some_file"
const char *extract_filename(const char *fname, const char *ext);

const char *rect2str(Rectangle rect);
Rectangle rect_from_arr(const float xywh[4]);

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

cpSpaceDebugColor from_Color(Color c);

struct CameraProcessScale {
    Camera2D    *cam;
    float       dscale_value;       // Приращение масштаба
    KeyboardKey modifier_key_down;
};

struct CameraProcessDrag {
    int         mouse_btn;
    Camera2D    *cam;
};

bool koh_camera_process_mouse_drag(struct CameraProcessDrag *cpd);
bool koh_camera_process_mouse_scale_wheel(struct CameraProcessScale *cps);
//bool koh_camera_process_mouse_scale_wheel(Camera2D *cam, float dscale_value);
//bool koh_color_eq(Color c1, Color c2);

struct CameraAxisDrawCtx {
    Color color_offset, color_target;
    Font  *fnt;
    int   fnt_size;
};

void draw_camera_axis(Camera2D *cam, struct CameraAxisDrawCtx ctx);
const char *transform2str(cpTransform tr);
const char *camera2str(Camera2D cam, bool multiline);
Color random_raylib_color();
uint32_t next_eq_pow2(uint32_t p);

const char *font2str(Font fnt);
void koh_screenshot_incremental();
void koh_trap();

#define KOH_TERM_BLACK        30         
#define KOH_TERM_RED          31         
#define KOH_TERM_GREEN        32         
#define KOH_TERM_YELLOW       33         
#define KOH_TERM_BLUE         34         
#define KOH_TERM_MAGENTA      35         
#define KOH_TERM_CYAN         36         
#define KOH_TERM_WHITE        37         

void koh_term_color_set(int color);
void koh_term_color_reset();

/*
   Разбирает строчку вида " [5 6]    [ 1  2][0  3 ] [ 7 8 ]"
   Не больше 128 пробелов между цифрами.
   Возвращает количество найденных пар и два указателя на массивы с первыми 
   числами и вторыми числами. 
   Память по указателям first и seconf требует освобождения.
*/
void parse_bracketed_string(
    const char *str, int **first, int **second, int *len
);

typedef struct FilesSearchResult {
    struct FilesSearchResultInternal    *internal;
    char                                *path, *regex_pattern;
    char                                **names;
    int                                 num, capacity;
} FilesSearchResult;

typedef struct FilesSearchSetup {
    const char  *path,          // относительный или абсолютный?
                *regex_pattern;
    int     deep; // глубина поиска
                  // -1 - неограниченная, 0 - без захода в подкаталоги
} FilesSearchSetup;

// Возвращает указатель на статискую строчку
char *koh_files_search_setup_2str(struct FilesSearchSetup *setup);
struct FilesSearchResult koh_search_files(struct FilesSearchSetup *setup);
void koh_search_files_shutdown(struct FilesSearchResult *fsr);
void koh_search_files_print(struct FilesSearchResult *fsr);
void koh_search_files_exclude_pcre(
    struct FilesSearchResult *fsr, const char *exclude_pattern
);

enum VisualToolMode visual_tool_mode2metaloader_type(
    const enum MetaLoaderType type
);
void koh_try_ray_cursor();
// Возвращает указатель на статический внутренний буфер
const char *koh_incremental_fname(const char *fname, const char *ext);

// Возвращает указатель на статический буффер.
const char *koh_extract_path(const char *fname);

int koh_cpu_count();
bool koh_is_pow2(int n);
int koh_less_or_eq_pow2(int n);

struct IgWindowState {
    Vector2 wnd_pos, wnd_size;
};

void koh_window_post();
const struct IgWindowState *koh_window_state();
void koh_window_state_print();
bool koh_window_is_point_in(Vector2 point, Camera2D *cam);

void koh_backtrace_print();

char *Vector2_tostr_alloc(const Vector2 *verts, int num);

char **Texture2D_to_str(Texture2D *tex);
Rectangle rect_by_texture(Texture2D tex);

// Принимает массив строк завершающийся NULL. Возвращает указатель на
// склеенные строки. Нужно вызывать free()
char *concat_iter_to_allocated_str(char **lines);
bool koh_file_exist(const char *fname);

static inline char *r_sprintf(char* buffer, const char* format, ...) {
    assert(buffer);
    assert(format);

    va_list args1;
    va_start(args1, format);
    va_list args2;
    va_copy(args2, args1);
    va_end(args1);
    /*vsnprintf(buf, sizeof buf, fmt, args2);*/
    vsprintf(buffer, format, args2);
    va_end(args2);

    return buffer;
}

bool rgexpr_match(const char *str, size_t *str_len, const char *pattern);
bool koh_check_fname(const char *fname);

char *points2table_allocated(const Vector2 *points, int points_num);
const char *koh_str_gen_aA(size_t len);
