#include "koh_music.h"

/*#define MUSIC_SUNVOX*/

#ifdef MUSIC_SUNVOX
#ifdef PLATFORM_WEB
    #pragma message("sunvox static loading")
    #define SUNVOX_STATIC_LIB
#else
    #pragma message("sunvox dyn loading")
    #define SUNVOX_MAIN /* We are using a dynamic lib. SUNVOX_MAIN adds implementation of sv_load_dll()/sv_unload_dll() */
    #include <dlfcn.h>
#endif
#endif

#include "koh_common.h"
#include "koh_logger.h"
#include "koh_table.h"
#include "raylib.h"
#include "string.h"
#include "sunvox.h"
#include <assert.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

//#define MUSIC_RAY

struct Playlist {
    Music   *tracks;
    int     tracks_num, current;
};

static bool is_inited = false;
#ifdef MUSIC_SUNVOX
static HTable *songs = NULL;
static int last_slot = 0;
#endif
uint32_t koh_music_freq = 44100;

#ifdef MUSIC_RAY
static const char *MUSIC_PATH = "assets/background";
/*static float volume = 0.5;*/
static struct Playlist plst = {0};
#endif

#ifdef MUSIC_SUNVOX
static void on_song_remove(
    const void *key, int key_len, void *value, int value_len 
) {
    struct Song *song = value;
    trace("on_song_remove: sv_close_slot %d\n", song->slot);
    sv_lock_slot(song->slot);
    sv_close_slot(song->slot);
    sv_unlock_slot(song->slot);
}
#endif

#ifdef MUSIC_SUNVOX
static void svx_init() {
#ifndef PLATFORM_WEB
    int errcode = sv_load_dll();
    trace("music_init: sv_load_dll %d\n", errcode);
#endif

    int ver = sv_init(NULL, koh_music_freq, 2, 0 );
    if( ver >= 0 ) {
        int major = ( ver >> 16 ) & 255;
        int minor1 = ( ver >> 8 ) & 255;
        int minor2 = ( ver ) & 255;
        trace("music_init: SunVox lib version: %d.%d.%d\n", major, minor1, minor2 );
        trace("music_init: Current sample rate: %d\n", sv_get_sample_rate() );
    }

    songs = htable_new(&(struct HTableSetup) {
        .cap = 256,
        .on_remove = on_song_remove,
    });
}
#else
static void svx_init() {
}
#endif

#ifdef MUSIC_SUNVOX
static void svx_shutdown() {
    htable_free(songs);
    sv_deinit();

#ifndef PLATFORM_WEB
    int errcode = sv_unload_dll();
    trace("svx_shutdown: sv_unload_dll %d\n", errcode);
#endif

}
#else
static void svx_shutdown() {
}
#endif

#ifdef MUSIC_RAY
static void music_init() {
    FilePathList filelist = LoadDirectoryFiles(MUSIC_PATH);
    plst.tracks_num = filelist.count;
    plst.tracks = calloc(plst.tracks_num, sizeof(plst.tracks[0]));
    assert(plst.tracks);
    for (int i = 0; i < filelist.count; ++i) {
        Music msc = LoadMusicStream(filelist.paths[i]);
        plst.tracks[i] = msc;
    }
    UnloadDirectoryFiles(filelist);
    plst.current = -1;
}
#endif

void koh_music_init() {
    if (is_inited)
        return;
#ifdef MUSIC_RAY
    music_init();
#endif
    svx_init();
    is_inited = true;
}

#ifdef MUSIC_RAY
static void music_shutdown() {
    for (int j = 0; j < plst.tracks_num; ++j) {
        UnloadMusicStream(plst.tracks[j]);
    }
    if (plst.tracks) {
        free(plst.tracks);
        plst.tracks = NULL;
    }
    plst.tracks_num = 0;
}
#endif

void koh_music_shutdown() {
    if (!is_inited) 
        return;
#ifdef MUSIC_RAY
    music_shutdown();
#endif
    svx_shutdown();
    is_inited = false;
}

#ifdef MUSIX_SUNVOX
struct Song *koh_music_load(const char *fname) {
    assert(fname);

    /*
    char fullname[512] = {0};
    strcat(fullname, assets_dir);
    if (assets_dir[strlen(assets_dir) - 1] != '/')
        strcat(fullname, "/");
    strcat(fullname, modname);
    */

    trace("koh_music_load: \"%s\"\n", fname);
    sv_open_slot(last_slot);
    int err = sv_load(last_slot, fname);
    //int err = sv_load(last_slot, "fwfwe");
    if (!err) {
        trace("koh_music_load: sv_load(\"%s\") error code %d\n", fname, err);
    }
    struct Song song = {
        .slot = last_slot,
    };
    const char *basename = get_basename(fname);
    trace("koh_music_load: basename %s\n", basename);
    last_slot++;
    return htable_add_s(songs, basename, &song, sizeof(song));
}
#else
struct Song *koh_music_load(const char *fname) {
    return NULL;
}
#endif

#ifdef SUNVOX_MUSIC
void svx_play(const char *modname) {
    assert(modname);
    struct Song *song = htable_get_s(songs, modname, NULL);
    if (!song) {
        trace("svx_play: '%s' not found\n", modname);
        return;
    }
    trace("svx_play: slot %d\n", song->slot);
	int err = -1;
    //err = sv_open_slot(song->slot);
    sv_volume(song->slot, 256);
    //trace("svx_play: sv_open_slot err %d\n", err);
    err = sv_play_from_beginning(song->slot);
    trace("svx_play: sv_play_from_beginning err %d\n", err);
}
#else
void svx_play(const char *modname) {
}
#endif

void koh_music_play(const char *modname) {
    svx_play(modname);

    //if (plst.tracks_num == 0) return;
    //if (plst.current == -1) {
    //}
}

#ifdef SUNVOX_MUSIC
void koh_music_modules_list(const char *modname) {
    assert(modname);
    struct Song *song = htable_get_s(songs, modname, NULL);
    if (!song) {
        trace("koh_music_modules_list: '%s' not found\n", modname);
        return;
    }
    int modnum = sv_get_number_of_modules(song->slot);
    for (int i = 0; i < modnum; i++) {
        const char *_modname = sv_get_module_name(song->slot, i);
        trace("koh_music_modules_list: %d '%s'\n", i, _modname);
    }
}
#else
void koh_music_modules_list(const char *modname) {
}
#endif

#ifdef SUNVOX_MUSIC
void koh_music_scope(const char *modname, int mod_num, Vector2 pos) {
    assert(modname);
    struct Song *song = htable_get_s(songs, modname, NULL);
    if (!song) {
        trace("koh_music_modules_list: '%s' not found\n", modname);
        return;
    }
    uint32_t samples_to_read = 1024;
    int16_t dst[samples_to_read];
    memset(dst, 0, sizeof(dst));
    uint32_t read = sv_get_module_scope2(
        song->slot, mod_num, 0, dst, samples_to_read
    );
    trace("koh_music_scope: %d\n", read);
    Vector2 next = {0}, last = pos;
    Color color = RED;
    float thick = 3.;
    float dx = 3.;
    for (int i = 0; i < read; i++) {
        int16_t sample = dst[i];
        trace("koh_music_scope: sample %d\n", sample);
        next = Vector2Add(last, (Vector2) { dx, sample});
        DrawLineEx(last, next, thick, color);
    }
}
#else
void koh_music_scope(const char *modname, int mod_num, Vector2 pos) {
}
#endif
