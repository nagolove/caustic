// vim: set colorcolumn=85
// vim: fdm=marker

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// {{{ SparseSet implementation

/** The sparse set. */
// https://manenko.com/2021/05/23/sparse-sets.html
// https://gitlab.com/manenko/rho-sparse-set/-/blob/master/include/rho/sparse_set.h
typedef struct SparseSet {
    int64_t  *sparse; /**< Sparse array used to speed-optimise the set.    */
    int64_t  *dense;  /**< Dense array that stores the set's items.        */
    int64_t  n;      /**< Number of items in the dense array.             */
    int64_t  max;    /**< Maximum allowed item ID.                        */
} SparseSet;

// {{{ Sparse functions declarations

/**
 * Allocate the sparse set that can hold IDs up to max_id.
 *
 * The set can contain up to (max_id + 1) items.
 */
// test +
static inline SparseSet ss_alloc( int64_t max_id );

/**
 * Free the sparse set resources, previously allocated with ss_alloc.
 */
// test +
static inline void ss_free( SparseSet *ss );

/**
 * Remove all items from the sparse set but do not free memory used by the set.
 */
// test +
static inline void ss_clear( SparseSet *ss );

/**
 * Return the number of items in the sparse set.
 */
// test +
static inline size_t ss_size( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set is full and cannot add new
 * items.
 */
// test +
static inline bool ss_full( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set has no items.
 */
// test +
static inline bool ss_empty( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set contains the given item.
 */
// test +
static inline bool ss_has( SparseSet *ss, int64_t id );

/**
 * Add an item to the sparse set unless it is already there.
 */
//test+
static inline void ss_add( SparseSet *ss, int64_t id );

/**
 * Remove an item from the sparse set unless it is not there.
 */
//test+
static inline void ss_remove( SparseSet *ss, int64_t id );

// }}}


static inline SparseSet ss_alloc( int64_t max_id ) {
    /* Maximum possible value of the ss_id_t. */
    static const int64_t ss_id_max = INT64_MAX - 1;

    if (max_id < 0 || max_id >= ss_id_max) {
        printf("ss_alloc: max_id %ld is not in required range\n", max_id);
        exit(EXIT_FAILURE);
    }

    size_t   array_size = sizeof( int64_t ) * ( max_id + 1 );
    SparseSet ss         = { 0 };

    ss.dense  = calloc(1, array_size);
    ss.sparse = calloc(1, array_size);

    if ( !ss.dense || !ss.sparse ) {
        free( ss.dense );
        free( ss.sparse );

        ss.dense  = NULL;
        ss.sparse = NULL;
    } else {
        ss.max = max_id;
    }

    return ss;
}

static inline void ss_free( SparseSet *ss )
{
    assert( ss );

    free( ss->dense );
    free( ss->sparse );

    ss->dense  = NULL;
    ss->sparse = NULL;
    ss->max    = 0;
    ss->n      = 0;
}

static inline void ss_clear( SparseSet *ss )
{
    assert( ss );

    ss->n = 0;
}

static inline size_t ss_size( SparseSet *ss )
{
    assert( ss );

    return ss->n;
}

static inline bool ss_full( SparseSet *ss )
{
    assert( ss );

    return ss->n > ss->max;
}

static inline bool ss_empty( SparseSet *ss )
{
    assert( ss );

    return ss->n == 0;
}

static inline bool ss_has( SparseSet *ss, int64_t id )
{
    assert( ss );
    assert( id <= ss->max );
    assert(id >= 0);
    assert(ss->dense);
    assert(ss->sparse);

    size_t dense_index = ss->sparse[id];
    return dense_index < ss->n && ss->dense[dense_index] == id;
}

static inline void ss_add( SparseSet *ss, int64_t id)
{
    assert( ss );
    assert( id <= ss->max );

    if ( ss_has( ss, id ) ) {
        return;
    }

    ss->dense[ss->n] = id;
    ss->sparse[id]   = ss->n;

    ++ss->n;
}

static inline void ss_remove( SparseSet *ss, int64_t id )
{
    assert( ss );
    assert( id <= ss->max );

    if ( ss_has( ss, id ) ) {
        --ss->n;
        int64_t dense_index = ss->sparse[id];
        int64_t item        = ss->dense[ss->n];
        ss->dense[dense_index]  = item;
        ss->sparse[item]        = dense_index;
    }
}

// }}}

