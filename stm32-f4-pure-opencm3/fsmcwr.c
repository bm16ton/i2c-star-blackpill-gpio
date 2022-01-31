
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#include <libopencm3/stm32/fsmc.h>

void fsmc_write_burst_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_CBURSTRW;
}

void fsmc_write_burst_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_CBURSTRW;
}

void fsmc_extended_mode_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_EXTMOD;
}
void fsmc_extended_mode_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_EXTMOD;
}


void fsmc_write_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_WREN;
}
void fsmc_write_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_WREN;
}

void fsmc_wait_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_WAITEN;
}
void fsmc_wait_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_WAITEN;
}


void fsmc_wait_signal_polarity_low(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_WAITPOL;
}
void fsmc_wait_signal_polarity_high(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_WAITPOL;
}

void fsmc_wrapped_burst_mode_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_WRAPMOD;
}
void fsmc_wrapped_burst_mode_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_WRAPMOD;
}


void fsmc_burst_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_BURSTEN;
}
void fsmc_burst_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_BURSTEN;
}


void fsmc_wait_timing_configuration_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_WAITEN;
}
void fsmc_wait_timing_configuration_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_WAITEN;
}

void fsmc_address_data_multiplexing_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_MUXEN;
}
void fsmc_address_data_multiplexing_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_MUXEN;
}

void fsmc_memory_bank_enable(uint32_t bcr) {
    FSMC_BCR(bcr) |= FSMC_BCR_MBKEN;
}

void fsmc_memory_bank_disable(uint32_t bcr) {
    FSMC_BCR(bcr) &= ~FSMC_BCR_MBKEN;
}

void fsmc_set_access_mode(uint32_t bank, uint32_t mode) {
     FSMC_BTR(bank) |= FSMC_BTR_ACCMODx(mode);
}

void fsmc_set_data_latency(uint32_t bank, uint32_t lat) {
     FSMC_BTR(bank) |= FSMC_BTR_DATLATx(lat);
}

void fsmc_set_clock_divide_ratio(uint32_t bank, uint32_t div) {
     FSMC_BTR(bank) |= FSMC_BTR_CLKDIVx(div);
}

void fsmc_set_turnaround_phase_duration(uint32_t bank, uint32_t duration) {
     FSMC_BTR(bank) |= FSMC_BTR_BUSTURNx(duration);
}

void fsmc_set_data_phase_duration(uint32_t bank, uint32_t duration) {
     FSMC_BTR(bank) |= FSMC_BTR_DATASTx(duration);
}

void fsmc_set_address_hold_phase_duration(uint32_t bank, uint32_t duration) {
     FSMC_BTR(bank) |= FSMC_BTR_ADDHLDx(duration);
}

void fsmc_set_address_setup_phase_duration(uint32_t bank, uint32_t duration) {
     FSMC_BTR(bank) |= FSMC_BTR_ADDSETx(duration);
}

