/* Timer Demo: Simple TMR2 Pulse
 * Warren W. Gay VE3WWG
 * Thu Apr 13 19:36:10 2017
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

//#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
//#include <libopencm3/cm3/scb.h>
//#include <libopencm3/stm32/exti.h>
//#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
//#include <libopencm3/stm32/f1/bkp.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*********************************************************************
 * Setup TMR2 for pulse generation
 *********************************************************************/

static void
timer2_once(uint32_t div) {

	rcc_periph_clock_enable(RCC_TIM2);
	timer_reset(TIM2);
	timer_set_mode(TIM2,TIM_CR1_CKD_CK_INT_MUL_4,TIM_CR1_CMS_EDGE,TIM_CR1_DIR_UP);
	timer_one_shot_mode(TIM2);
	timer_set_period(TIM2,div);		// 1/72MHz/7200 ~= 100 usec
}

/*********************************************************************
 * This delays approx 100us, for programming 27C512 etc.
 *********************************************************************/

static void
delay_100us(void) {

	timer_clear_flag(TIM2,TIM_SR_UIF);	// UIF=0
	timer_enable_counter(TIM2);		// Start timer
	while ( !timer_get_flag(TIM2,TIM_SR_UIF) )
		;				// Until UIF=1
}

/*********************************************************************
 * Task to initialize and repeatedly generate a pulse
 *********************************************************************/

static void
timer_task(void *arg __attribute__((unused))) {
	
	timer2_once(7000u);			// Setup timer 2 for 100us
	gpio_clear(GPIOC,GPIO13);

	for (;;) {
		// Generate 100us Pulse on C13
		gpio_set(GPIOC,GPIO13);
		delay_100us();			// 100 usec
		gpio_clear(GPIOC,GPIO13);

		// Pause for another 100us
		delay_100us();			// 100 usec
	}
}

/*
 * Main program: Device initialization etc.
 */
int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	xTaskCreate(timer_task,"timer",300,NULL,configMAX_PRIORITIES-1,NULL);

	vTaskStartScheduler();
	for (;;);
}

/* End main.c */
