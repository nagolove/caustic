// vim: fdm=marker
#pragma once

#include "koh_metaloader.h"
#include "koh_rand.h"
#include "koh_timerman.h"
#include "koh_visual_tools.h"
#include "raylib.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

//////////////////////////// отключить предупреждения при снятии константности
#if defined(__GNUC__) || defined(__clang__)
#define DIAG_PUSH_WCASTQUAL_OFF \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#define DIAG_POP \
    _Pragma("GCC diagnostic pop")
#else
#define DIAG_PUSH_WCASTQUAL_OFF
#define DIAG_POP
#endif


//////////////////// NORETURN
// кросс-языковая обёртка
#if __STDC_VERSION__ >= 201112L
#define NORETURN _Noreturn
#elif defined(__cplusplus) && __cplusplus >= 201103L
#define NORETURN [[noreturn]]
#elif defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
#else
#define NORETURN
#endif
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
/////////////////////

/////////////////// THREAD_LOCAL
#if __STDC_VERSION__ >= 201112L
#define THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define THREAD_LOCAL __thread
#else
#define THREAD_LOCAL /* no TLS */
#endif
///////////////////

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

#ifndef __attribute_deprecated__
#define __attribute_deprecated__  __attribute__((deprecated))
#endif

#define KOH_FORCE_INLINE inline __attribute__((always_inline))
#define KOH_INLINE inline __attribute__((always_inline))
#define KOH_ATTR_FORMAT(f, s) __attribute__((__format__(__printf__, f, s))) 
#define KOH_HIDDEN __attribute__((visibility("hidden")))

typedef int64_t     i64;
typedef uint64_t    u64;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int16_t     i16;
typedef uint16_t    u16;
typedef int8_t      i8;
typedef uint8_t     u8;
typedef float       f32;
typedef double      f64;

typedef struct { 
    f32 x, y; 
} v2;

struct Common {
    bool    quit;
    int     *font_chars;
    int     font_chars_num, font_chars_cap;
};

struct Common *koh_cmn();

void koh_common_init(void);
void koh_common_shutdown(void);

//Color interp_color(Color a, Color b, float t);
//Color height_color(float value);

const char *color2str(Color c);
const char *Color_to_str(Color c);
Font load_font_unicode(const char *fname, int size);

// -1..1 -> 0..1
float axis2zerorange(float value);

// TODO: Сделать слоты на кольцевом буфере для статической памяти
// не выделяет памяти под строку, статический буфер в функции
const char *to_bitstr_uint8_t(uint8_t value);
// не выделяет памяти под строку, статический буфер в функции
const char *to_bitstr_uint32_t(uint32_t value);
const char *to_bitstr_uint64_t(uint64_t value);

int u8_codeptlen(const char *str);
//void bb_draw(cpBB bb, Color color);
Vector2 random_inrect(xorshift32_state *st, Rectangle rect);
Vector2 random_outrect_quad(
    xorshift32_state *st, Vector2 start, int w, int border_width
);
Vector2 bzr_cubic(Vector2 segments4[4], float t);
//const char *shapefilter2str(cpShapeFilter filter);

//void paragraph_paste_collision_filter(Paragraph *pa, cpShapeFilter filter);
//cpShape *circle2polyshape(cpSpace *space, cpShape *inshape);
//cpShape *make_circle_polyshape(cpBody *body, float radius, cpTransform *tr);

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

const char *get_basename(char *path);

typedef int (*QSortCmpFunc)(void *a, void *b);
typedef void (*QSortSwapFunc)(size_t index1, size_t index2, void *udata);

void koh_qsort_soa(
    void *arr, size_t nmemb, size_t size, 
    QSortCmpFunc cmp, QSortSwapFunc swap, void *udata, 
    bool reverse
);

//cpSpaceDebugColor from_Color(Color c);

typedef struct CameraProcessScale {
    Camera2D    *cam;
    float       dscale_value;       // Приращение масштаба
    KeyboardKey modifier_key_down, 
                // speed * 2.
                boost_modifier_key_down;
} CameraProcessScale;

typedef struct CameraProcessDrag {
    int         mouse_btn;
    Camera2D    *cam;
} CameraProcessDrag;

typedef struct CameraAutomat {
    Camera2D *cam;
    TimerMan *tm;
    double   last_scroll[64];
    int      i;
    float    dscale_value;
} CameraAutomat;

void cam_auto_init(CameraAutomat *ca, Camera2D *cam);
void cam_auto_shutdown(CameraAutomat *ca);
void cam_auto_update(CameraAutomat *ca);

// TODO: Добавить разные интерполяции
// TODO: Добавить гуи для изменения параметров
// TODO: Добавить сохранение в луа строку
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
//const char *transform2str(cpTransform tr);
const char *camera2str(Camera2D cam, bool multiline);
Color random_raylib_color();
uint32_t next_eq_pow2(uint32_t p);

const char *font2str(Font fnt);
void koh_screenshot_incremental();
void koh_trap();

