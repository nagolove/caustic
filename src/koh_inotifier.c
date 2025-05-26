// vim: fdm=marker
#include "koh_inotifier.h"
//#include "koh_console.h"
#include "koh_hashers.h"
#include "koh_lua.h"

#ifdef __wasm__
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

bool inotifier_verbose = false;

#define MAX_WATCHED_FILES 64

// TODO: Перенести в структуру
struct {
    int      fd;
    int      wd[MAX_WATCHED_FILES];
    char     *fnames[MAX_WATCHED_FILES];
    uint32_t masks[MAX_WATCHED_FILES];
    int      watched_num;
    HTable   *tbl;
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

typedef struct Mask2Str {
    uint32_t mask;
    char *str_mask, *str_comment;
} Mask2Str;

Mask2Str _mask2str[] = {
    // {{{
    { IN_ACCESS, "IN_ACCESS", " File was accessed.", },
    { IN_MODIFY, "IN_MODIFY", " File was modified.", },
    { IN_ATTRIB, "IN_ATTRIB", " Metadata changed.", },
    { IN_CLOSE_WRITE, "IN_CLOSE_WRITE", " Writtable file was closed.", },
    { IN_CLOSE_NOWRITE, "IN_CLOSE_NOWRITE", " Unwrittable file closed.", },
    { IN_CLOSE, "IN_CLOSE", "Close."},
    { IN_OPEN , "IN_OPEN  ", " File was opened.", },
    { IN_MOVED_FROM, "IN_MOVED_FROM", " File was moved from X.", },
    { IN_MOVED_TO, "IN_MOVED_TO",      " File was moved to Y.", },
    { IN_MOVE, "IN_MOVE" , "Moves." },
    { IN_CREATE, "IN_CREATE", " Subfile was created.", },
    { IN_DELETE, "IN_DELETE", " Subfile was deleted.", },
    { IN_DELETE_SELF, "IN_DELETE_SELF", " Self was deleted.", },
    { IN_MOVE_SELF, "IN_MOVE_SELF", " Self was moved.", },
    { IN_UNMOUNT, "IN_UNMOUNT", " Backing fs was unmounted.", },
    { IN_Q_OVERFLOW, "IN_Q_OVERFLOW", " Event queued overflowed.", },
    { IN_IGNORED, "IN_IGNORED", " File was ignored.", },
    { IN_ONLYDIR, "IN_ONLYDIR", " Only watch the path if it is a directory.", },
    { IN_DONT_FOLLOW, "IN_DONT_FOLLOW", " Do not follow a sym link.", },
    { IN_EXCL_UNLINK, "IN_EXCL_UNLINK", " Exclude events on unlinked objects.", },
    { IN_MASK_CREATE, "IN_MASK_CREATE", " Only create watches.", },
    { IN_MASK_ADD, "IN_MASK_ADD", " Add to the mask of an already existing watch.", },
    { IN_ISDIR, "IN_ISDIR", " Event occurred against dir.", },
    { IN_ONESHOT, "IN_ONESHOT", " Only send event once.", },
    { 0, NULL, NULL },
    // }}}
};

Mask2Str *mask2str(uint32_t mask) {
    static Mask2Str strs[100];
    memset(strs, 0, sizeof(strs));

    int num = 0;
    for (int i = 0; _mask2str[i].str_mask; i++) {
        if (mask & _mask2str[i].mask)
            strs[num++] = _mask2str[i];
    }

    trace("mask2str: num %d\n", num);

    return strs;
}

static void process_event(const struct inotify_event *event) {
    assert(event);

    if (event->mask & IN_CREATE) {
        if (inotifier_verbose)
            trace("process_event: IN_CREATE '%s'\n", event->name);
    }

    //XXX: При флаге IN_DELETE_SELF заново вызывать inotify_add_watch()

    if (inotifier_verbose) {
        static char buf[1024 * 2] = {}, *pbuf = buf;
        memset(buf, 0, sizeof(buf));

        Mask2Str *strs = mask2str(event->mask);
        while (strs->str_mask) {
            trace("process_event: str_mask '%s'\n", strs->str_mask);
            int r = sprintf(pbuf, "%s|", strs->str_mask);
            pbuf += r;
            strs++;
        }

        trace(
            "process_event: mask '%s', name '%s'\n",
            //to_bitstr_uint32_t(event->mask),
            buf,
            event->name
        );
    }

    if (!(event->mask & IN_MOVE_SELF))
        return;

    //if (!(event->mask & IN_CLOSE))
        //return;

    const char *fname = get_fname(event->wd);
    if (!fname)
        return;

    //trace("fname %s\n", fname);
    struct WatchContext *ctx;
    ctx = htable_get(in.tbl, fname, strlen(fname) + 1, NULL);
    //trace("ctx %p\n", ctx);
    if (ctx && ctx->cb) {
        if (inotifier_verbose)
            trace("process_event: callback\n");
        ctx->cb(fname, ctx->data);
    }
}

static void inotifier_handle_events() {
    if (inotifier_verbose)
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
    /*console_buf_write(">%s", (char*)key);*/
    trace("inotifier_list: %s\n", (const char*)key);
    return HTABLE_ACTION_NEXT;
}


/*
static int l_inotifier_list(lua_State *lua) {
    htable_each(in.tbl, iter_list, NULL);
    return 0;
}
*/

void inotifier_list() {
    htable_each(in.tbl, iter_list, NULL);
}

void inotifier_init() {
    if (inotifier_verbose)
        trace("inotifier_init:\n");

    assert(in.tbl == NULL);

    in.tbl = htable_new(&(struct HTableSetup) {
        .cap    = MAX_WATCHED_FILES,
        //.f_hash = koh_hasher_mum,
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

    /*
    sc_register_function(
        l_inotifier_list,
        "inotifier_list",
        "Вывести список отслеживаемых для перезагрузки файлов."
    );
    // */
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
    if (inotifier_verbose)
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
        if (inotifier_verbose)
            trace("inotifier_watch: MAX_WATCHED_FILES reached\n");
        return;
    }

    if (inotifier_verbose)
        trace("inotifier_watch: '%s' with data %p\n", fname, data);
    uint32_t mask = IN_ALL_EVENTS;
    //uint32_t mask = IN_MOVE_SELF;
    in.wd[in.watched_num] = inotify_add_watch(in.fd, fname, mask);
    in.fnames[in.watched_num] = strdup(fname);

    if (in.wd[in.watched_num] == -1) {
        if (inotifier_verbose)
            trace(
                "inotifier_watch: cannot watch '%s': %s\n",
                fname, strerror(errno)
            );
    }

    struct WatchContext ctx = {
        .cb = cb, .data = data,
    };
    htable_add(in.tbl, fname, strlen(fname) + 1, &ctx, sizeof(ctx));
    in.watched_num++;
}

void inotifier_watch_remove(const char *fname) {
    // Система не инициализирована
    if (!in.tbl) return;

    assert(fname);
    // XXX: Где удаление in.fnames? Где декремент in.watched_num?
    for (int i = 0; i < in.watched_num; i++) {
        if (!strcmp(fname, in.fnames[i])) {
            inotify_rm_watch(in.fd, in.wd[i]);
            htable_remove(in.tbl, fname, strlen(fname) + 1);
        }
    }
}

#endif
