#pragma once

#include <stdlib.h>

/*
    Блочный менеджер памяти с гибкой политикой выделения.
 */

typedef struct MMArenaOpts {
    // Начальный запас блоков, размер блока
    size_t capacity1, block_sz;
    // Порог количества элементов, после которого будет вызов realloc()
    size_t capacity2, capacity3, capacity4;
    // Коээфициент на который домножается cap при достижении данного порога.
    float  mult2, mult3, mult4;
} MMArenaOpts;

typedef struct MMArena {
    char               *arena;
    size_t             cap, 
                       num_a, // количество выделенных
                       num_f; // количество свободных
    struct MMArenaOpts opts;
    void               **blocks_allocated, //
                       **blocks_free;
} MMArena;

extern struct MMArenaOpts mm_arena_opts_default;

void mm_arena_init(MMArena *mm, struct MMArenaOpts opts);
void mm_arena_shutdown(MMArena *mm);

void *mm_arena_alloc(MMArena *mm);
void mm_arena_free(MMArena *mm, void *ptr);
