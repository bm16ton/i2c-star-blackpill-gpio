
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <libopencm3/cm3/dwt.h>

#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/fsmc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/rng.h>
#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/exti.h>


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wctype.h>
#include <ctype.h>
#include <locale.h>
#include <wchar.h>
#include <time.h>

#include <syscall.h>
#include <fsmcwr.h>

#include <buffer.h>
#include <uastdio.h>
#include <ili9341.h>
#include <console.h>
#include <random.h>

#include <rtc4xx.h>
#include <xpt2046.h>

volatile uint32_t loop_counter = 0;
volatile uint32_t cpu_counter = 0;

volatile uint32_t rate_value = 0;
volatile uint32_t cpu_rate_value = 0;

volatile static uint32_t systick_counter = 0;

void delay(uint32_t n) {
    for (volatile int i = 0; i < n * 10; i++)
        __asm__("nop");
}

void _delay_ms(uint32_t n) {
    for (volatile int i = 0; i < n * 80000; i++)
        __asm__("nop");
}


static void clock_setup(void) {
    rcc_clock_setup_hse_3v3(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_GPIOD);
    rcc_periph_clock_enable(RCC_GPIOE);

    rcc_periph_clock_enable(RCC_SPI2);
    rcc_periph_clock_enable(RCC_SYSCFG);

    rcc_periph_clock_enable(RCC_FSMC);

    rcc_periph_clock_enable(RCC_TIM2);
    rcc_periph_clock_enable(RCC_TIM7);
    rcc_periph_clock_enable(RCC_USART1);

    rcc_periph_clock_enable(RCC_PWR);
    rcc_periph_clock_enable(RCC_RTC);
    rcc_periph_clock_enable(RCC_RNG);


}

/*** USART ***/
void usart_setup(void) {

    usart_disable(USART1);
    nvic_enable_irq(NVIC_USART1_IRQ);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);

    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);
    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
    usart_set_mode(USART1, USART_MODE_TX_RX);

    usart_enable_rx_interrupt(USART1);

    usart_enable(USART1);
}

inline bool usart_recv_is_ready(uint32_t usart) {
    return (USART_SR(usart) & USART_SR_RXNE);
}

inline bool usart_rx_int_is_enable(uint32_t usart) {
    return (USART_CR1(USART1) & USART_CR1_RXNEIE);
}

void usart1_isr(void) {
    static uint8_t data = 0;

    if (usart_rx_int_is_enable(USART1) && usart_recv_is_ready(USART1)) {
        data = usart_recv(USART1);
        buffer_put_byte(&stdin_buffer, data);
        buffer_put_byte(&stdout_buffer, data);
        if (data == '\r')
            buffer_put_byte(&stdout_buffer, '\n');
    }
}

/*** TIM2 ***/
void tim2_setup(void) {
    nvic_enable_irq(NVIC_TIM2_IRQ);
    //timer_reset(TIM2);

    timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_direction_up(TIM2);
    timer_set_prescaler(TIM2, 64);
    timer_disable_preload(TIM2);
    timer_continuous_mode(TIM2);
    timer_set_period(TIM2, 64);
    timer_set_oc_value(TIM2, TIM_OC1, 1);

    timer_enable_irq(TIM2, TIM_DIER_CC1IE);
    timer_enable_counter(TIM2);
}

void tim2_isr(void) {
    if (timer_get_flag(TIM2, TIM_SR_CC1IF)) {
        timer_clear_flag(TIM2, TIM_SR_CC1IF);

        uint8_t c;
        while ((c = buffer_get_byte(&stdout_buffer)) > 0)
            usart_send_blocking(USART1, c);
    }
}

/*** TIM7 ***/
void tim7_setup(void) {
    //timer_reset(TIM7);
    timer_set_prescaler(TIM7, rcc_apb2_frequency / 1000);
    timer_set_period(TIM7, 10000);

    nvic_enable_irq(NVIC_TIM7_IRQ);
    timer_enable_update_event(TIM7);
    timer_enable_irq(TIM7, TIM_DIER_UIE);
    timer_enable_counter(TIM7);
}

