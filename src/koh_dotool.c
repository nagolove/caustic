// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_dotool.h"
#include "koh_common.h"
#include "raymath.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh.h"
#include "koh_routine.h"
#include "raylib.h"
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <threads.h>
#include <unistd.h>

struct KeyboardKeyRec {
    int     key;
    char    *keyname;
};

struct KeyboardKeyRec keys[] = {
    // {{{
    { KEY_NULL, "" },
    { KEY_SPACE, "space" },
    { KEY_APOSTROPHE, "apostrophe" },
    { KEY_COMMA, "comma" },
    { KEY_MINUS, "minus" },
    { KEY_PERIOD, "period" },
    { KEY_SLASH, "slash" },
    { KEY_ZERO, "0" },
    { KEY_ONE, "1" },
    { KEY_TWO, "2" },
    { KEY_THREE, "3" },
    { KEY_FOUR, "4" },
    { KEY_FIVE, "5" },
    { KEY_SIX, "6" },
    { KEY_SEVEN, "7" },
    { KEY_EIGHT, "8" },
    { KEY_NINE, "9" },
    { KEY_SEMICOLON, "semicolon" },
    { KEY_EQUAL, "equal" },
    { KEY_A, "a" },
    { KEY_B, "b" },
    { KEY_C, "c" },
    { KEY_D, "d" },
    { KEY_E, "e" },
    { KEY_F, "f" },
    { KEY_G, "g" },
    { KEY_H, "h" },
    { KEY_I, "i" },
    { KEY_J, "j" },
    { KEY_K, "k" },
    { KEY_L, "l" },
    { KEY_M, "m" },
    { KEY_N, "n" },
    { KEY_O, "o" },
    { KEY_P, "p" },
    { KEY_Q, "q" },
    { KEY_R, "r" },
    { KEY_S, "s" },
    { KEY_T, "t" },
    { KEY_U, "u" },
    { KEY_V, "v" },
    { KEY_W, "w" },
    { KEY_X, "x" },
    { KEY_Y, "y" },
    { KEY_Z, "z" },
    { KEY_LEFT_BRACKET, "bracketleft" },
    { KEY_BACKSLASH, "backslash" },
    { KEY_RIGHT_BRACKET, "bracketright" },
    { KEY_GRAVE, "grave" },
    { KEY_ESCAPE, "escape" },
    { KEY_ENTER, "Return" },
    { KEY_TAB, "Tab" },
    { KEY_BACKSPACE, "Backspace" },
    { KEY_INSERT, "Insert" },
    { KEY_DELETE, "Delete" },
    { KEY_RIGHT, "Right" },
    { KEY_LEFT, "Left" },
    { KEY_DOWN, "Down" },
    { KEY_UP, "Up" },
    { KEY_PAGE_UP, "Prior" },
    { KEY_PAGE_DOWN, "Next" },
    { KEY_HOME, "Home" },
    { KEY_END, "End" },
    { KEY_CAPS_LOCK, "CAPS_LOCK" },
    //{ KEY_SCROLL_LOCK, "SCROLL_LOCK" },
    //{ KEY_NUM_LOCK, "NUM_LOCK" },
    { KEY_PRINT_SCREEN, "Print" },
    { KEY_PAUSE, "Pause" },
    { KEY_F1, "F1" },
    { KEY_F2, "F2" },
    { KEY_F3, "F3" },
    { KEY_F4, "F4" },
    { KEY_F5, "F5" },
    { KEY_F6, "F6" },
    { KEY_F7, "F7" },
    { KEY_F8, "F8" },
    { KEY_F9, "F9" },
    { KEY_F10, "F10" },
    { KEY_F11, "F11" },
    { KEY_F12, "F12" },
    { KEY_LEFT_SHIFT, "Shift_L" },
    { KEY_LEFT_CONTROL, "Control_L" },
    { KEY_LEFT_ALT, "Alt_L" },
    { KEY_LEFT_SUPER, "Super_L" },
    { KEY_RIGHT_SHIFT, "Shift_R" },
    { KEY_RIGHT_CONTROL, "Control_R" },
    { KEY_RIGHT_ALT, "Alt_R" },
    { KEY_RIGHT_SUPER, "Super_R" },
    { KEY_KB_MENU, "Menu" },
    //{ KEY_KP_0, "KP_0" },
    //{ KEY_KP_1, "KP_1" },
    //{ KEY_KP_2, "KP_2" },
    //{ KEY_KP_3, "KP_3" },
    //{ KEY_KP_4, "KP_4" },
    //{ KEY_KP_5, "KP_5" },
    //{ KEY_KP_6, "KP_6" },
    //{ KEY_KP_7, "KP_7" },
    //{ KEY_KP_8, "KP_8" },
    //{ KEY_KP_9, "KP_9" },
    //{ KEY_KP_DECIMAL, "KP_DECIMAL" },
    //{ KEY_KP_DIVIDE, "KP_DIVIDE" },
    //{ KEY_KP_MULTIPLY, "KP_MULTIPLY" },
    //{ KEY_KP_SUBTRACT, "KP_SUBTRACT" },
    //{ KEY_KP_ADD, "KP_ADD" },
    //{ KEY_KP_ENTER, "KP_ENTER" },
    //{ KEY_KP_EQUAL, "KP_EQUAL" },
    // }}}
};

