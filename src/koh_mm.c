#include "koh_mm.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

struct MMArenaOpts mm_arena_opts_default = {
    .block_sz = 0,
    .capacity1 = 1000,
    .capacity2 = 1500,
    .mult2 = 1.5,
    .capacity3 = 10000,
    .mult3 = 1.5,
    .capacity4 = 100000,
    .mult4 = 1.5,
};

void mm_arena_init(MMArena *mm, struct MMArenaOpts opts) {
    assert(mm);
    assert(opts.capacity1 > 0);
    assert(opts.capacity2 > opts.capacity1);
    assert(opts.capacity3 > opts.capacity2);
    assert(opts.capacity4 > opts.capacity3);
    assert(opts.mult2 > 0.);
    assert(opts.mult3 > 0.);
    assert(opts.mult4 > 0.);
    assert(opts.block_sz > 0);

    memset(mm, 0, sizeof(*mm));
    mm->opts = opts;

    struct MMArenaOpts *o = &mm->opts;
    mm->arena = calloc(o->block_sz, o->capacity1);

    mm->blocks_allocated = calloc(
        sizeof(mm->blocks_allocated[0]),
        o->capacity1
    );
    mm->blocks_free = calloc(
        sizeof(mm->blocks_free[0]),
        o->capacity1
    );

}

void mm_arena_shutdown(MMArena *mm) {
    assert(mm);

    if (mm->arena) 
        free(mm->arena);
    if (mm->blocks_free)
        free(mm->blocks_free);
    if (mm->blocks_allocated)
        free(mm->blocks_allocated);

    memset(mm, 0, sizeof(*mm));
}

void *mm_arena_alloc(MMArena *mm) {
    assert(mm);
    
    // TODO: Проверить указатель на 
    //mm->arena >= ptr >= mm->arena + mm->cap * mm->blocks_allocated

    if (mm->num_a + 1 == mm->cap) {
        // realloc here please
    }

    if (mm->num_f == 0) {
        printf("mm_arena_alloc: not enough memmory\n");
        abort();
    }

    void *ptr = mm->blocks_free[mm->num_f--];
    return ptr;
}

void mm_arena_free(MMArena *mm, void *ptr) {
    assert(mm);
    assert(ptr);

    // TODO: Проверить указатель на 
    //mm->arena >= ptr >= mm->arena + mm->cap * mm->blocks_allocated
    
    // TODO: Проверить указатель на гранулярность блок
    // (ptr - mm->arena) % mm->opts.block_sz == 0

    mm->blocks_free[mm->num_f++] = ptr;
}

