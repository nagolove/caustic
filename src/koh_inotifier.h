#pragma once

#include <stdbool.h>
#include "koh_strbuf.h"

typedef void (*WatchCallback)(const char *fname, void *data);

// XXX: Почему не используется какая-то переменая контекта?
// Есть ли необходимость в глобальных переменных?

#ifdef _WIN32

// Заглушки для Windows (inotify недоступен)
static inline void inotifier_init() {}
static inline void inotifier_shutdown() {}
static inline void inotifier_watch(
	const char *fname, WatchCallback cb, void *data
) { (void)fname; (void)cb; (void)data; }
static inline void inotifier_watch_remove(
	const char *fname
) { (void)fname; }
static inline void inotifier_update() {}
static inline StrBuf inotifier_list() {
	return (StrBuf){};
}
static inline void inotifier_gui() {}
#ifdef KOH_INOTIFIER_VERBOSE
static bool inotifier_verbose = false;
#endif

#else

void inotifier_init();
void inotifier_shutdown();
// TODO: Починить необходимость переустановки наблюдателя
// при вызове обратной функции
void inotifier_watch(
	const char *fname, WatchCallback cb, void *data
);
void inotifier_watch_remove(const char *fname);
void inotifier_update();
StrBuf inotifier_list();
void inotifier_gui();

extern bool inotifier_verbose;

#endif
