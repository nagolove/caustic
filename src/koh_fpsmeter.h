#pragma once

#include <stdbool.h>

void koh_fpsmeter_init();
void koh_fpsmeter_shutdown();
void koh_fpsmeter_frame_begin();
void koh_fpsmeter_frame_end();
void koh_fpsmeter_draw();
void koh_fpsmeter_stat_set(bool state);
bool koh_fpsmeter_stat_get();
void koh_fpsmeter_mark();
