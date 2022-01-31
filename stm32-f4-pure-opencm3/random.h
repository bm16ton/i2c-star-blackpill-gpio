/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#ifndef _RANDOM_H_ICU_
#define _RANDOM_H_ICU_

void rng_enable_interrupt(void);
void rng_disable_interrupt(void);
#if 0
void rng_enable(void);
void rng_disable(void);
#endif
uint32_t rng_random(void);
#endif

/* EOF */

