
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#include <libopencm3/stm32/rng.h>

#include <stdlib.h>

void rng_enable_interrupt(void) {
    RNG_CR |= RNG_CR_IE;
}

void rng_disable_interrupt(void) {
    RNG_CR &= ~RNG_CR_IE;
}


#if 0
void rng_enable(void) {
    RNG_CR |= RNG_CR_RNGEN;
}

void rng_disable(void) {
    RNG_CR &= ~RNG_CR_RNGEN;
}
#endif

uint32_t rng_random(void) {
    static uint32_t last_value;
    static uint32_t new_value;

    uint32_t error = 0;
    error = RNG_SR_SEIS | RNG_SR_CEIS;
    while (new_value == last_value) {
        if (((RNG_SR & error) == 0) && ((RNG_SR & RNG_SR_DRDY) == 1)) {
            new_value = RNG_DR;
        }
    }
    last_value = new_value;
    return new_value;
}

/* EOF */
