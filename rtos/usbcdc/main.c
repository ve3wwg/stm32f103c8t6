/* USB CDC Demo, using the usbcdc library
 * Warren W. Gay VE3WWG
 * Wed Mar 15 21:56:50 2017
 */
#include <stdlib.h>
#include <string.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>

#include "usbcdc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*
 * I/O Task: Here we:
 *
 *	1) Read a character from USB
 *	2) Switch case, if character is alpha
 *	3) Echo character back to USB (and toggle LED)
 */
static void
rxtx_task(void *arg) {
	char ch;
	
	(void)arg;

	for (;;) {
		ch = usb_getch();
		gpio_toggle(GPIOC,GPIO13);
		if ( ( ch >= 'A' && ch <= 'Z' ) || ( ch >= 'a' && ch <= 'z' ) )
			ch ^= 0x20;
		usb_putch(ch);
	}
}

/*
 * Main program: Device initialization etc.
 */
int
main(void) {

	SCB_CCR &= ~SCB_CCR_UNALIGN_TRP;		// Make sure alignment is not enabled

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"

	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	xTaskCreate(rxtx_task,"RXTX",200,NULL,configMAX_PRIORITIES-1,NULL);

	usb_start(1);
	gpio_clear(GPIOC,GPIO13);

	vTaskStartScheduler();
	for (;;);
}

/* End main.c */
