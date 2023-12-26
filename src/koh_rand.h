#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

typedef struct xorshift32_state {
  uint32_t a;
} xorshift32_state;

typedef xorshift32_state prng_32;

static inline xorshift32_state xorshift32_init(void) {
    return (xorshift32_state) { .a = (uint32_t)time(NULL) };
}

/* The state word must be initialized to non-zero */
// XXX Каковы границы числа из генератора?
static inline uint32_t xorshift32_rand(xorshift32_state *state)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint32_t x = state->a;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return state->a = x;
}

static inline double xorshift32_rand1(xorshift32_state *state) {
    return xorshift32_rand(state) / (double)UINT32_MAX;
}

typedef struct xorshift64_state {
    uint64_t a;
} prng;

static inline prng prng_init(void) {
    return (prng) { .a = (uint64_t)time(NULL) };
}

static inline uint32_t prng_rand(prng *state)
{
    uint64_t x = state->a;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return state->a = x * 0x2545F4914F6CDD1DULL;
}

static inline double prng_rand1(prng *state) {
    return prng_rand(state) / (double)UINT32_MAX;
}
