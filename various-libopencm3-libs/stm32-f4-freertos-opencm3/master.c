
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/scb.h>

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

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>

/* Object definition */

#define IRQ2NVIC_PRIOR(x)       ((x) << 4)
#define UART_QUEUE_LEN      1024
#define CONSOLE_QUEUE_LEN   8

xTaskHandle usart1_task_h;
xTaskHandle counter_task_h;
xTaskHandle console_task_h;


volatile QueueHandle_t usart1_q;
volatile QueueHandle_t console_q;

#define CONSOLE_STR_LEN 32
#define STR_LEN 16

typedef struct console_message_t {
    uint8_t row;
    uint8_t col;
    uint8_t str[CONSOLE_STR_LEN + 1];
} console_message_t;


/* Generic */
void delay(uint32_t n) {
    for (volatile int i = 0; i < n * 10; i++)
        __asm__("nop");
}

void _delay_ms(uint32_t n) {
    for (volatile int i = 0; i < n * 80000; i++)
        __asm__("nop");
}

/* RCC CLOCK */
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

/* USART generic */

inline bool usart_recv_is_ready(uint32_t usart) {
    return (USART_SR(usart) & USART_SR_RXNE);
}

inline bool usart_rx_int_is_enable(uint32_t usart) {
    return (USART_CR1(USART1) & USART_CR1_RXNEIE);
}

/* USART */
void usart1_setup(void) {

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


void usart1_isr(void) {
    static uint8_t data = 0;

    if (usart_rx_int_is_enable(USART1) && usart_recv_is_ready(USART1)) {
        data = usart_recv(USART1);
    }
}


/* FSMC */

#define FSMC_PD (GPIO4 | GPIO5 | GPIO7 | GPIO13 | GPIO14 | GPIO15 | GPIO0 | GPIO1 | GPIO8 | GPIO9 | GPIO10)
#define FSMC_PE (GPIO7 | GPIO8 | GPIO9 | GPIO10 | GPIO11 | GPIO12 | GPIO13 | GPIO14 | GPIO15)

void fsmc_setup(void) {

    gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, FSMC_PD);
    gpio_set_output_options(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, FSMC_PD);
    gpio_set_af(GPIOD, GPIO_AF12, FSMC_PD);

    gpio_mode_setup(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, FSMC_PE);
    gpio_set_output_options(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, FSMC_PE);
    gpio_set_af(GPIOE, GPIO_AF12, FSMC_PE);

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

/* TASKs */
static void usart1_task(void *args __attribute__ ((unused))) {
    uint8_t c;
    while (1) {
        if (xQueueReceive(usart1_q, &c, 10) == pdPASS) {
            while (!usart_get_flag(USART1, USART_SR_TXE))
                taskYIELD();
            usart_send_blocking(USART1, c);

        } else {
            taskYIELD();
        }
    }
}

void send_console_msg(uint8_t row, uint8_t col, uint8_t * str) {
    console_message_t msg;
    msg.row = row;
    msg.col = col;
    memcpy(msg.str, str, CONSOLE_STR_LEN);
    xQueueSend(console_q, &msg, portMAX_DELAY);
}

#define MAX_TASK_COUNT  6
void print_stats(void) {
    volatile UBaseType_t task_count = MAX_TASK_COUNT;
    TaskStatus_t *status_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));

    if (status_array != NULL) {
        uint32_t total_time, stat_as_percentage;
        task_count = uxTaskGetSystemState(status_array, task_count, &total_time);

        total_time /= 100UL;
        if (total_time > 0) {
            uint16_t row = 3;
            for (UBaseType_t x = 0; x < task_count; x++) {
                stat_as_percentage = status_array[x].ulRunTimeCounter / total_time;

                if (stat_as_percentage >= 0UL) {
                    #define CONSOLE_MAX_STAT_ROW 8
                    if (row < CONSOLE_MAX_STAT_ROW) {
                        console_message_t msg;
                        msg.row = row;
                        msg.col = 0;
                        snprintf(msg.str, CONSOLE_STR_LEN, "%-5s %3d %3u",
                                 status_array[x].pcTaskName, stat_as_percentage, status_array[x].usStackHighWaterMark);
                        xQueueSend(console_q, &msg, portMAX_DELAY);
                    }
                    row++;
                }
            }
        }
    }
    vPortFree(status_array);
}

static void counter_task(void *args __attribute__ ((unused))) {
    uint32_t i = 0;
    uint8_t str[CONSOLE_STR_LEN + 1];
    while (1) {

        print_stats();

        snprintf(str, CONSOLE_STR_LEN, "0x%08X", i);
        send_console_msg(16, 0, str);

        vTaskDelay(pdMS_TO_TICKS(500));
        i++;
    }
}

static void console_task(void *args __attribute__ ((unused))) {
    console_message_t msg;
    while (1) {
        if (xQueueReceive(console_q, &msg, 10) == pdPASS) {
            console_xyputs(&console, msg.row, msg.col, msg.str);
        } else {
            taskYIELD();
        }
    }
}


/* MAIN */
int main(void) {

    scb_set_priority_grouping(SCB_AIRCR_PRIGROUP_GROUP16_NOSUB);
    nvic_set_priority(NVIC_SYSTICK_IRQ, IRQ2NVIC_PRIOR(15));
    nvic_set_priority(NVIC_USART1_IRQ, configMAX_SYSCALL_INTERRUPT_PRIORITY + IRQ2NVIC_PRIOR(1));

    clock_setup();
    usart1_setup();

    delay(100);

    fsmc_setup();
    lcd_setup();
    lcd_clear();

    uint32_t i = 1;
    uint8_t str[STR_LEN + 1];

    console_xyputs(&console, 0, 0, "FreeRTOS STM32-F4 CONSOLE V0.2");
    console_xyputs(&console, 1, 0, "SYS READY>");

    usart1_q = xQueueCreate(UART_QUEUE_LEN, sizeof(uint8_t));
    console_q = xQueueCreate(CONSOLE_QUEUE_LEN, sizeof(console_message_t));

    xTaskCreate(usart1_task, "UAR1", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, usart1_task_h);
    xTaskCreate(counter_task, "CNTR", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, counter_task_h);
    xTaskCreate(console_task, "CONS", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, console_task_h);

    vTaskStartScheduler();

    return 0;
}

/* EOF */
