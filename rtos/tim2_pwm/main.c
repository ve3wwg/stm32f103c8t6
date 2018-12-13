/* Simple PWM on Timer 2 demo:
 * Warren Gay	Sat Dec 30 11:59:39 2017
 *
 * When LED PC13 is dark, the PA1 output will be
 * on for 20% of the cycle, or about 1.3 ms.
 * When LED PC13 is lit, PA1 output is high for
 * 40% of the cycle, or about 2.6 ms.
 * The full cycle is 6.67 ms (150 Hz).
 *
 * PC13		LED
 * PA1		TIM2.CH out
 *
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

static void
task1(void *args __attribute__((unused))) {
	static const int ms[4] = { 500, 1200, 2500, 1200 };
	int msx = 0;

	gpio_set(GPIOC,GPIO13);				// LED on

	rcc_periph_clock_enable(RCC_TIM2);		// Need TIM2 clock
	rcc_periph_clock_enable(RCC_AFIO);		// Need AFIO clock

	// PA1 == TIM2.CH2	
	rcc_periph_clock_enable(RCC_GPIOA);		// Need GPIOA clock
	gpio_primary_remap(
		AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF,	// Optional
		AFIO_MAPR_TIM2_REMAP_NO_REMAP);		// This is default: TIM2.CH2=GPIOA1
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,	// High speed
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO1);	// GPIOA1=TIM2.CH2

	// TIM2:
	timer_disable_counter(TIM2);
	rcc_periph_reset_pulse(RST_TIM2);

	timer_set_mode(TIM2,
		TIM_CR1_CKD_CK_INT,
		TIM_CR1_CMS_EDGE,
		TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM2,72-1); //see https://github.com/ve3wwg/stm32f103c8t6/pull/12/
	// Only needed for advanced timers:
	// timer_set_repetition_counter(TIM2,0);
	timer_enable_preload(TIM2);
	timer_continuous_mode(TIM2);
	timer_set_period(TIM2,33333-1);

	timer_disable_oc_output(TIM2,TIM_OC2);
	timer_set_oc_mode(TIM2,TIM_OC2,TIM_OCM_PWM1);
	timer_enable_oc_output(TIM2,TIM_OC2);

	timer_set_oc_value(TIM2,TIM_OC2,ms[msx=0]);
	timer_enable_counter(TIM2);

	for (;;) {
		vTaskDelay(1000);
		gpio_toggle(GPIOC,GPIO13);
		msx = (msx+1) % 4;
		timer_set_oc_value(TIM2,TIM_OC2,ms[msx]);
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);
	gpio_clear(GPIOC,GPIO13);			// LED off

	xTaskCreate(task1,"task1",100,NULL,1,NULL);
	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End main.c
