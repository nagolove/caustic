#include "koh_sfx.h"

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "koh_hashers.h"
#include "lua.h"
#include "raylib.h"

#include "koh_common.h"
#include "koh_console.h"
#include "koh_logger.h"
#include "koh_lua_tools.h"
#include "koh_script.h"
#include "koh_table.h"

static HTable *sounds_tbl = NULL;
static const char *SFX_PATH = "assets/sfx";
static const int SOUND_ARR_CAPACITY = 5;
//static Sound* current_snd = NULL;
static float volume = 0.5;

struct SoundArray {
    Sound *sounds;
    int   num, cap;
};

static HTableAction iter_print_sound(
    const void *key, int key_len, void *value, int value_len, void *data
) {
    struct SoundArray *sound_arr = value;
    console_buf_write("[%s] - %d variations\n", (char*)key, sound_arr->num);
    return HTABLE_ACTION_NEXT;
}

int l_sounds(lua_State *lua) {
    if (sounds_tbl)
        htable_each(sounds_tbl, iter_print_sound, NULL);
    return 0;
}

int l_sound_play(lua_State *lua) {
    trace("l_sound_play: [%s]\n", L_stack_dump(lua));
    if (lua_isstring(lua, 1)) {
        sfx_play(lua_tostring(lua, 1));
    }
    return 0;
}

void sfx_init() {
    trace("sfx_init:\n");
    sounds_tbl = htable_new(&(struct HTableSetup) {
        .hash_func = koh_hasher_mum,
        .cap = 64,
    });

    FilePathList filelist = LoadDirectoryFiles(SFX_PATH);
    SetTraceLogLevel(LOG_ERROR);
    for (int q = 0; q < filelist.count; ++q) {
        const char *fname = filelist.paths[q];
        Sound snd = LoadSound(fname);
        SetSoundVolume(snd, volume);
        
        // TODO: Учет длительности звучания при проигрывании звука
        /*float duration = snd.frameCount / (float)snd.stream.sampleRate;*/
        /*printf("duration %f sec\n", duration);*/

        const char *basename = extract_filename(fname, ".wav");
        assert(basename);
        const char *without_suffix = remove_suffix(basename);

        if (!without_suffix) {
            trace("sfx_init: without_suffix: NULL on '%s'\n", basename);
            continue;
        }

        trace("sfx_init: without suffix %s\n", without_suffix);
        struct SoundArray *sound_arr = htable_get_s(
            sounds_tbl, without_suffix, NULL
        );
        if (sound_arr) {
            if (sound_arr->num + 1 < sound_arr->cap) {
                sound_arr->cap += SOUND_ARR_CAPACITY;
                sound_arr->sounds = realloc(
                    sound_arr->sounds, 
                    sizeof(*sound_arr->sounds) * sound_arr->cap
                );
            }
            sound_arr->sounds[sound_arr->num++] = snd;
        } else {
            struct SoundArray tmp = {0};
            sound_arr = &tmp;
            sound_arr->cap = SOUND_ARR_CAPACITY;
            sound_arr->sounds = calloc(
                sound_arr->cap, sizeof(*sound_arr->sounds)
            );
            sound_arr->sounds[sound_arr->num++] = snd;
            htable_add_s(
                sounds_tbl, without_suffix, sound_arr, sizeof(*sound_arr)
            );
        }
    }
    UnloadDirectoryFiles(filelist);
    SetTraceLogLevel(LOG_ALL);

    sc_register_function(
        l_sounds, "sounds", "Получить список звуков загруженных в уровень"
    );
    sc_register_function(
        l_sound_play, "sound_play", 
        "Воспроизвести звук по строковому идентификатору"
    );
}

HTableAction iter_unload_sound_arr(
    const void *key, int key_len, void *value, int value_len, void *data
) {
    struct SoundArray *sound_arr = value;
    if (sound_arr) {
        for (int j = 0; j < sound_arr->num; ++j)
            UnloadSound(sound_arr->sounds[j]);
        free(sound_arr->sounds);
    }
    return HTABLE_ACTION_NEXT;
}

void sfx_shutdown() {
    if (sounds_tbl) {
        htable_each(sounds_tbl, iter_unload_sound_arr, NULL);
        htable_free(sounds_tbl);
        sounds_tbl = NULL;
    }
}

float sfx_play(const char *sfx_name) {
    struct SoundArray *sound_arr = htable_get_s(
        sounds_tbl, sfx_name, NULL
    );
    if (sound_arr) {
        int index = rand() % sound_arr->num;
        trace("sfx_play: '%s', index %d\n", sfx_name, index);
        //if (!IsSoundPlaying(sound_arr->sounds[index])) {
            //current_snd = &sound_arr->sounds[index];
            Sound *snd = &sound_arr->sounds[index];
            float duration = snd->frameCount / (float)snd->stream.sampleRate;
            PlaySound(*snd);
            return duration;
        //}
    } else {
        // TODO: Коллекционировать эти записи, под которые нету звуков.
        trace("sfx_play: '%s' not found\n", sfx_name);
    }
    return 0.;
}

