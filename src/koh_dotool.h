#pragma once

#include <pthread.h>

typedef struct dotool_ctx dotool_ctx_t;

dotool_ctx_t *dotool_new();
void dotool_free(dotool_ctx_t *ctx_t);
void dotool_send_signal(dotool_ctx_t *ctx_t);
void dotool_exec_script(dotool_ctx_t *ctx_t, const char *script_fname);
void dotool_gui(dotool_ctx_t *ctx_t);
void dotool_setup_display(dotool_ctx_t *ctx);
void dotool_update(dotool_ctx_t *ctx);
void dotool_record_start(dotool_ctx_t *ctx);
void dotool_record_stop(dotool_ctx_t *ctx);