// Цвета терминала {{{
#define KOH_TERM_BLACK        30         
#define KOH_TERM_RED          31         
#define KOH_TERM_GREEN        32         
#define KOH_TERM_YELLOW       33         
#define KOH_TERM_BLUE         34         
#define KOH_TERM_MAGENTA      35         
#define KOH_TERM_CYAN         36         
#define KOH_TERM_WHITE        37         

#define KOH_TERM_BRIGHT_BLACK        90         
#define KOH_TERM_BRIGHT_RED          91         
#define KOH_TERM_BRIGHT_GREEN        92         
#define KOH_TERM_BRIGHT_YELLOW       93         
#define KOH_TERM_BRIGHT_BLUE         94         
#define KOH_TERM_BRIGHT_MAGENTA      95         
#define KOH_TERM_BRIGHT_CYAN         96         
#define KOH_TERM_BRIGHT_WHITE        97         
// }}}

void koh_term_color_set(int color);
void koh_term_color_reset();

/*
   Разбирает строчку вида " [5 6]    [ 1  2][0  3 ] [ 7 8 ]"
   Не больше 128 пробелов между цифрами.
   Возвращает количество найденных пар и два указателя на массивы с первыми 
   числами и вторыми числами. 
   Память по указателям first и seconf требует освобождения.
*/
void parse_bracketed_string_alloc(
    const char *str, int **first, int **second, int *len
);

typedef struct FilesSearchResult {
    struct FilesSearchResultInternal    *internal;
    char                                *path, *regex_pattern;
    char                                **names;
    int                                 num, capacity;
    void                                *udata;
    void (*on_search_begin)(struct FilesSearchResult *fsr);
    bool (*on_search)(struct FilesSearchResult *fsr, const char *fname);
    void (*on_search_end)(struct FilesSearchResult *fsr);
    void (*on_shutdown)(struct FilesSearchResult *fsr);
} FilesSearchResult;

typedef struct FilesSearchSetup {
    const char  *path,          // относительный или абсолютный?
                *regex_pattern;
    int     deep; // глубина поиска
                  // -1 - неограниченная, 0 - без захода в подкаталоги
    void    *udata; // Данные пользователя
    // Вызывается в начале поиска
    void (*on_search_begin)(struct FilesSearchResult *fsr);
    // Возвращает истину если файл соответствует критериям поиска
    bool (*on_search)(struct FilesSearchResult *fsr, const char *fname);
    // Вызывается после завершения поиска
    void (*on_search_end)(struct FilesSearchResult *fsr);
    // Вызывается при удалении структуры поиск(koh_search_files_shutdown)
    void (*on_shutdown)(struct FilesSearchResult *fsr);
} FilesSearchSetup;

// Возвращает указатель на статискую строчку
char *koh_files_search_setup_2str(FilesSearchSetup *setup);
// Заполняет структуру.
// TODO: Возможность поиска в отдельном потоке
FilesSearchResult koh_search_files(FilesSearchSetup *setup);
// Освобождает память, зануляет содержимое структуры. 
// Можно вызывать несколько раз.
void koh_search_files_shutdown(struct FilesSearchResult *fsr);

// Добавляет в список out файлы из add. Результат без дубликатов.
// Истина в случае успеха.
bool koh_search_files_concat(FilesSearchResult *out, FilesSearchResult add);

void koh_search_files_print(struct FilesSearchResult *fsr);

void koh_search_files_print2(
    struct FilesSearchResult *fsr,
    int (*print_fnc)(const char *fmt, ...)
);
void koh_search_files_print3(
    struct FilesSearchResult *fsr,
    int (*print_fnc)(void *udata, const char *fmt, ...),
    void *udata
);

void koh_search_files_exclude_pcre(
    struct FilesSearchResult *fsr, const char *exclude_pattern
);

enum VisualToolMode visual_tool_mode2metaloader_type(
    const enum MetaLoaderType type
);
void koh_try_ray_cursor();
// Возвращает указатель на статический внутренний буфер
const char *koh_incremental_fname(const char *fname, const char *ext);

// Возвращает указатель на статический буффер с путем до файла, без имени.
// Путь может содержать '/' в конце. 
// [XXX: Может-ли? При fname = "/text.txt"]
const char *koh_extract_path(const char *fname);

int koh_cpu_count();
bool koh_is_pow2(int n);
int koh_less_or_eq_pow2(int n);

struct IgWindowState {
    Vector2 wnd_pos, wnd_size;
};

// XXX: Зачем нужны следущие функции?
void koh_window_post();
const struct IgWindowState *koh_window_state();
void koh_window_state_print();
bool koh_window_is_point_in(Vector2 point, Camera2D *cam);

void koh_backtrace_print();
// Возвращает указатель на статический массив со стеком вызовов.
// XXX: Как получить названия функций, а не только адреса?
// XXX: После вызова данной функции могут быть проблемы с дальнейшим 
// выполнением программы и освобождением памяти
const char *koh_backtrace_get();

