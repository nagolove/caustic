// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include <stdbool.h>

extern bool koh_verbose_input;

typedef struct InputKbMouseDrawer InputKbMouseDrawer;

struct InputKbMouseDrawerSetup {
    int btn_width;
};

InputKbMouseDrawer *input_kb_new(struct InputKbMouseDrawerSetup *setup);
void input_kb_free(InputKbMouseDrawer *kb);
void input_kb_update(InputKbMouseDrawer *kb);
void input_kb_gui_update(InputKbMouseDrawer *kb);


typedef struct InputGamepadDrawer InputGamepadDrawer;

/*
struct InputGamepadDrawerSetup {
    int btn_width;
};
*/

InputGamepadDrawer *input_gp_new();
void input_gp_free(InputGamepadDrawer *kb);
void input_gp_update(InputGamepadDrawer *kb);
/*void input_gp_gui_update(InputGamepadDrawer *kb);*/
