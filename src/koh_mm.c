#include "koh_mm.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

struct MMArenaOpts mm_arena_opts_default = {
    .block_sz = 0,
    .initial_capacity = 1000,
    .threshold1 = 2000,
    .mult1 = 1.5,
    .threshold2 = 2000,
    .mult2 = 1.5,
    .threshold3 = 2000,
    .mult3 = 1.5,
};

void mm_arena_init(MMArena *mm, struct MMArenaOpts opts) {
    assert(mm);
    assert(opts.initial_capacity > 0);
    assert(opts.threshold1 > opts.initial_capacity);
    assert(opts.threshold2 > opts.threshold1);
    assert(opts.threshold3 > opts.threshold2);
    assert(opts.mult1 > 0.);
    assert(opts.mult2 > 0.);
    assert(opts.mult3 > 0.);
    assert(opts.block_sz > 0);

    memset(mm, 0, sizeof(*mm));
    mm->opts = opts;

    struct MMArenaOpts *o = &mm->opts;
    mm->arena = calloc(o->block_sz, o->initial_capacity);

    mm->blocks_allocated = calloc(
        sizeof(mm->blocks_allocated[0]),
        o->initial_capacity
    );
    mm->blocks_free = calloc(
        sizeof(mm->blocks_free[0]),
        o->initial_capacity
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

    mm->blocks_free[mm->num_f++] = ptr;
}

