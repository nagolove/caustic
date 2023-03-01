#pragma once

#include "raylib.h"

#define MAX_STAGES_NUM  32
#define MAX_STAGE_NAME  64

struct Stage;

typedef void (*Stage_callback)(struct Stage *s);
typedef void (*Stage_data_callback)(struct Stage *s, void *data);

typedef struct Stage {
    Stage_data_callback init;
    Stage_callback      shutdown;
    Stage_callback      draw, update;
    Stage_data_callback enter, leave;

    void *data;
    char name[MAX_STAGE_NAME];
} Stage;

void stage_init(void);
void stage_add(Stage *st, const char *name);
void stage_subinit(void);
void stage_shutdown_all(void);
void stage_update_active(void);
void stage_set_active(const char *name, void *data);
const char *stage_get_active_name();
Stage *stage_find(const char *name);
void *stage_assert(Stage *st, const char *name);
void stages_print();
