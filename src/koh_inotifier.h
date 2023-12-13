#pragma once

typedef void (*WatchCallback)(const char *fname, void *data);

void inotifier_init();
void inotifier_shutdown();
// TODO: Починить необходимость переустановки наблюдателя при вызове обратной
// функции
void inotifier_watch(const char *fname, WatchCallback cb, void *data);
void inotifier_remove_watch(const char *fname);
void inotifier_update();
