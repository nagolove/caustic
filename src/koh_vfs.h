#pragma once

#include <stdlib.h>
#include <stdbool.h>

void vfs_init(char *argv0);
void vfs_shutdown();
void *vfs_load(const char *vpath, size_t *out_sz, bool add_null_term);

// Мягкая загрузка: NULL если VFS не инициализирован
// или файл не найден. Не вызывает koh_fatal().
void *vfs_try_load(
    const char *vpath, size_t *out_sz,
    bool add_null_term
);
