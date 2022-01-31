/*
 * console_stm32.c
 *
 * Part of librfn (a general utility library from redfelineninja.org.uk)
 *
 * This file was derived from libopencm3's usart_irq_printf.c example.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>,
 * Copyright (C) 2011 Piotr Esden-Tempski <piotr@esden.net>
 * Copyright (C) 2014 Daniel Thompson <daniel@redfelineninja.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <errno.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <librfn/console.h>

/* prototype functions not found in the headers (for -Wmissing-prototypes) */
int _write(int file, char *ptr, int len);

static uint8_t outbuf[1024];
static ringbuf_t outring = RINGBUF_VAR_INIT(outbuf, sizeof(outbuf));

static console_t *console;

void console_hwinit(console_t *c)
{
	console = c;

	/* Enable clocks for GPIO port A (for GPIO_USART1_TX) and USART1. */
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_USART3);

	/* Enable the USART1 interrupt. */
	nvic_enable_irq(NVIC_USART3_IRQ);

#ifdef STM32F4
	/* The F4 series have a completely new GPIO peripheral */
	/* PA9 and PA10 for Tx and Rx respectively */
	gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO9);

	/* Actual Alternate function number (in this case 7) */
	gpio_set_af(GPIOD, GPIO_AF7, GPIO8 | GPIO9);
#else
	rcc_periph_clock_enable(RCC_AFIO);

	/* Setup GPIO pin GPIO_USART1_RE_TX on PA9 */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART3_TX);

	/* Setup GPIO pin GPIO_USART1_RE_RX on PA10 */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_FLOAT, GPIO_USART3_RX);
#endif

	/* Setup UART parameters. */
	usart_set_baudrate(USART3, 115200);
	usart_set_databits(USART3, 8);
	usart_set_stopbits(USART3, USART_STOPBITS_1);
	usart_set_parity(USART3, USART_PARITY_NONE);
	usart_set_flow_control(USART3, USART_FLOWCONTROL_NONE);
	usart_set_mode(USART3, USART_MODE_TX_RX);

	/* Enable USART1 Receive interrupt. */
	USART_CR1(USART3) |= USART_CR1_RXNEIE;

	/* Finally enable the USART. */
	usart_enable(USART3);
}

void usart3_isr(void)
{
	/* Check if we were called because of RXNE. */
	if (((USART_CR1(USART3) & USART_CR1_RXNEIE) != 0) &&
	    ((USART_SR(USART3) & USART_SR_RXNE) != 0)) {
		uint16_t c = usart_recv(USART3);
		if (c == '\r') {
			(void) ringbuf_put(&outring, '\r');
			c = '\n';
		}
		(void) ringbuf_put(&outring, c);

		/* Enable transmit interrupt so it repeats the character back */
		USART_CR1(USART3) |= USART_CR1_TXEIE;

#ifdef CONFIG_CONSOLE_FROM_ISR
		console_process(console, c);
#else
		console_putchar(console, c);
#endif
	}

	/* Check if we were called because of TXE. */
	if (((USART_CR1(USART3) & USART_CR1_TXEIE) != 0) &&
	    ((USART_SR(USART3) & USART_SR_TXE) != 0)) {
		int data = ringbuf_get(&outring);

		if (data == -1) {
			/* Disable the TXE interrupt, it's no longer needed. */
			USART_CR1(USART3) &= ~USART_CR1_TXEIE;
		} else {
			/* Put data into the transmit register. */
			usart_send(USART3, data);
		}
	}
}

int _write(int file, char *ptr, int len)
{
	if (file == 1 || file == 2) {
		for (int i=0; i<len; i++) {
			if (ptr[i] == '\n')
				(void) ringbuf_put(&outring, '\r');
			(void) ringbuf_put(&outring, ptr[i]);
			USART_CR1(USART3) |= USART_CR1_TXEIE;
		}

		return 0;
	}

	errno = EIO;
	return -1;
}
