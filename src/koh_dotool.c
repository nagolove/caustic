#include "koh_dotool.h"

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

// XXX: Сложная система записи
enum ActionType {
    AT_MOUSE_MOVE       = 0,
    AT_MOUSE_CLICK      = 1,
    AT_MOUSE_DOWN       = 1,
    AT_MOUSE_UP         = 2,
    AT_KEYBOARD_PRESS
};

struct Action {

};

struct dotool_ctx {
    pthread_cond_t      *condition;
    pthread_mutex_t     *mutex;
    const char          *shm_name_mutex, *shm_name_cond;
    pthread_condattr_t  cond_attr;
    pthread_mutexattr_t mutex_attr;
    Vector2             dispacement;
    bool                is_recording;
};

static const char   *default_shm_name_mutex = "caustic_xdt_mutex",
                    *default_shm_name_cond = "caustic_xdt_cond";

struct dotool_ctx *dotool_new() {
    struct dotool_ctx *ctx = calloc(1, sizeof(*ctx));
    assert(ctx);

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

    return ctx;
}

void dotool_free(struct dotool_ctx *ctx) {
    if (!ctx)
        return;

    pthread_condattr_destroy(&ctx->cond_attr);
    pthread_mutexattr_destroy(&ctx->mutex_attr);
    pthread_mutex_destroy(ctx->mutex);
    pthread_cond_destroy(ctx->condition);

    munmap(ctx->mutex, sizeof(*ctx->mutex));
    munmap(ctx->condition, sizeof(*ctx->condition));
    shm_unlink(ctx->shm_name_cond);
    shm_unlink(ctx->shm_name_mutex);

    free(ctx);
}

void dotool_send_signal(struct dotool_ctx *ctx) {
    assert(ctx);

    pthread_mutex_lock(ctx->mutex);
    pthread_cond_signal(ctx->condition);
    pthread_mutex_unlock(ctx->mutex);

    trace("dotool_send_signal: wake up, \n");
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
        execve(
            "/usr/bin/xdotool",
            (char *const []){ 
                (char*)script_fname,
                NULL 
            },
            __environ
        );
    }
}

void dotool_gui(struct dotool_ctx *ctx) {
    ImGuiWindowFlags wnd_flags = 0;
    static bool open = true;
    igBegin("xdotool scripts recorder", &open, wnd_flags);

    if (igButton("⏺", (ImVec2){})) {
        open = false;
        dotool_record_start(ctx);
    }
    igSameLine(0., 5.);
    if (igButton("⏹", (ImVec2){})) {
        open = true;
        dotool_record_stop(ctx);
    }

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
        trace(
            "dotool_setup_display: height %d\n", GetMonitorHeight(monitor)
        );
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
    }
}

void dotool_record_start(dotool_ctx_t *ctx) {
    trace("dotool_record_start:\n");
}

void dotool_record_stop(dotool_ctx_t *ctx) {
    trace("dotool_record_stop:\n");
}