void tim7_isr(void) {
    if (timer_get_flag(TIM7, TIM_SR_UIF)) {
        timer_clear_flag(TIM7, TIM_SR_UIF);
        rate_value = loop_counter;
        loop_counter = 0;

        volatile uint32_t cpu_counter_new = dwt_read_cycle_counter();
        cpu_rate_value = cpu_counter_new - cpu_counter;
        cpu_counter = cpu_counter_new;
    }
}


/*** SYSTICK ***/

static void systick_setup(void) {
    //systick_set_reload(168000);
    //systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_frequency(1, rcc_ahb_frequency / 1000);
    systick_counter_enable();
    systick_interrupt_enable();
}

void sys_tick_handler(void) {
    systick_counter++;
}

void delay_ms(uint32_t delay) {
    uint32_t wake = systick_counter + delay;
    while (wake > systick_counter);
}



/*** FSMC ***/

void fsmc_setup(void) {
    /*
       D0   PD14    D8   PE11   NOE  PD4
       D1   PD15    D9   PE12   NWE  PD5
       D2   PD0     D10  PE13   NE1  PD7
       D3   PD1     D11  PE14   A18  PD13
       D12  PE15
       D4   PE7     
       D5   PE8     D13  PD8
       D6   PE9     D14  PD9
       D7   PE10    D15  PD10
     */

#define FSMC_PD (GPIO4 | GPIO5 | GPIO7 | GPIO13 | GPIO14 | GPIO15 | GPIO0 | GPIO1 | GPIO8 | GPIO9 | GPIO10)
#define FSMC_PE (GPIO7 | GPIO8 | GPIO9 | GPIO10 | GPIO11 | GPIO12 | GPIO13 | GPIO14 | GPIO15)

    gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, FSMC_PD);
    gpio_set_output_options(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, FSMC_PD);
    gpio_set_af(GPIOD, GPIO_AF12, FSMC_PD);

    gpio_mode_setup(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, FSMC_PE);
    gpio_set_output_options(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, FSMC_PE);
    gpio_set_af(GPIOE, GPIO_AF12, FSMC_PE);

    /*
       FSMC_BTR(0) = FSMC_BTR_ACCMODx(FSMC_BTx_ACCMOD_B) | 
       FSMC_BTR_DATLATx(0) |
       FSMC_BTR_CLKDIVx(0) |
       FSMC_BTR_BUSTURNx(0) |
       FSMC_BTR_DATASTx(5) |
       FSMC_BTR_ADDHLDx(0) |
       FSMC_BTR_ADDSETx(1);
     */

    /* FSMC_BCR(0) = FSMC_BCR_WREN | FSMC_BCR_MWID | FSMC_BCR_MBKEN; */

    fsmc_set_access_mode(BANK1, FSMC_BTx_ACCMOD_B);
    fsmc_set_data_latency(BANK1, 0);
    fsmc_set_clock_divide_ratio(BANK1, 0);
    fsmc_set_turnaround_phase_duration(BANK1, 0);
    fsmc_set_data_phase_duration(BANK1, 5);
    fsmc_set_address_hold_phase_duration(BANK1, 0);
    fsmc_set_address_setup_phase_duration(BANK1, 1);


    fsmc_address_data_multiplexing_disable(BANK1);
    fsmc_write_burst_disable(BANK1);
    fsmc_wrapped_burst_mode_disable(BANK1);
    fsmc_extended_mode_disable(BANK1);
    fsmc_write_enable(BANK1);

    fsmc_wait_signal_polarity_low(BANK1);
    fsmc_wait_timing_configuration_enable(BANK1);
    fsmc_wait_disable(BANK1);
    fsmc_memory_bank_enable(BANK1);
}


void demo_gpio_setup(void) {
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO6);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7);
}

/*** RTC ***/
void rtc_init(void) {

    pwr_backup_domain_enable_write();
    //rcc_backup_domain_software_reset();

    rcc_rtc_clock_enable();
    rcc_external_lowspeed_oscillator_enable();
    rcc_rtc_clock_source_selection(RCC_BDCR_RTCSEL_LSE);
    rtc_write_protection_disable();
    rtc_set_calibration_output_1hz();
    rtc_write_protection_disable();
    rtc_calibration_output_disable();

    rtc_init_mode_enable();
    rtc_write_prescaler(0x7F, 0xFF);

    rtc_init_mode_disable();
    rtc_write_protection_enable();
    pwr_backup_domain_disable_write();
}


