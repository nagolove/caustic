#include "koh_music.h"

#include <assert.h>
#include <stdlib.h>
#include "raylib.h"

struct Playlist {
    Music   *tracks;
    int     tracks_num, current;
};

static const char *MUSIC_PATH = "assets/background";
/*static float volume = 0.5;*/
static struct Playlist plst = {0};

void music_init() {
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

void music_shutdown() {
    for (int j = 0; j < plst.tracks_num; ++j) {
        UnloadMusicStream(plst.tracks[j]);
    }
    if (plst.tracks) {
        free(plst.tracks);
        plst.tracks = NULL;
    }
    plst.tracks_num = 0;
}

void music_play() {
    if (plst.tracks_num == 0) return;

    if (plst.current == -1) {
    }
}

