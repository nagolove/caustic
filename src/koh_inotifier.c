// vim: fdm=marker
#include "koh_inotifier.h"
#include "koh_console.h"
#include "koh_hashers.h"
#include "koh_script.h"

#if defined(PLATFORM_WEB)
void inotifier_init() { }
void inotifier_update() { }
void inotifier_shutdown() { }
void inotifier_watch(const char *fname, WatchCallback cb, void *data) { };
void inotifier_remove_watch(const char *fname) { }
#else

#include "koh_logger.h"
#include "koh_table.h"
#include "raylib.h"
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

struct WatchContext {
    WatchCallback   cb;
    void            *data;
};

#define MAX_WATCHED_FILES 32

// TODO: Перенести в структуру
struct {
    int fd;
    int wd[MAX_WATCHED_FILES];
    char *fnames[MAX_WATCHED_FILES];
    int watched_num;
    HTable *tbl;
    struct pollfd fds[1];
} in;

static const char *get_fname(int target_wd) {
    for (int i = 0; i < in.watched_num; ++i) {
        if (in.wd[i] == target_wd) {
            return in.fnames[i];
        }
    }
    return NULL;
}

static void process_event(const struct inotify_event *event) {
    assert(event);

    //{{{
    /*
    if (event->mask &IN_ACCESS	 )
        printf("IN_ACCESS	 \n");
    if (event->mask &IN_MODIFY	 )
        printf("IN_MODIFY	 \n");
    if (event->mask &IN_ATTRIB	 )
        printf("IN_ATTRIB	 \n");
    if (event->mask &IN_CLOSE_WRITE	 )
        printf("IN_CLOSE_WRITE	 \n");
    if (event->mask &IN_CLOSE_NOWRITE )
        printf("IN_CLOSE_NOWRITE \n");
    if (event->mask &IN_CLOSE	 )
        printf("IN_CLOSE	 \n");
    if (event->mask &IN_OPEN		 )
        printf("IN_OPEN		 \n");
    if (event->mask &IN_MOVED_FROM	 )
        printf("IN_MOVED_FROM	 \n");
    if (event->mask &IN_MOVED_TO      )
        printf("IN_MOVED_TO      \n");
    if (event->mask &IN_MOVE		 )
        printf("IN_MOVE		 \n");
    if (event->mask &IN_CREATE	 )
        printf("IN_CREATE	 \n");
    if (event->mask &IN_DELETE	 )
        printf("IN_DELETE	 \n");
    if (event->mask &IN_DELETE_SELF	 )
        printf("IN_DELETE_SELF	 \n");
    if (event->mask &IN_MOVE_SELF	 )
        printf("IN_MOVE_SELF	 \n");
    */
    //}}}

    //XXX: При флаге IN_DELETE_SELF заново вызывать inotify_add_watch()


    if (!(event->mask & IN_MOVE_SELF))
        return;

    const char *fname = get_fname(event->wd);
    if (!fname)
        return;

    //trace("fname %s\n", fname);
    struct WatchContext *ctx;
    ctx = htable_get(in.tbl, fname, strlen(fname) + 1, NULL);
    //trace("ctx %p\n", ctx);
    if (ctx && ctx->cb) {
        trace("process_event: callback\n");
        ctx->cb(fname, ctx->data);
    }
}

static void inotifier_handle_events() {
    trace("inotifier_handle_events:\n");
    char buf[4096]
        __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    for (;;) {
        ssize_t len = read(in.fd, buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN) {
            perror("handle_events: read");
            exit(EXIT_FAILURE);
        }

        if (len <= 0)
            break;

        /* Loop over all events in the buffer. */
        int size = sizeof(struct inotify_event);
        for (char *ptr = buf; ptr < buf + len; ptr += size + event->len) {
            event = (const struct inotify_event*)ptr;
            process_event(event);
        }
    }
}

static HTableAction iter_list(
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    console_buf_write(">%s", (char*)key);
    trace("l_notifier_list: %s\n", (char*)key);
    return HTABLE_ACTION_NEXT;
}

