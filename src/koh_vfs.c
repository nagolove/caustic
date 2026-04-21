#include "koh_vfs.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "physfs.h"
#include "koh_routine.h"

void vfs_init(
    int argc, char **argv, const char *archive
) {
    PHYSFS_init(argv[0]);

    bool archive_only = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i],
            "--vfs-archive-only") == 0)
            archive_only = true;
    }

    if (!archive_only)
        PHYSFS_mount(".", NULL, 0);

    if (archive) {
        int append = archive_only ? 0 : 1;
        if (PHYSFS_mount(archive, NULL, append))
            printf(
                "vfs: %s смонтирован\n", archive
            );
        else
            printf(
                "vfs: ошибка монтирования %s\n",
                archive
            );
    }
}

void vfs_shutdown() {
    PHYSFS_deinit();
}

void *vfs_load(const char *vpath, size_t *out_sz, bool add_null_term) {
    assert(vpath);
    assert(out_sz);

    *out_sz = 0;

    PHYSFS_File* f = PHYSFS_openRead(vpath);
    if (!f) {
        printf(
            "read_entire_file: could not load '%s' by PHYSFS_openRead()\n",
            vpath
        );
        koh_fatal();
    }

    PHYSFS_sint64 len64 = PHYSFS_fileLength(f);
    if (len64 < 0) {
        PHYSFS_close(f);
        printf("read_entire_file: PHYSFS_fileLength failed");
        koh_fatal();
    }

    // На всякий случай проверим, что влезает в size_t (актуально для 32-бит).
    if ((unsigned long long)len64 > (unsigned long long)(SIZE_MAX - (add_null_term ? 1 : 0))) {
        PHYSFS_close(f);
        printf("read_entire_file: file too large: %s\n", vpath);
        return NULL;
    }

    size_t len = (size_t)len64;
    size_t allocSize = len + (add_null_term ? 1 : 0);

    void* data = malloc(allocSize);
    if (!data) {
        PHYSFS_close(f);
        printf("read_entire_file: out of memory allocating %zu bytes\n", allocSize);
        return NULL;
    }

    PHYSFS_sint64 got = PHYSFS_readBytes(f, data, (PHYSFS_sint64)len);
    if (got < 0 || (size_t)got != len) {
        //free(data);
        PHYSFS_close(f);
        printf("read_entire_file: PHYSFS_readBytes (whole file) failed\n");
        return NULL;
    }

    if (add_null_term) 
        ((char*)data)[len] = 0;

    if (!PHYSFS_close(f)) {
        //free(data);
        printf("read_entire_file: PHYSFS_close failed\n");
    }

    *out_sz = len;
    return data;
}

void *vfs_try_load(
    const char *vpath, size_t *out_sz,
    bool add_null_term
) {
    assert(vpath);
    assert(out_sz);
    *out_sz = 0;

    if (!PHYSFS_isInit())
        return NULL;
    if (!PHYSFS_exists(vpath))
        return NULL;

    PHYSFS_File *f = PHYSFS_openRead(vpath);
    if (!f)
        return NULL;

    PHYSFS_sint64 len64 = PHYSFS_fileLength(f);
    if (len64 < 0) {
        PHYSFS_close(f);
        return NULL;
    }

    size_t len = (size_t)len64;
    size_t alloc_sz =
        len + (add_null_term ? 1 : 0);
    void *data = malloc(alloc_sz);
    if (!data) {
        PHYSFS_close(f);
        return NULL;
    }

    PHYSFS_sint64 got =
        PHYSFS_readBytes(f, data, (PHYSFS_sint64)len);
    PHYSFS_close(f);

    if (got < 0 || (size_t)got != len) {
        free(data);
        return NULL;
    }

    if (add_null_term)
        ((char *)data)[len] = 0;

    *out_sz = len;
    return data;
}
