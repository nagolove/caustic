#pragma once

#include <unistd.h>
#include <pthread.h>

struct dotool_ctx {
    pthread_cond_t      *condition;
    pthread_mutex_t     *mutex;
    const char          *shm_name_mutex, *shm_name_cond;
    pthread_condattr_t  cond_attr;
    pthread_mutexattr_t mutex_attr;
};

void dotool_init(struct dotool_ctx *ctx);
void dotool_shutdown(struct dotool_ctx *ctx);
void dotool_send_signal(struct dotool_ctx *ctx);
void dotool_exec_script(struct dotool_ctx *ctx);