static int l_inotifier_list(lua_State *lua) {
    htable_each(in.tbl, iter_list, NULL);
    return 0;
}

void inotifier_init() {
    trace("inotifier_init:\n");

    if (in.tbl) {
        const char *msg = "inotifier_init: tbl ~= NULL, "
                          "may be a double initialization?\n";
        trace("%s", msg);
        exit(EXIT_FAILURE);
    }

    in.tbl = htable_new(&(struct HTableSetup) {
        .cap       = MAX_WATCHED_FILES,
        .hash_func = koh_hasher_mum,
    });

    in.fd = inotify_init1(IN_NONBLOCK);
    if (in.fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }
    in.fds[0] = (struct pollfd) {
        .fd = in.fd,
        .events = POLLIN,
    };
    //fds[1] = (struct pollfd) {
        //.fd = STDIN_FILENO,
        //.events = POLLIN,
    //};

    sc_register_function(
        l_inotifier_list,
        "inotifier_list",
        "Вывести список отслеживаемых для перезагрузки файлов."
    );
};

void inotifier_update() {
    // Система не инициализирована
    if (!in.tbl) return;

    //trace("inotifier_update\n");
    //fds[0].fd = STDIN_FILENO;       [> Console input <]
    //fds[0].events = POLLIN;

    int timeout = 0;
    int poll_num = poll(in.fds, 1, timeout);

    //trace("poll_num %d\n", poll_num);
    
    if (poll_num == -1) {
        if (errno == EINTR) {
            trace("inotifier_update: error %s", strerror(errno));
            return;
        }
        perror("poll");
        exit(EXIT_FAILURE);
    }

    /*
    if (poll_num > 0 && fds[1].revents & POLLIN) {
        char buf;
        while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
            continue;
    }
    */

    /*
    trace("fd %d, events %d, revents %d\n",
            fds[0].fd,
            fds[0].events,
            fds[0].revents
          );
    */

    if (poll_num > 0 && in.fds[0].revents & POLLIN) {
        /*trace(" fds[0].revents & POLLIN %d\n", fds[0].revents & POLLIN);*/
        inotifier_handle_events();
    }
}

static void fnames_free() {
    for (int i = in.watched_num - 1; i >= 0; i--) {
        free(in.fnames[i]);
    }
    in.watched_num = 0;
}

void inotifier_shutdown() {
    trace("inotifier_shutdown:\n");
    if (in.tbl) {
        htable_free(in.tbl);
        in.tbl = NULL;
    }
    close(in.fd);
    fnames_free();
}

void inotifier_watch(const char *fname, WatchCallback cb, void *data) {
    // Система не инициализирована
    if (!in.tbl) return;

    if (in.watched_num == MAX_WATCHED_FILES) {
        trace("inotifier_watch: MAX_WATCHED_FILES reached\n");
        return;
    }

    trace("inotifier_watch: '%s' with data %p\n", fname, data);
    uint32_t mask = IN_ALL_EVENTS;
    //uint32_t mask = IN_MOVE_SELF;
    in.wd[in.watched_num] = inotify_add_watch(in.fd, fname, mask);
    in.fnames[in.watched_num] = strdup(fname);

    if (in.wd[in.watched_num] == -1) {
        trace("inotifier_watch: cannot watch '%s': %s\n", fname, strerror(errno));
    }

    struct WatchContext ctx = {
        .cb = cb, .data = data,
    };
    htable_add(in.tbl, fname, strlen(fname) + 1, &ctx, sizeof(ctx));
    in.watched_num++;
}

void inotifier_remove_watch(const char *fname) {
    // Система не инициализирована
    if (!in.tbl) return;

    assert(fname);
    for (int i = 0; i < in.watched_num; i++) {
        if (!strcmp(fname, in.fnames[i])) {
            inotify_rm_watch(in.fd, in.wd[i]);
            htable_remove(in.tbl, fname, strlen(fname) + 1);
        }
    }
}

#endif

