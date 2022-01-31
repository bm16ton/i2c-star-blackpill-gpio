
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#ifndef FSMCWR_H
#define FSMCWR_H

#define BANK1 0

void fsmc_write_burst_disable(uint32_t bcr);
void fsmc_write_burst_enable(uint32_t bcr);
void fsmc_extended_mode_disable(uint32_t bcr);
void fsmc_extended_mode_enable(uint32_t bcr);
void fsmc_write_disable(uint32_t bcr);
void fsmc_write_enable(uint32_t bcr);
void fsmc_wait_disable(uint32_t bcr);
void fsmc_wait_enable(uint32_t bcr);
void fsmc_wait_signal_polarity_low(uint32_t bcr);
void fsmc_wait_signal_polarity_high(uint32_t bcr);
void fsmc_wrapped_burst_mode_disable(uint32_t bcr);
void fsmc_wrapped_burst_mode_enable(uint32_t bcr);
void fsmc_burst_disable(uint32_t bcr);
void fsmc_burst_enable(uint32_t bcr);
void fsmc_wait_timing_configuration_disable(uint32_t bcr);
void fsmc_wait_timing_configuration_enable(uint32_t bcr);
void fsmc_address_data_multiplexing_disable(uint32_t bcr);
void fsmc_address_data_multiplexing_enable(uint32_t bcr);
void fsmc_memory_bank_enable(uint32_t bcr);
void fsmc_memory_bank_disable(uint32_t bcr);
void fsmc_set_access_mode(uint32_t bank, uint32_t mode);
void fsmc_set_data_latency(uint32_t bank, uint32_t lat);
void fsmc_set_clock_divide_ratio(uint32_t bank, uint32_t div);
void fsmc_set_turnaround_phase_duration(uint32_t bank, uint32_t duration);
void fsmc_set_data_phase_duration(uint32_t bank, uint32_t duration);
void fsmc_set_address_hold_phase_duration(uint32_t bank, uint32_t duration);
void fsmc_set_address_setup_phase_duration(uint32_t bank, uint32_t duration);

#endif
