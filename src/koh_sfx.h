#pragma once

void sfx_init();
void sfx_shutdown();
// XXX: Зачем возвращается float?
float sfx_play(const char *tex_name);
