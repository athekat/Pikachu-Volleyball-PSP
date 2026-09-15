/*
 * rand.c - PRNG matching the original game's _rand() (Visual Studio LCG).
 */
#include "rand.h"

static unsigned int g_rand_state = 1u;

void pika_srand(unsigned int seed) {
    g_rand_state = seed;
}

int pika_rand(void) {
    g_rand_state = g_rand_state * 214013u + 2531011u;
    return (int)((g_rand_state >> 16) & 0x7FFFu);
}
