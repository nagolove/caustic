// vim: fdm=marker
#include "koh_inotifier.h"

#include "raylib.h"

#if defined(PLATFORM_WEB)
void inotifier_init() { }
void inotifier_update() { }
void inotifier_shutdown() { }
void inotifier_watch(const char *fname, WatchCallback cb, void *data) { };
void inotifier_remove_watch(const char *fname) { }
#else

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>

#include "koh_table.h"

struct WatchContext {
    WatchCallback   cb;
    void            *data;
};

#define MAX_WATCHED_FILES 32

static int fd;
static int wd[MAX_WATCHED_FILES] = {0};
static char *fnames[MAX_WATCHED_FILES] = {0};
static int watched_num = 0;
static HTable *tbl = NULL;

const char *get_fname(int target_wd) {
    for (int i = 0; i < watched_num; ++i) {
        if (wd[i] == target_wd) {
            return fnames[i];
        }
    }
    return NULL;
}

void process_event(const struct inotify_event *event) {
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

    printf("fname %s\n", fname);
    struct WatchContext *ctx;
    ctx = htable_get(tbl, fname, strlen(fname) + 1, NULL);
    printf("ctx %p\n", ctx);
    if (ctx && ctx->cb) {
        printf("handle_events: callback\n");
        ctx->cb(fname, ctx->data);
    }
}

static void handle_events() {
    printf("handle_events\n");
    char buf[4096]
        __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    for (;;) {
        ssize_t len = read(fd, buf, sizeof(buf));
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

struct pollfd fds[1] = {0};

void inotifier_init() {
    printf("inotifier_init\n");
    tbl = htable_new(&(struct HTableSetup) {
        .cap = MAX_WATCHED_FILES
    });

    fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }
    fds[0] = (struct pollfd) {
        .fd = fd,
        .events = POLLIN,
    };
    //fds[1] = (struct pollfd) {
        //.fd = STDIN_FILENO,
        //.events = POLLIN,
    //};
};

void inotifier_update() {
   //printf("inotifier_update\n");
    //fds[0].fd = STDIN_FILENO;       [> Console input <]
    //fds[0].events = POLLIN;

    int timeout = 0;
    int poll_num = poll(fds, 1, timeout);

    //printf("poll_num %d\n", poll_num);
    
    if (poll_num == -1) {
        if (errno == EINTR) {
            printf("inotifier_update: error %s", strerror(errno));
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
    printf("fd %d, events %d, revents %d\n",
            fds[0].fd,
            fds[0].events,
            fds[0].revents
          );
    */

    if (poll_num > 0 && fds[0].revents & POLLIN) {
        /*printf(" fds[0].revents & POLLIN %d\n", fds[0].revents & POLLIN);*/
        handle_events();
    }
}

void fnames_free() {
    for (int i = watched_num - 1; i >= 0; i--) {
        free(fnames[i]);
    }
}

void inotifier_shutdown() {
    printf("inotifier_shutdown\n");
    htable_free(tbl);
    close(fd);
    fnames_free();
}

void inotifier_watch(const char *fname, WatchCallback cb, void *data) {
    if (watched_num == MAX_WATCHED_FILES) {
        printf("inotifier_watch: MAX_WATCHED_FILES reached\n");
        return;
    }

    printf("inotifier_watch '%s' with data %p\n", fname, data);
    uint32_t mask = IN_ALL_EVENTS;
    //uint32_t mask = IN_MOVE_SELF;
    wd[watched_num] = inotify_add_watch(fd, fname, mask);
    fnames[watched_num] = strdup(fname);

    if (wd[watched_num] == -1) {
        printf("Cannot watch '%s': %s\n", fname, strerror(errno));
    }

    struct WatchContext ctx = {
        .cb = cb, .data = data,
    };
    htable_add(tbl, fname, strlen(fname) + 1, &ctx, sizeof(ctx));
    watched_num++;
}

void inotifier_remove_watch(const char *fname) {
    assert(fname);
    for (int i = 0; i < watched_num; i++) {
        if (!strcmp(fname, fnames[i])) {
            inotify_rm_watch(fd, wd[i]);
            htable_remove(tbl, fname, strlen(fname) + 1);
        }
    }
}

#endif

