
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#ifndef RTC4XX_H_XYZ
#define RTC4XX_H_XYZ

void pwr_backup_domain_enable_write(void);
void pwr_backup_domain_disable_write(void);
void rcc_backup_domain_software_reset(void);
void rcc_external_lowspeed_oscillator_enable(void);
void rcc_rtc_clock_enable(void);
void rcc_rtc_clock_disable(void);
void rcc_rtc_clock_source_selection(uint32_t source);
void rtc_write_protection_disable(void);
void rtc_write_protection_enable(void);
void rtc_set_calibration_output_1hz(void);
void rtc_set_calibration_output_512hz(void);
void rtc_calibration_output_enable(void);
void rtc_calibration_output_disable(void);
void rtc_write_prescaler(uint32_t async, uint32_t sync);
void rtc_set_24h_format(void);
void rtc_set_12h_format(void);
void rtc_init_mode_enable(void);
void rtc_init_mode_disable(void);
void rtc_register_synchronization(void);
void rtc_set_time_register(uint32_t tr);
void rtc_set_date_register(uint32_t dr);

#endif

/* EOF */