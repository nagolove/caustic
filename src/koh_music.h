#pragma once

#include <stdint.h>
#include "raylib.h"

struct Song {
    int slot;
};

void koh_music_init();
void koh_music_shutdown();
void koh_music_play(const char *modname);
struct Song *koh_music_load(const char *fname);
void koh_music_modules_list(const char *modname);
void koh_music_scope(const char *modname, int mod_num, Vector2 pos);

extern uint32_t koh_music_freq;

