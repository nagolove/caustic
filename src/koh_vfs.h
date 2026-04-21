#pragma once

#include <stdlib.h>
#include <stdbool.h>

// Инициализация VFS: монтирование каталога и/или архива.
// archive — путь к архиву (NULL — без архива).
// Флаг --vfs-archive-only — только архив, без каталога.
void vfs_init(
    int argc, char **argv, const char *archive
);
void vfs_shutdown();
void *vfs_load(
    const char *vpath, size_t *out_sz,
    bool add_null_term
);

// Мягкая загрузка: NULL если VFS не инициализирован
// или файл не найден. Не вызывает koh_fatal().
void *vfs_try_load(
    const char *vpath, size_t *out_sz,
    bool add_null_term
);
