
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/pwr.h>

#include <stdlib.h>

#include <rtc4xx.h>


/*
  Sample:
    void rtc_init(void) {
        pwr_backup_domain_enable_write();
        rcc_backup_domain_software_reset();
    
        rcc_external_lowspeed_oscillator_enable();
        rcc_rtc_clock_enable();
        rcc_rtc_clock_source_selection(RCC_BDCR_RTCSEL_LSE);

        rtc_write_protection_disable();
        rtc_set_calibration_output_1hz();

        rtc_write_protection_disable();
        rtc_calibration_output_enable();

        rtc_init_mode_enable();

        rtc_write_prescaler(0x07, 0xFF);
        rtc_set_24h_format();

        rtc_init_mode_disable();

        rtc_write_protection_enable();
        pwr_backup_domain_disable_write();
}
 */



void pwr_backup_domain_enable_write(void) {
    PWR_CR |= PWR_CR_DBP;
}

void pwr_backup_domain_disable_write(void) {
    PWR_CR &= ~PWR_CR_DBP;
}

void rcc_backup_domain_software_reset(void) {
    RCC_BDCR |= RCC_BDCR_BDRST;
    RCC_BDCR &= ~RCC_BDCR_BDRST;
}

void rcc_external_lowspeed_oscillator_enable(void) {
    RCC_BDCR |= RCC_BDCR_LSEON;
    while (!(RCC_BDCR & RCC_BDCR_LSERDY));
}

void rcc_rtc_clock_enable(void) {
    RCC_BDCR |= RCC_BDCR_RTCEN;
}

void rcc_rtc_clock_disable(void) {
    RCC_BDCR &= ~RCC_BDCR_RTCEN;
}


void rcc_rtc_clock_source_selection(uint32_t source) {
    uint32_t reg = RCC_BDCR;
    reg &= ~(RCC_BDCR_RTCSEL_MASK << RCC_BDCR_RTCSEL_SHIFT);
    reg |= (source & RCC_BDCR_RTCSEL_MASK) << RCC_BDCR_RTCSEL_SHIFT;
    RCC_BDCR = reg;
}

void rtc_write_protection_disable(void) {
    RTC_WPR = 0xCA;
    RTC_WPR = 0x53;
}

void rtc_write_protection_enable(void) {
    RTC_WPR = 0xFF;
}

void rtc_set_calibration_output_1hz(void) {
    RTC_CR |= RTC_CR_COSEL;
}

void rtc_set_calibration_output_512hz(void) {
    RTC_CR &= ~RTC_CR_COSEL;
}


void rtc_calibration_output_enable(void) {
    RTC_CR |= RTC_CR_COE;
}

void rtc_calibration_output_disable(void) {
    RTC_CR &= ~RTC_CR_COE;
}


void rtc_write_prescaler(uint32_t sync, uint32_t async) {
    RTC_PRER = (sync << RTC_PRER_PREDIV_S_SHIFT);
    RTC_PRER |= (async << RTC_PRER_PREDIV_A_SHIFT);
}

void rtc_set_24h_format(void) {
    uint32_t reg = RTC_TR;
    reg &= ~RTC_TR_PM;
    RTC_TR = reg;
}

void rtc_set_12h_format(void) {
    uint32_t reg = RTC_TR;
    reg |= RTC_TR_PM;
    RTC_TR = reg;
}

void rtc_init_mode_enable(void) {
    RTC_ISR |= RTC_ISR_INIT;
    while (!(RTC_ISR & RTC_ISR_INITF));
}

void rtc_init_mode_disable(void) {
    RTC_ISR &= ~RTC_ISR_INIT;
    while (RTC_ISR & RTC_ISR_INITF);
}

void rtc_register_synchronization(void) {
    RTC_ISR &= ~(RTC_ISR_RSF);
    while (RTC_ISR & RTC_ISR_RSF);
}


void rtc_set_time_register(uint32_t tr) {
    pwr_backup_domain_enable_write();
    rtc_write_protection_disable();
    rtc_init_mode_enable();

    RTC_TR = tr;

    rtc_register_synchronization();
    rtc_init_mode_disable();
    rtc_write_protection_enable();
    pwr_backup_domain_disable_write();
}

void rtc_set_date_register(uint32_t dr) {
    pwr_backup_domain_enable_write();
    rtc_write_protection_disable();
    rtc_init_mode_enable();

    RTC_DR = dr;

    rtc_register_synchronization();
    rtc_init_mode_disable();
    rtc_write_protection_enable();
    pwr_backup_domain_disable_write();
}


/* EOF */
