#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "pwm.h"

#define TIM_CLOCK_FREQ_HZ					1000000	/* 1MHz */

void pwm_setup_timer(enum rcc_periph_clken 	clken, uint32_t timer_peripheral, uint32_t prescaler, uint32_t period)
{
     (void)clken;
     (void)prescaler;
     (void)timer_peripheral;
     
     /* Enable timer clock. */
     //rcc_peripheral_enable_clock(reg, en); version online
//     rcc_periph_clock_enable(clken);
     rcc_periph_clock_enable(RCC_TIM1);

     /* Reset TIM1 peripheral */
     //timer_reset(timer_peripheral);
//    rcc_periph_reset_pulse(RST_(timer_peripheral));
     /* Set the timers global mode to:
      * - use no divider
      * - alignment edge
      * - count direction up
      */
     timer_set_mode(TIM1,
                    TIM_CR1_CKD_CK_INT,
                    TIM_CR1_CMS_EDGE,
                    TIM_CR1_DIR_UP);

     timer_set_prescaler(TIM1, ((rcc_apb2_frequency * 2) / TIM_CLOCK_FREQ_HZ));
     timer_set_repetition_counter(TIM1, 0);
     timer_enable_preload(TIM1);
     timer_continuous_mode(TIM1);
     timer_set_period(TIM1, period);
}

void pwm_setup_output_channel(uint32_t timer_peripheral, enum tim_oc_id oc_id, enum rcc_periph_clken clken, uint32_t gpio_port, uint16_t gpio_pin)
{
     (void)clken;
     (void)timer_peripheral;
     /* Enable GPIO clock. */
     //rcc_peripheral_enable_clock(gpio_reg, gpio_en); version online
//     rcc_periph_clock_enable(clken);
     rcc_periph_clock_enable(RCC_TIM1);

     /* Set timer channel to output */
//     gpio_mode_setup(gpio_port,GPIO_MODE_AF,GPIO_PUPD_NONE,gpio_pin);
//     gpio_set_output_options(gpio_port, GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ, gpio_pin);     
//	 gpio_set_af(gpio_port,GPIO_AF1,gpio_pin);


    timer_set_deadtime(TIM1, 10);
	timer_set_enabled_off_state_in_idle_mode(TIM1);
	timer_set_enabled_off_state_in_run_mode(TIM1);
	timer_disable_break(TIM1);
	timer_set_break_polarity_high(TIM1);
	timer_disable_break_automatic_output(TIM1);
	timer_set_break_lock(TIM1, TIM_BDTR_LOCK_OFF);
	
	/* Enable GPIOD clock */
	rcc_periph_clock_enable(RCC_GPIOA);

	/* Set GPIO12, GPIO13, GPIO14, GPIO15 (in GPIO port D) to Alternate Function */
	gpio_mode_setup(GPIOA,
					GPIO_MODE_AF,
					GPIO_PUPD_NONE,
					GPIO8 | GPIO9 | GPIO10);

	/* Push Pull, Speed 50 MHz */
	gpio_set_output_options(GPIOA,
							GPIO_OTYPE_PP,
							GPIO_OSPEED_50MHZ,
							GPIO8 | GPIO9 | GPIO10);

	/* Alternate Function: TIM1 CH1/2/3/4 */
	gpio_set_af(GPIOA,
				GPIO_AF1,
				GPIO8 | GPIO9 | GPIO10);
				
//     timer_disable_oc_output(TIM1, oc_id);
     timer_disable_oc_output(TIM1, TIM_OC1);
     
//     timer_set_oc_mode(TIM1, oc_id, TIM_OCM_PWM1);
	 timer_set_oc_mode(TIM1, TIM_OC1, TIM_OCM_PWM1);
	 
//     timer_enable_oc_preload(TIM1, oc_id);
 	 timer_enable_oc_preload(TIM1, TIM_OC1);

//     timer_set_oc_value(TIM1, oc_id, 0);
	 timer_set_oc_value(TIM1, TIM_OC1, 0);
//    timer_enable_oc_output(TIM1, oc_id);
}

void pwm_set_pulse_width(uint32_t timer_peripheral, enum tim_oc_id oc_id, uint32_t pulse_width)
{
     (void)timer_peripheral;
     timer_set_oc_value(TIM1, oc_id, pulse_width);
}

void pwm_start_timer(uint32_t timer_peripheral)
{
	timer_generate_event(timer_peripheral, TIM_EGR_UG);
    timer_enable_counter(timer_peripheral);
}
