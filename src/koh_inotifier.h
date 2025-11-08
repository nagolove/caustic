#pragma once

#include <stdbool.h>
#include "koh_strbuf.h"

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
StrBuf inotifier_list();
void inotifier_gui();

extern bool inotifier_verbose;
