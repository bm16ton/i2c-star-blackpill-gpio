/*
* The MIT License (MIT)
*
* Copyright (c) 2015 Marco Russi
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


/* ---------------- Inclusions ----------------- */
#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/f4/nvic.h>

#include "pwm.h"




/* ----------- Exported functions ------------- */

/* Timer clock frequency [Hz] */
#define TIM_CLOCK_FREQ_HZ					1000000	/* 1MHz */

/* Default PWM frequency */
#define TIM_DEFAULT_PWM_FREQ_HZ				100	/* 100Hz */




/* ----------- Local variables declaration ------------- */

/* Store current PWM frequency */
static uint32_t current_timer_cnt_period;




/* ----------- Exported functions ------------- */

/* init PWM */
void pwm_init(void)
{
	/* Init timer */

	/* Enable Timer4 clock. */
	rcc_periph_clock_enable(RCC_TIM3);

	/* Reset TIM3 peripheral */
	//timer_reset(TIM3);

	/* Set the timers global mode to:
	 * - use no divider
	 * - alignment edge
	 * - count direction up
	 * */
	timer_set_mode(TIM3,
					TIM_CR1_CKD_CK_INT,
					TIM_CR1_CMS_EDGE,
					TIM_CR1_DIR_UP);

	/* set prescaler */
	timer_set_prescaler(TIM3, ((rcc_apb1_frequency * 2) / TIM_CLOCK_FREQ_HZ));
	/* enable preload */
	timer_enable_preload(TIM3);
	/* set continuous mode */
	timer_continuous_mode(TIM3);
	/* set repetition counter */
	timer_set_repetition_counter(TIM3, 0);
	/* set period */
	current_timer_cnt_period = ((TIM_CLOCK_FREQ_HZ / TIM_DEFAULT_PWM_FREQ_HZ) - 1);
	timer_set_period(TIM3, current_timer_cnt_period);

	/* Init output channels */

	/* Enable GPIOC clock */
	rcc_periph_clock_enable(RCC_GPIOC);

	/* Set GPIO6, GPIO7, GPIO8, GPIO9 (in GPIO port D) to Alternate Function */
	gpio_mode_setup(GPIOC,
					GPIO_MODE_AF,
					GPIO_PUPD_NONE,
					GPIO6 | GPIO7 | GPIO8 | GPIO9);

	/* Push Pull, Speed 50 MHz */
	gpio_set_output_options(GPIOC,
							GPIO_OTYPE_PP,
							GPIO_OSPEED_50MHZ,
							GPIO6 | GPIO7 | GPIO8 | GPIO9);

	/* Alternate Function: TIM3 CH1/2/3/4 */
	gpio_set_af(GPIOC,
				GPIO_AF2,
				GPIO6 | GPIO7 | GPIO8 | GPIO9);

	/* disable TIM3 channels */
	timer_disable_oc_output(TIM3, TIM_OC1);
	timer_disable_oc_output(TIM3, TIM_OC2);
	timer_disable_oc_output(TIM3, TIM_OC3);
	timer_disable_oc_output(TIM3, TIM_OC4);

	/* set OC mode for each channel */
	timer_set_oc_mode(TIM3, TIM_OC1, TIM_OCM_PWM1);
	timer_set_oc_mode(TIM3, TIM_OC2, TIM_OCM_PWM1);
	timer_set_oc_mode(TIM3, TIM_OC3, TIM_OCM_PWM1);
	timer_set_oc_mode(TIM3, TIM_OC4, TIM_OCM_PWM1);

	timer_enable_oc_preload(TIM3, TIM_OC1);
	timer_enable_oc_preload(TIM3, TIM_OC2);
	timer_enable_oc_preload(TIM3, TIM_OC3);
	timer_enable_oc_preload(TIM3, TIM_OC4);

	/* reset OC value for each channel */
	timer_set_oc_value(TIM3, TIM_OC1, 0);
	timer_set_oc_value(TIM3, TIM_OC2, 0);
	timer_set_oc_value(TIM3, TIM_OC3, 0);
	timer_set_oc_value(TIM3, TIM_OC4, 0);

	/* enable OC output for each channel */
	timer_enable_oc_output(TIM3, TIM_OC1);
	timer_enable_oc_output(TIM3, TIM_OC2);
	timer_enable_oc_output(TIM3, TIM_OC3);
	timer_enable_oc_output(TIM3, TIM_OC4);
}


void pwm_set_frequency(uint32_t pwm_freq_hz)
{
	/* if requested frequency is lower than
	 * timer clock frequency */
	if (pwm_freq_hz <= PWM_MAX_FREQ_HZ) {
		/* set period and store it */
		current_timer_cnt_period = ((TIM_CLOCK_FREQ_HZ / pwm_freq_hz) - 1);
		timer_set_period(TIM3, current_timer_cnt_period);
	}
}


/* set DC value for a channel */
void pwm_set_dc(uint8_t ch_index, uint16_t dc_value_permillage)
{
	uint32_t dc_tmr_reg_value;

	if (dc_value_permillage <= 1000) {
		/* calculate DC timer register value */
		dc_tmr_reg_value = (uint32_t)(((uint64_t)current_timer_cnt_period * dc_value_permillage) / 1000);
		/* update the required channel */
		switch(ch_index)
		{
		case 0:
		{
			timer_set_oc_value(TIM3, TIM_OC1, dc_tmr_reg_value);
			break;
		}
		case 1:
		{
			timer_set_oc_value(TIM3, TIM_OC2, dc_tmr_reg_value);
			break;
		}
		case 2:
		{
			timer_set_oc_value(TIM3, TIM_OC3, dc_tmr_reg_value);
			break;
		}
		case 3:
		{
			timer_set_oc_value(TIM3, TIM_OC4, dc_tmr_reg_value);
			break;
		}
		default:
		{
			/* wrong channel index: do nothing */
		}
		}
	}
}


/* set OC value for a channel */
void pwm_start(void)
{
	timer_generate_event(TIM3, TIM_EGR_UG);

	timer_enable_counter(TIM3);
}




/* End of file */

