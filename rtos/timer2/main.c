/* Timer 2 Output Compare Pulse Example 
 * Sat Apr 15 10:08:36 2017	Warren W. Gay VE3WWG
 *
 * This example produces a 100 usec pulse on GPIOA1 (TIM2.OC2)
 * using the timer to turn off the pulse. The software sets the
 * pulse high, and then starts the timer. The timer when it
 * reaches its count, will then set GPIOA1 low again.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#define mainECHO_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/*********************************************************************
 * The SYSCLK runs at 72 MHz and the AHB prescaler = 1. The input
 * to the APB1 prescaler is thus also 72 MHz. Because the APB1 bus
 * must not exceed 36 MHz, this APB1 prescaler divides by 2, giving
 * 36 MHz. However, since the APB1 prescaler is not equal to 1, the
 * clock going to TIM2, 3 and 4 is doubled and thus is 72 MHz (Figure
 * 8 of section 7.2 of RM0008). This sets CK_INT = 72 Mhz in Figure
 * 100 of section 15.2.
 *
 * Setting the prescaler TIM2_PSC = 360, produces a CK_CNT at every 
 * 5 usec (1/72000000/360). To generate a timer event at 100us, we
 * thus configure the counter and OC2 count to 20 (20 * 5 usec).
 *********************************************************************/

static void
timer_setup(void) {

	rcc_periph_clock_enable(RCC_GPIOA);		// Need GPIOA clock
	gpio_primary_remap(
		AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF,	// Optional
		AFIO_MAPR_TIM2_REMAP_NO_REMAP);		// This is default: TIM2.CH2=GPIOA1
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,	// High speed
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO1);	// GPIOA1=TIM2.CH2
	rcc_periph_clock_enable(RCC_TIM2);		// Need TIM2 clock
	rcc_periph_clock_enable(RCC_AFIO);		// Need AFIO clock

	rcc_periph_reset_pulse(RST_TIM2);				// Reset timer 2
	timer_set_prescaler(TIM2,360);			// Clock counts every 5 usec
	timer_set_mode(TIM2,TIM_CR1_CKD_CK_INT_MUL_4,
		TIM_CR1_CMS_EDGE,TIM_CR1_DIR_UP);	// Set timer mode
	timer_one_shot_mode(TIM2);			// One shot mode
	timer_set_period(TIM2,20);			// 100us

	timer_disable_oc_preload(TIM2,TIM_OC2);		// No oc preload
	timer_set_oc_polarity_high(TIM2,TIM_OC2);	// Normal polarity OC2
	timer_set_oc_value(TIM2,TIM_OC2,20);		// OC1 compare value: 20 * 5 us
	timer_enable_oc_output(TIM2,TIM_OC2);		// ..set output mode
	timer_set_oc_mode(TIM2,TIM_OC2,TIM_OCM_FORCE_LOW); // Force OC2=0
}

/*********************************************************************
 * Set GPIOA1 high and start the counter. Then wait for the counter
 * event to complete before returning.
 *********************************************************************/

static void
pulse(void) {

	cm_disable_interrupts();
	timer_set_oc_mode(TIM2,TIM_OC2,TIM_OCM_FORCE_HIGH);	// Force OC2 active
	timer_set_oc_mode(TIM2,TIM_OC2,TIM_OCM_INACTIVE);	// Timer to reset to inactive
	timer_enable_counter(TIM2);				// Start timer
	cm_enable_interrupts();

	while ( !timer_get_flag(TIM2,TIM_SR_CC2IF) )
		taskYIELD();					// Wait for timer event
}

/*********************************************************************
 * FreeRTOS task to setup the timer and continually generate pulses
 * in a loop. A 1 tick RTOS delay is inserted between each pulse.
 *********************************************************************/

static void
timer_task(void *args __attribute((unused))) {

	timer_setup();

	for (;;) {
		pulse();		// Create pulse
		vTaskDelay(1);		// Delay 1 tick
	}
}

/*********************************************************************
 * Main program.
 *********************************************************************/

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	xTaskCreate(timer_task,"timer",100,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;)			// Should never get here
		;
	return 0;
}

// End main.c