// Добавить запись клавиатуры
struct InputState {
    Vector2 pos;
    bool    lb_down, rb_down, mb_down;
    bool    keys[128];
    double  timestamp;
};

struct dotool_ctx {
    Rectangle                   excluded_areas[16];
    int                         excluded_areas_num;
    pthread_cond_t              *condition;
    pthread_mutex_t             *mutex;
    const char                  *shm_name_mutex, *shm_name_cond;
    pthread_condattr_t          cond_attr;
    pthread_mutexattr_t         mutex_attr;
    Vector2                     dispacement;
    bool                        is_recording;
    _Atomic(bool)               is_saving;
    char                        script_fname[256];
    struct InputState           *rec, rec_prev;
    int                         rec_num, rec_cap;
    char                        *ini_data;
    struct FilesSearchResult    fsr_scripts;
};

static const char   *default_shm_name_mutex = "caustic_xdt_mutex",
                    *default_shm_name_cond = "caustic_xdt_cond";

static void update_scripts_list(dotool_ctx_t *ctx) {
    assert(ctx);
    koh_search_files_shutdown(&ctx->fsr_scripts);
    ctx->fsr_scripts = koh_search_files(".", "^dotool.*\\.txt$");
}

static const char *get_key(int key) {
    int low = 0;
    int high = sizeof(keys) / sizeof(keys[0]);
    while (low <= high) {
        int mid = (low + high) / 2;
        int value = keys[mid].key;
        
        if (value < key)
            low = mid + 1;
        else if (value > key)
            high = mid - 1;
        else {
            return keys[mid].keyname;
        }
    }
    return NULL;
}

// XXX: Сделать включение только для отладочного режима
static void check_keys_order() {
    int num = sizeof(keys) / sizeof(keys[0]);
    int prev_value = -1;
    for (int i = 0; i < num; i++) {
        if (prev_value >= keys[i].key) {
            trace(
                "check_keys_order: prev_value %d, keys[%d].key %d\n",
                prev_value, i, keys[i].key
            );
            abort();
        }
        prev_value = keys[i].key;
    }
}

static void dotool_record_first_tick(dotool_ctx_t *ctx) {
    assert(ctx);

    /*
    struct InputState *ms = &ctx->rec[ctx->rec_num];
    ms->lb_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    ms->rb_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    ms->mb_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    ms->pos = Vector2Add(GetMousePosition(), ctx->dispacement);
    ms->timestamp = GetTime();
    ctx->rec_prev = ctx->rec[0];
    ctx->rec_num++;
    */
}

static void check_realloc(dotool_ctx_t *ctx) {
    if (ctx->rec_num + 1 == ctx->rec_cap) {
        ctx->rec_cap += 1;
        ctx->rec_cap *= 1.5;
        size_t sz = sizeof(ctx->rec[0]) * ctx->rec_cap;
        void *new_ptr = realloc(ctx->rec, sz);
        assert(new_ptr);
        ctx->rec = new_ptr;
    }
}

static bool is_in_excluded_area(dotool_ctx_t *ctx, Vector2 mouse_pos) {
    assert(ctx);
    for (int i = 0; i < ctx->excluded_areas_num; i++) {
        /*
        trace(
            "is_in_excluded_area: i %d, rect %s\n",
            i, rect2str(ctx->excluded_areas[i])
        );
        // */
        if (CheckCollisionPointRec(mouse_pos, ctx->excluded_areas[i]))
            return true;
    }
    return false;
}

static void dotool_record_tick(dotool_ctx_t *ctx) {
    assert(ctx);
    trace("dotool_record_tick:\n");

    check_realloc(ctx);

    Vector2 mp = Vector2Add(GetMousePosition(), ctx->dispacement);
    if (!is_in_excluded_area(ctx, mp)) {
        struct InputState *ms = &ctx->rec[ctx->rec_num];
        ms->lb_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        ms->rb_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        ms->mb_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        //ms->pos = Vector2Add(mp, ctx->dispacement);
        ms->pos = mp;
        ms->timestamp = GetTime();
        if (ctx->rec_num) {
            ctx->rec_prev = ctx->rec[ctx->rec_num - 1];
        } else {
            ctx->rec_prev = ctx->rec[ctx->rec_num];
        }
        ctx->rec_num++;
    }
}

