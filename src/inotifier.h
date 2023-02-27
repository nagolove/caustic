#pragma once

typedef void (*WatchCallback)(const char *fname, void *data);

void inotifier_init();
void inotifier_shutdown();
void inotifier_watch(const char *fname, WatchCallback cb, void *data);
void inotifier_remove_watch(const char *fname);
void inotifier_update();
