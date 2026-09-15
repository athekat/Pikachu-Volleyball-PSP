/*
 * rand.h - PRNG matching the original game's _rand() (Visual Studio LCG).
 *
 * The JavaScript port notes it uses the Visual Studio rand() which returns an
 * integer in [0, 32767].  The Microsoft C runtime rand() is the LCG:
 *     state = state * 214013 + 2531011;  return (state >> 16) & 0x7FFF;
 */
#ifndef PIKA_RAND_H
#define PIKA_RAND_H

void pika_srand(unsigned int seed);

/* Return a pseudo-random integer in [0, 32767]. */
int pika_rand(void);

#endif /* PIKA_RAND_H */
