/* Input PWM to Timer 4, PB3:
 * Warren Gay	Mon Jan  1 10:49:51 2018
 *
 * PC13		LED
 * PB6		TIM4.CH1 in (5-volt tolerant)
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "mcuio.h"

static volatile uint32_t cc1if = 0, cc2if = 0,
	c1count = 0, c2count = 0;

void
tim4_isr(void) {
	uint32_t sr = TIM_SR(TIM4);

	if ( sr & TIM_SR_CC1IF ) {
		cc1if = TIM_CCR1(TIM4);
		++c1count;
		timer_clear_flag(TIM4,TIM_SR_CC1IF);
	}
	if ( sr & TIM_SR_CC2IF ) {
		cc2if = TIM_CCR2(TIM4);
		++c2count;
		timer_clear_flag(TIM4,TIM_SR_CC2IF);
	}
}

static void
task1(void *args __attribute__((unused))) {

	gpio_set(GPIOC,GPIO13);				// LED on

	rcc_periph_clock_enable(RCC_TIM4);		// Need TIM4 clock

	// PB6 == TIM4.CH1
	rcc_periph_clock_enable(RCC_GPIOB);		// Need GPIOB clock
	gpio_set_mode(GPIOB,GPIO_MODE_INPUT,		// Input
		GPIO_CNF_INPUT_FLOAT,GPIO6);		// PB6=TIM4.CH1

	// TIM4:
	timer_disable_counter(TIM4);
	rcc_periph_reset_pulse(RST_TIM4);
	nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ,2);
	nvic_enable_irq(NVIC_TIM4_IRQ);
	timer_set_mode(TIM4,
		TIM_CR1_CKD_CK_INT,
		TIM_CR1_CMS_EDGE,
		TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM4,72-1);
	timer_ic_set_input(TIM4,TIM_IC1,TIM_IC_IN_TI1);
	timer_ic_set_input(TIM4,TIM_IC2,TIM_IC_IN_TI1);
	timer_ic_set_filter(TIM4,TIM_IC_IN_TI1,TIM_IC_CK_INT_N_2);
	timer_ic_set_prescaler(TIM4,TIM_IC1,TIM_IC_PSC_OFF);
	timer_slave_set_mode(TIM4,TIM_SMCR_SMS_RM);
	timer_slave_set_trigger(TIM4,TIM_SMCR_TS_TI1FP1);
	TIM_CCER(TIM4) &= ~(TIM_CCER_CC2P|TIM_CCER_CC2E
		|TIM_CCER_CC1P|TIM_CCER_CC1E);
	TIM_CCER(TIM4) |= TIM_CCER_CC2P|TIM_CCER_CC2E|TIM_CCER_CC1E;
	timer_ic_enable(TIM4,TIM_IC1);
	timer_ic_enable(TIM4,TIM_IC2);
	timer_enable_irq(TIM4,TIM_DIER_CC1IE|TIM_DIER_CC2IE);
	timer_enable_counter(TIM4);

	for (;;) {
		vTaskDelay(1000);
		gpio_toggle(GPIOC,GPIO13);

		std_printf("cc1if=%u (%u), cc2if=%u (%u)\n",
			(unsigned)cc1if,(unsigned)c1count,
			(unsigned)cc2if,(unsigned)c2count);
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);
	gpio_clear(GPIOC,GPIO13);			// LED off

	usb_start(1,1);
	std_set_device(mcu_usb);			// Use USB for std I/O

	xTaskCreate(task1,"task1",100,NULL,1,NULL);
	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End main.c
