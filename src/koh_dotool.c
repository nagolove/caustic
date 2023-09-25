#include "koh_dotool.h"
#include "koh_routine.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "raylib.h"
#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

// Добавить запись клавиатуры
struct MouseState {
    Vector2 pos;
    bool    lb_down, rb_down, mb_down;
    double  timestamp;
};

struct dotool_ctx {
    pthread_cond_t      *condition;
    pthread_mutex_t     *mutex;
    const char          *shm_name_mutex, *shm_name_cond;
    pthread_condattr_t  cond_attr;
    pthread_mutexattr_t mutex_attr;
    Vector2             dispacement;
    bool                is_recording;
    char                status_msg[256];
    struct MouseState   *rec, rec_prev;
    int                 rec_num, rec_cap;
};

static const char   *default_shm_name_mutex = "caustic_xdt_mutex",
                    *default_shm_name_cond = "caustic_xdt_cond";

static void dotool_record_first_tick(dotool_ctx_t *ctx) {
    assert(ctx);

    /*
    struct MouseState *ms = &ctx->rec[ctx->rec_num];
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

static void dotool_record_tick(dotool_ctx_t *ctx) {
    assert(ctx);
    trace("dotool_record_tick:\n");

    check_realloc(ctx);

    struct MouseState *ms = &ctx->rec[ctx->rec_num];
    ms->lb_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    ms->rb_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    ms->mb_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    ms->pos = Vector2Add(GetMousePosition(), ctx->dispacement);
    ms->timestamp = GetTime();
    if (ctx->rec_num) {
        ctx->rec_prev = ctx->rec[ctx->rec_num - 1];
    } else {
        ctx->rec_prev = ctx->rec[ctx->rec_num];
    }
    ctx->rec_num++;
}

static void ipc_init(struct dotool_ctx *ctx) {
    ctx->shm_name_cond = default_shm_name_cond;
    ctx->shm_name_mutex = default_shm_name_mutex;
    int mode = S_IRWXU | S_IRWXG;

    int des_mutex = shm_open(
            ctx->shm_name_mutex, O_CREAT | O_RDWR | O_TRUNC, mode
            );

    if (des_mutex < 0) {
        trace("dotool_new: shm_open() failed on des_mutex\n");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(des_mutex, sizeof(pthread_mutex_t)) == -1) {
        trace("dotool_new: error on ftruncate to sizeof pthread_mutex_t\n");
        exit(EXIT_FAILURE);
    }

    ctx->mutex = (pthread_mutex_t*)mmap(
            NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED,
            des_mutex, 0
            );

    if (ctx->mutex == MAP_FAILED ) {
        trace("dotool_new: error on mmap on mutex\n");
        exit(EXIT_FAILURE);
    }

    int des_cond = shm_open(
            ctx->shm_name_cond, O_CREAT | O_RDWR | O_TRUNC, mode
            );

    if (des_cond < 0) {
        trace("dotool_new: shm_open() failed on des_cond\n");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(des_cond, sizeof(pthread_cond_t)) == -1) {
        trace("dotool_new: error on ftruncate to sizeof pthread_cond_t\n");
        exit(-1);
    }

    ctx->condition = (pthread_cond_t*)mmap(
            NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED,
            des_cond, 0
            );

    if (ctx->condition == MAP_FAILED ) {
        trace("dotool_new: error on mmap on condition\n");
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

    ipc_shutdown(ctx);
    if (ctx->rec) {
        ctx->rec_cap = ctx->rec_num = 0;
        free(ctx->rec);
        ctx->rec = NULL;
    }
    free(ctx);
}

void dotool_send_signal(struct dotool_ctx *ctx) {
    assert(ctx);

    pthread_mutex_lock(ctx->mutex);
    pthread_cond_signal(ctx->condition);
    pthread_mutex_unlock(ctx->mutex);

    trace("dotool_send_signal: wake up\n");
}

void dotool_exec_script(struct dotool_ctx *ctx, const char *script_fname) {
    assert(ctx);
    trace("dotool_exec_script: script_fname %s\n", script_fname);

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

static const char *incremental_name(const char *fname, const char *ext) {
    trace("incremental_name: fname %s, ext %s\n", fname, ext);
    static char _fname[512] = {};
    const int max_increment = 200;
    for (int j = 0; j < max_increment; ++j) {
        sprintf(_fname, "%s%d.%s", fname, j, ext);
        FILE *checker = fopen(_fname, "r");
        if (!checker) {
            trace("incremental_name: _fname %s\n", _fname);
            return _fname;
        }
        fclose(checker);
    }
    return NULL;
}

// TODO: Исключить запись при нахождении мыши в окне dotool_gui()
void dotool_gui(struct dotool_ctx *ctx) {
    ImGuiWindowFlags wnd_flags = 0;
    static bool open = true;
    igBegin("xdotool scripts recorder", &open, wnd_flags);

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
        const char *fname = incremental_name("dotool", "txt");
        if (dotool_record_save(ctx, fname)) {
            snprintf(
                ctx->status_msg, sizeof(ctx->status_msg),
                "saved to '%s'", fname
            );
        }
    }
    if (ctx->is_recording) {
        igText("recoring .. %d ticks", ctx->rec_num);
    }
    if (strlen(ctx->status_msg))
        igText("%s", ctx->status_msg);

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
    dotool_record_first_tick(ctx);
}

void dotool_record_stop(dotool_ctx_t *ctx) {
    trace("dotool_record_stop:\n");
    assert(ctx);
    ctx->is_recording = false;
}

bool dotool_record_save(dotool_ctx_t *ctx, const char *fname) {
    assert(ctx);
    assert(fname);
    FILE *fdest = fopen(fname, "w");
    if (!fdest) {
        trace("dotool_record_save: could not open file '%s'\n", fname);
        return false;
    }
    Vector2 prev_pos = ctx->rec[0].pos;
    double prev_time = ctx->rec[0].timestamp;
    
    fprintf(
        fdest, "mousemove -- %d %d\n", (int)prev_pos.x, (int)prev_pos.y
    );

    for (int i = 0; i < ctx->rec_num; ++i) {
        struct MouseState *cur = &ctx->rec[i];
        trace("dotool_record_save: prev_pos %s\n", Vector2_tostr(prev_pos));
        trace(
            "dotool_record_save: ctx->rec[i].pos %s\n",
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
    return true;
}