// Возвращает Луа строку { {x0, y0}, {x1, y1}, ...}
char *Vector2_tostr_alloc(const Vector2 *verts, int num);

char **Texture2D_to_str(Texture2D *tex);
// Возвращает размер текстуры
Rectangle rect_by_texture(Texture2D tex);
/*Rectangle rect_by_texture1(Texture2D tex);*/

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

__attribute_deprecated__
bool rgexpr_match(const char *str, size_t *str_len, const char *pattern);
// str_len может быть равна NULL, тогда будет вызвана функция strlen(str)
bool koh_str_match(const char *str, size_t *str_len, const char *pattern);

// Проверяет окончивается ли имя файла на
// ".png", ".jpg", ".tga", ".PNG", ".JPG", ".TGA"
bool koh_is_fname_image_ext(const char *fname);

char *points2table_alloc(const Vector2 *points, int points_num);

// Возвращает указатель на статический буфер заполненный символами в 
// диапазоне 'a'-'A'.
// В случае неудачи из-за нехватки буфера возвращает NULL
const char *koh_str_gen_aA(size_t len);

// XXX: Функция протестирована?
char *koh_str_sub_alloc(
    const char *subject, const char* pattern, const char *replacement
);

extern bool common_verbose;

// Вспомогательная функция для получения сообщения об ошибке компиляции 
// регулярного выражения из числового кода.
// Возвращает указатель на статическую память.
char *pcre_code_str(int errnumber);

const char *koh_bin2hex(const void *data, size_t data_len);

// Возвращает массив целых случайных чисел. Максимальное значение rand() % up
// Длина массива num
// Память необходимо освобождать
// Максимальное значение num - INT16_MAX
int *koh_rand_uniq_arr_alloc(int up, int num);

// С вероятностью 50% вернет ложь или истину. Внутри - rand()
bool koh_maybe();

// Быстрая сортировка, аналогичная qsort(), но с передачей в функцию сравнения
// указателя на данные пользователя.
void koh_qsort(
    void *arr, 
    int num, int size,
    int (*cmp)(const void *a, const void *b, void *ud),
    void *ud
);

// Возвращает строковое имя цвета, цвета используются только из raylib.h
// Если значение не найдено, возвращается "unknown"
const char *Color2name(Color c);

typedef struct CoSysOpts {
    Rectangle dest;
    int       step;
    Color     color;
} CoSysOpts;

// Рисовать сетку для системы координат или чего-то такого.
void cosys_draw(CoSysOpts opts);

bool koh_file_read_alloc(const char *fname, char **data, size_t *data_len);
bool koh_file_write(const char *fname, const char *data, size_t data_len);
__attribute_deprecated__
void rect2uv(Rectangle rect, Vector2 uv[4]);
void set_uv_from_rect(Rectangle rect, Vector2 uv[4]);

// Установить текстурные координаты для вырезания прямоугольника
void set_uv1(Vector2 uv[4]);
void set_uv1_inv_y(Vector2 uv[4]);

// TODO: Сделать сохранение в файл, что-бы работало не только в WASM
const char *local_storage_load(const char *key);
void local_storage_save(const char *key, const char *value);

// Возвращает статическую строку c именем файла из префикса, даты+времени и
// суффикса. Используется для уникальных имен файлов.
const char *koh_uniq_fname_str(const char *prefix, const char *suffix);

// Переводит небольшой массив чисел в плавающей точкой в строку содержащую
// Луа таблицу. Возвращает указатель на статическую память.
// Если строка не умещается в буфер предназначенный для возврата, то функция
// возвращает NULL
const char *float_arr_tostr(const float *arr, size_t num);

// Работает аналогично float_arr_tostr().
// Возвращает таблицу с таблицами { {0.f, 0.f}, {1.f, 1.f} } и так далле, что 
// может быть не очень хорошо для некоторых случаев.
// Аналогично points2table_alloc(), но работает с небольшим внутренним 
// статическим буфером.
const char *Vector2_arr_tostr(Vector2 *arr, size_t num);

static inline void em_setup_screen_size(int *_w, int *_h) {
#ifdef __wasm__
    assert(_w);
    assert(_h);

    double w = 0, h = 0;
    // Получаем размер canvas в CSS-пикселях
    emscripten_get_element_css_size("#canvas", &w, &h);

    *_w = (int)w;
    *_h = (int)h;
#endif
}

#define assume(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

int get_hardware_concurrency();

// Примитивный многочлен: x^64 + x^4 + x^3 + x + 1
#define LFSR64_POLY 0x000000000000001B

// static uint64_t lfsr = 1; // начальное состояние (≠ 0)
static inline u64 lfsr64_next(u64 lfsr) {
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & LFSR64_POLY);
    return lfsr;
}

bool bit_calculator_gui(const char *caption, u64 *value);
const char *uint64_to_str_bin(uint64_t value);

NORETURN void koh_fatal();


#if defined(__GNUC__) || defined(__clang__)
#  define KOH_UNUSED __attribute__((unused))
#else
#  define KOH_UNUSED
#endif
