#pragma once

void sfx_init();
void sfx_shutdown();
// Если звук найден, то возвращается его длительность, в секундах(?)
float sfx_play(const char *tex_name);
