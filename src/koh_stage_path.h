// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "koh_path.h"
#include "koh_stages.h"

typedef struct Stage_Path {
    Stage parent;
    Path path;
} Stage_Path;

Stage *stage_path_new(void);