static void ipc_init(struct dotool_ctx *ctx) {
    ctx->shm_name_cond = default_shm_name_cond;
    ctx->shm_name_mutex = default_shm_name_mutex;
    int mode = S_IRWXU | S_IRWXG;

    int des_mutex = shm_open(
            ctx->shm_name_mutex, O_CREAT | O_RDWR | O_TRUNC, mode
            );

    if (des_mutex < 0) {
        trace("ipc_init: shm_open() failed on des_mutex\n");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(des_mutex, sizeof(pthread_mutex_t)) == -1) {
        trace("ipc_init: error on ftruncate to sizeof pthread_mutex_t\n");
        exit(EXIT_FAILURE);
    }

    ctx->mutex = (pthread_mutex_t*)mmap(
            NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED,
            des_mutex, 0
            );

    if (ctx->mutex == MAP_FAILED ) {
        trace("ipc_init: error on mmap on mutex\n");
        exit(EXIT_FAILURE);
    }

    int des_cond = shm_open(
            ctx->shm_name_cond, O_CREAT | O_RDWR | O_TRUNC, mode
            );

    if (des_cond < 0) {
        trace("ipc_init: shm_open() failed on des_cond\n");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(des_cond, sizeof(pthread_cond_t)) == -1) {
        trace("ipc_init: error on ftruncate to sizeof pthread_cond_t\n");
        exit(-1);
    }

    ctx->condition = (pthread_cond_t*)mmap(
            NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED,
            des_cond, 0
            );

    if (ctx->condition == MAP_FAILED ) {
        trace("ipc_init: error on mmap on condition\n");
        exit(EXIT_FAILURE);
    }

    /* set mutex shared between processes */
    pthread_mutexattr_setpshared(&ctx->mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(ctx->mutex, &ctx->mutex_attr);

    /* set condition shared between processes */
    pthread_condattr_setpshared(&ctx->cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(ctx->condition, &ctx->cond_attr);
}


struct dotool_ctx *dotool_new() {
    check_keys_order();
    struct dotool_ctx *ctx = calloc(1, sizeof(*ctx));
    assert(ctx);
    ipc_init(ctx);

    // 10min = 600 * 60 = 3600
    ctx->rec_cap = 600 * 60;
    ctx->rec = calloc(ctx->rec_cap, sizeof(ctx->rec[0]));
    assert(ctx->rec);

    return ctx;
}

static void ipc_shutdown(struct dotool_ctx *ctx) {
    pthread_condattr_destroy(&ctx->cond_attr);
    pthread_mutexattr_destroy(&ctx->mutex_attr);
    pthread_mutex_destroy(ctx->mutex);
    pthread_cond_destroy(ctx->condition);

    munmap(ctx->mutex, sizeof(*ctx->mutex));
    munmap(ctx->condition, sizeof(*ctx->condition));
    shm_unlink(ctx->shm_name_cond);
    shm_unlink(ctx->shm_name_mutex);
}

void dotool_free(struct dotool_ctx *ctx) {
    if (!ctx)
        return;

    if (ctx->ini_data) {
        free(ctx->ini_data);
        ctx->ini_data = NULL;
    }
    ipc_shutdown(ctx);
    if (ctx->rec) {
        ctx->rec_cap = ctx->rec_num = 0;
        free(ctx->rec);
        ctx->rec = NULL;
    }
    free(ctx);
}

static void read_gui_ini(const char *script_fname) {
    char ini_fname[512] = {};
    strcat(ini_fname, script_fname);
    strcat(ini_fname, ".imgui");
    /*
    trace("read_gui_ini: ini_fname %s\n", ini_fname);
    FILE *file = fopen(ini_fname, "r");
    if (!file) 
        return;
    trace("read_gui_ini: reading from '%s'\n", ini_fname);
    fseek(file, 0, SEEK_END);
    int fsize = ftell(file);
    trace("read_gui_ini: fsize %d\n", fsize);
    char *ini_data = calloc(1, fsize + 1);
    assert(ini_data);
    rewind(file);
    int items_num = fread(ini_data, 1, fsize, file);
    printf("read_gui_ini: ini_data '%s'\n", ini_data);
    trace("read_gui_ini: fsize %d, items_num %d\n", fsize, items_num);
    assert(items_num == fsize);
    igLoadIniSettingsFromMemory(ini_data, fsize);
    fclose(file);
    free(ini_data);
    //*/
    igLoadIniSettingsFromDisk(ini_fname);
}

void dotool_send_signal(struct dotool_ctx *ctx) {
    assert(ctx);

    if (strlen(ctx->script_fname))
        read_gui_ini(ctx->script_fname);
    
    pthread_mutex_lock(ctx->mutex);
    pthread_cond_signal(ctx->condition);
    pthread_mutex_unlock(ctx->mutex);

    trace("dotool_send_signal: wake up\n");
}

void dotool_exec_script(struct dotool_ctx *ctx, const char *script_fname) {
    assert(ctx);
    trace("dotool_exec_script: script_fname %s\n", script_fname);

    //read_gui_ini(script_fname);
    strncpy(ctx->script_fname, script_fname, sizeof(ctx->script_fname));

    pid_t ret = fork();
    if (ret == -1) {
        trace("dotool_exec_script: fork() failed\n");
        exit(EXIT_FAILURE);
    }

    if (!ret) {
        // Ждать пока приложение не будет готово к взаимодействию с 
        // устройствами ввода
        pthread_mutex_lock(ctx->mutex);
        pthread_cond_wait(ctx->condition, ctx->mutex);
        pthread_mutex_unlock(ctx->mutex);

        trace("dotool_exec_script: before execve\n");
        const char *abs_path = realpath(script_fname, NULL);
        trace("dotool_exec_script: abs_path %s\n", abs_path);
        const char *xdotool_path = "/usr/bin/xdotool";

        execve(
            xdotool_path,
            (char *const []){ 
                (char*)xdotool_path,
                (char*)abs_path,
                NULL 
            },
            __environ
        );
    }
}

// TODO: Исключить запись при нахождении мыши в окне dotool_gui()
void dotool_gui(struct dotool_ctx *ctx) {
    static const char *fname = NULL;
    ImGuiWindowFlags wnd_flags = 0;
    static bool open = true;
    igBegin("xdotool scripts recorder", &open, wnd_flags);

    ImVec2 wnd_pos, wnd_size;
    igGetWindowPos(&wnd_pos);
    igGetWindowSize(&wnd_size);
    ctx->excluded_areas[0] = (Rectangle) {
        .x = wnd_pos.x + ctx->dispacement.x,
        .y = wnd_pos.y + ctx->dispacement.y,
        .width = wnd_size.x,
        .height = wnd_size.y,
    };
    ctx->excluded_areas_num = 1;

    /*if (igButton("⏺", (ImVec2){})) {*/
    if (igButton("rec", (ImVec2){})) {
        open = false;
        dotool_record_start(ctx);
    }
    igSameLine(0., 5.);
    /*if (igButton("⏹", (ImVec2){})) {*/
    if (igButton("stop", (ImVec2){})) {
        open = true;
        dotool_record_stop(ctx);
    }
    igSameLine(0., 5.);
    // TODO: Сохранение в файл с инкрементальным именем
    if (igButton("save", (ImVec2){})) {
        if (!dotool_is_saving(ctx)) {
            fname = koh_incremental_fname("dotool", "txt");
            dotool_record_save(ctx, fname);
        }
    }
    if (ctx->is_recording) {
        igText("recoring .. %d ticks", ctx->rec_num);
    }
    if (dotool_is_saving(ctx)) {
        igText("saving to '%s'", fname);
    }

    /*
    if (igButton("save ini", (ImVec2){})) {
        igSaveIniSettingsToMemory(
    }
    */

    igEnd();
}

void dotool_setup_display(dotool_ctx_t *ctx) {
    assert(ctx);
    trace("dotool_setup_display:\n");
    int count = GetMonitorCount();
    trace("dotool_setup_display: count %d\n", count);
    trace("dotool_setup_display: current %d\n", GetCurrentMonitor());
    for (int monitor = 0; monitor < count; monitor++) {
        Vector2 pos = GetMonitorPosition(monitor);
        trace(
            "dotool_setup_display: monitor %d position %s\n", 
            monitor, Vector2_tostr(pos)
        );

        trace(
            "dotool_setup_display: width %d\n", GetMonitorWidth(monitor)
        );

        // XXX: Странное предупреждение от gcc
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
        trace(
            "dotool_setup_display: height %d\n", GetMonitorHeight(monitor)
        );
#pragma GCC diagnostic push

        trace(
            "dotool_setup_display: phys width %d\n",
            GetMonitorPhysicalWidth(monitor)
        );
        trace(
            "dotool_setup_display: phys height %d\n",
            GetMonitorPhysicalHeight(monitor)
        );
    }

    ctx->dispacement = GetMonitorPosition(GetCurrentMonitor());
}

void dotool_update(dotool_ctx_t *ctx) {
    assert(ctx);
    if (ctx->is_recording) {
        dotool_record_tick(ctx);
    }
}

void dotool_record_start(dotool_ctx_t *ctx) {
    trace("dotool_record_start:\n");
    if (ctx->is_recording) {
        trace("dotool_record_start: already recording\n");
        return;
    }
    ctx->rec_num = 0;
    ctx->is_recording = true;
    if (ctx->ini_data) {
        free(ctx->ini_data);
        ctx->ini_data = NULL;
    }
    ctx->ini_data = strdup(igSaveIniSettingsToMemory(NULL));
    dotool_record_first_tick(ctx);
}

void dotool_record_stop(dotool_ctx_t *ctx) {
    trace("dotool_record_stop:\n");
    assert(ctx);
    ctx->is_recording = false;
}

static void write_gui_ini(const dotool_ctx_t *ctx, const char *fname) {
    char buf[256] = {};
    strcat(buf, fname);
    strcat(buf, ".imgui");
    FILE *file = fopen(buf, "w");
    if (!file)
        return;
    int sz = strlen(ctx->ini_data);
    int items_num = fwrite(ctx->ini_data, sz, 1, file);
    //trace("write_gui_ini: sz %d, items_num %d\n", sz, items_num);
    assert(1 == items_num);
    fclose(file);
}

void _dotool_record_save(dotool_ctx_t *ctx, const char *fname) {
    trace("_dotool_record_save:\n");
    assert(ctx);
    assert(fname);
    FILE *fdest = fopen(fname, "w");
    if (!fdest) {
        trace("_dotool_record_save: could not open file '%s'\n", fname);
        return;
    }
    Vector2 prev_pos = ctx->rec[0].pos;
    double prev_time = ctx->rec[0].timestamp;
    
    fprintf(
        fdest, "mousemove -- %d %d\n", (int)prev_pos.x, (int)prev_pos.y
    );

    for (int i = 0; i < ctx->rec_num; ++i) {
        struct InputState *cur = &ctx->rec[i];
        trace("_dotool_record_save: prev_pos %s\n", Vector2_tostr(prev_pos));
        trace(
            "_dotool_record_save: ctx->rec[i].pos %s\n",
            Vector2_tostr(cur->pos)
        );
        Vector2 mouse_pos_d = Vector2Subtract(prev_pos, cur->pos);
        double time_d = cur->timestamp - prev_time;
        fprintf(
            fdest, "mousemove_relative -- %d %d\n",
            -(int)mouse_pos_d.x, -(int)mouse_pos_d.y
        );
        fprintf(fdest, "sleep %.2f\n", time_d);

        if (cur->lb_down)
            fprintf(fdest, "mousedown 1\n");
        else
            fprintf(fdest, "mouseup 1\n");
        if (cur->rb_down)
            fprintf(fdest, "mousedown 3\n");
        else
            fprintf(fdest, "mouseup 3\n");
        if (cur->mb_down)
            fprintf(fdest, "mousedown 2\n");
        else
            fprintf(fdest, "mouseup 2\n");
        prev_pos = cur->pos;
        prev_time = cur->timestamp;
    }

    fclose(fdest);
    write_gui_ini(ctx, fname);
}

struct ThreadCtx {
    dotool_ctx_t    *ctx;
    char            fname[128];
};

static int thread_save(void *arg) {
    assert(arg);
    struct ThreadCtx *thread_ctx = arg;
    assert(thread_ctx->ctx);
    thread_ctx->ctx->is_saving = true;
    _dotool_record_save(thread_ctx->ctx, thread_ctx->fname);
    thread_ctx->ctx->is_saving = false;
    return 0;
}

// Функцию нельзя запустить пока она не закончила сохранение
void dotool_record_save(dotool_ctx_t *ctx, const char *fname) {
    assert(ctx);
    assert(fname);

    bool is_saving = atomic_load(&ctx->is_saving);
    if (is_saving) {
        trace("dotool_record_save: already saving\n");
        return;
    }

    thrd_t thread_saver;
    static struct ThreadCtx thread_ctx;
    thread_ctx.ctx = ctx;
    strncpy(thread_ctx.fname, fname, sizeof(thread_ctx.fname));
    thrd_create(&thread_saver, thread_save, &thread_ctx);
}

bool dotool_is_saving(dotool_ctx_t *ctx) {
    assert(ctx);
    return atomic_load(&ctx->is_saving);
}
