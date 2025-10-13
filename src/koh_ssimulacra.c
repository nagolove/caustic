#include <threads.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include "koh_common.h"
#include <string.h>

typedef enum SsimulacraState {
    // пора заканчивать поток
    ss_quit,
    // идет вызовд exec
    ss_busy,
    // готов принять заказ на обработку
    ss_ready,
} SsimulacraState;

#define SSIMULACRA_PATH_LEN     128
#define SSIMULACRA_QUEUE_CAP    32

typedef struct ssimulacra {
    char                    queue[SSIMULACRA_QUEUE_CAP][SSIMULACRA_PATH_LEN];
    _Atomic(int)            queue_num;
    _Atomic(SsimulacraState)    state;
    thrd_t                      t;
} Ssimulacra;

extern char **environ;

int run_ssimulacra(const char *cand, const char *ref) {
    pid_t pid;
    DIAG_PUSH_WCASTQUAL_OFF
    char *argv[] = { "ssimulacra2_rs", (char*)cand, (char*)ref, NULL };
    DIAG_POP
    int err = posix_spawnp(&pid, "ssimulacra2_rs", NULL, NULL, argv, environ);
    if (err != 0) {
        errno = err; perror("posix_spawnp");
        return -1;
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return -1; }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    fprintf(stderr, "ssimulacra2_rs exited with %d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return -1;
}

int worker_ssimulacra(void *p) {
    Ssimulacra *s = p;
    assert(s);

    s->state = ss_ready;

    while (s->state != ss_quit) {
        struct timespec ts;
        ts.tv_sec = 0;          // секунды
        ts.tv_nsec = 100000000; // наносекунды (100_000_000 ns = 100 мс)

        //printf("Спим 100 мс...\n");
        thrd_sleep(&ts, NULL);

        //assert(s->state != ss_busy);
        if (s->queue_num > 0 && ss_ready == s->state) {
            s->state = ss_busy;
            const char *render_sample_fname = s->queue[s->queue_num - 1];
            printf(
                "worker_ssimulacra: render_sample_fname '%s'\n",
                render_sample_fname
            );

            char ref_fname[SSIMULACRA_PATH_LEN] = {};
            int maxchars = snprintf(
                ref_fname,
                SSIMULACRA_PATH_LEN,
                "references/%s",
                render_sample_fname
            );
            assert(maxchars < SSIMULACRA_PATH_LEN);

            printf(
                "worker_ssimulacra: execv '%s' '%s'\n",
                render_sample_fname, ref_fname
            );

            int code = run_ssimulacra(render_sample_fname, ref_fname);
            if (code != 0) {
                printf(
                    "worker_ssimulacra: run_ssimulacra failed with code %d\n",
                    code
                );
            }

            s->queue_num--;
            s->state = ss_ready;
        }
    }

    return 0;
}

void ssimulacra_init(Ssimulacra *s) {
    assert(s);
    int code = thrd_create(&s->t, worker_ssimulacra, s);
    printf("ssimulacra_init: code %d\n", code);
}

void ssimulacra_shutdown(Ssimulacra *s) {
    assert(s);
    s->state = ss_quit;
    thrd_join(s->t, NULL);
}

void ssimulacra_push(Ssimulacra *s, const char *render_sample_fname) {
    assert(s);
    assert(render_sample_fname);
    assert(s->queue_num < SSIMULACRA_QUEUE_CAP);
    assert(strlen(render_sample_fname) < SSIMULACRA_PATH_LEN);

    char *queue_slot = s->queue[s->queue_num++];
    memset(queue_slot, 0, SSIMULACRA_PATH_LEN);
    strncpy(queue_slot, render_sample_fname, SSIMULACRA_PATH_LEN);
}

