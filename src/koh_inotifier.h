#pragma once

#include <stdbool.h>

typedef void (*WatchCallback)(const char *fname, void *data);

// XXX: Почему не используется какая-то переменая контекта?
// Есть ли необходимость в глобальных переменных?

void inotifier_init();
void inotifier_shutdown();
// TODO: Починить необходимость переустановки наблюдателя при вызове обратной
// функции
void inotifier_watch(const char *fname, WatchCallback cb, void *data);
void inotifier_watch_remove(const char *fname);
void inotifier_update();
void inotifier_list();

extern bool inotifier_verbose;