/*** EXTI ***/

#define TS_IRQ_PORT  GPIOC
#define TS_IRQ_PIN   GPIO5

static void exti5_setup(void) {
    nvic_enable_irq(NVIC_EXTI9_5_IRQ);

    gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO5);

    exti_set_trigger(EXTI5, EXTI_TRIGGER_BOTH);
    exti_select_source(EXTI5, GPIOC);
    exti_enable_request(EXTI5);
}

#define TS_PRS  1
#define TS_RLS  2

volatile static uint8_t ts_prev_status = TS_PRS;
volatile static uint8_t ts_curr_status = TS_RLS;


volatile static uint32_t ts_press_time = 0;
volatile static uint32_t ts_release_time = 0;

#define HOLD_TIME_MIN   10

void exti9_5_isr(void) {

    if (exti_get_flag_status(EXTI5)) {
        exti_reset_request(EXTI5);

        if (gpio_get(TS_IRQ_PORT, TS_IRQ_PIN)) {
            ts_curr_status = TS_RLS;
        } else {
            ts_curr_status = TS_PRS;
        }

        if ((ts_prev_status == TS_RLS) && (ts_curr_status == TS_PRS)) {
            ts_prev_status = ts_curr_status;
            ts_press_time = systick_counter;
            return;
        }

        if ((ts_prev_status == TS_PRS) && (ts_curr_status == TS_RLS)) {
            ts_prev_status = ts_curr_status;
            ts_release_time = systick_counter;
            uint32_t hold_time = ts_release_time - ts_press_time;

            if (hold_time > HOLD_TIME_MIN) {
                printf("TS UP %d\r\n", hold_time);
            }

            return;
        }
    }
}

int main(void) {
    clock_setup();
    systick_setup();
    io_setup();
    usart_setup();
    tim2_setup();
    tim7_setup();
    delay(10);

    fsmc_setup();
    lcd_setup();
    lcd_clear();
    ts_spi_setup();
    exti5_setup();

    //rng_enable();
    //dwt_enable_cycle_counter();

    uint32_t i = 1;

    console_xyputs(&console, 0, 0, "STM32-F4 CONSOLE V0.1");
    console_xyputs(&console, 1, 0, "SYS READY>");

    //rtc_init();

    while (1) {
        console_xyputs(&console, 0, 0, "STM32-F4 CONSOLE V0.1");

#define STR_LEN 20
        uint8_t str[STR_LEN + 1];

        snprintf(str, STR_LEN, "RT %4u CPU %4.2f", rate_value,
                 (cpu_rate_value * 1.0) / 168000000.0f);
        console_xyputs(&console, 2, 0, str);

#if 0
        snprintf(str, STR_LEN, "RTC_TR 0x%08lX", RTC_TR);
        console_xyputs(&console, 5, 0, str);
        snprintf(str, STR_LEN, "RTC_DR 0x%08lX", RTC_DR);
        console_xyputs(&console, 6, 0, str);
#endif

        snprintf(str, STR_LEN, "0x%08X", i);
        console_xyputs(&console, 16, 0, str);

#if 1

        uint16_t y_value = i % LCD_HEIGHT;
        uint16_t y_prev;

        int16_t x_value = i % LCD_WIDTH;
        int16_t x_prev;
        if ((x_prev != x_value) && (y_prev != y_value)) {
            lcd_write_rect(20, y_prev, 10, 10, LCD_BLUE);
            lcd_write_rect(20, y_value, 10, 10, LCD_GREEN);
        }
        y_prev = y_value;
        x_prev = x_value;
#endif

        while (ts_curr_status == TS_PRS) {
            uint16_t x = ts_get_x();
            uint16_t y = ts_get_y();
            if (x < 25 && y < 25)
                lcd_clear();
            lcd_draw_pixel(x, y, LCD_BLUE);
        }

        delay_ms(10);
        loop_counter++;
        i++;
    }
    return 0;
}

/* EOF */
