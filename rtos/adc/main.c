/* ADC1 - Read Temperature, Vref and a Single ADC input
 * Warren W. Gay	Thu Jul 13 22:25:31 2017
 *
 *	GPIO	Function
 *	A0	Analog In
 *	A1	Analog In
 *
 *	Console on USB
 *
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/cm3/nvic.h>

#include "mcuio.h"
#include "miniprintf.h"

#define GPIO_PORT_LED		GPIOC		// Builtin LED port
#define GPIO_LED		GPIO13		// Builtin LED

/*********************************************************************
 * Read ADC Channel
 *********************************************************************/
static uint16_t
read_adc(uint8_t channel) {

	adc_set_sample_time(ADC1,channel,ADC_SMPR_SMP_239DOT5CYC);
	adc_set_regular_sequence(ADC1,1,&channel);
	adc_start_conversion_direct(ADC1);
	while ( !adc_eoc(ADC1) )
		taskYIELD();
	return adc_read_regular(ADC1);
}

/*********************************************************************
 * Return temperature in C * 100
 *********************************************************************/
static int
degrees_C100(void) {
	static const int v25 = 1365; // see https://github.com/ve3wwg/stm32f103c8t6/issues/11
	int vtemp;

	vtemp = (int)read_adc(ADC_CHANNEL_TEMP) * 3300 / 4095;

	return (v25 - vtemp) * 1000 / 45 + 2500; // temp = (1.43 - Vtemp) / 4.5 + 25.00
}

/*********************************************************************
 * Demo Task:
 *********************************************************************/
static void
demo_task(void *arg __attribute((unused))) {
	int temp100, vref;
	int adc0, adc1;

        for (;;) {
		vTaskDelay(pdMS_TO_TICKS(1500));
		gpio_toggle(GPIO_PORT_LED,GPIO_LED);

		temp100 = degrees_C100();
		vref = read_adc(ADC_CHANNEL_VREF) * 330 / 4095;
		adc0 = read_adc(0) * 330 / 4095;
		adc1 = read_adc(1) * 330 / 4095;

		std_printf("Temperature %d.%02d C, Vref %d.%02d Volts, ch0 %d.%02d V, ch1 %d.%02d V\n",
			temp100/100,temp100%100,
			vref/100,vref%100,
			adc0/100,adc0%100,
			adc1/100,adc1%100);
        }
}

/*********************************************************************
 * Main program and setup
 *********************************************************************/
int
main(void) {

        rcc_clock_setup_in_hse_8mhz_out_72mhz();        // Use this for "blue pill"

        rcc_periph_clock_enable(RCC_GPIOA);		// Enable GPIOA for ADC
	gpio_set_mode(GPIOA,
		GPIO_MODE_INPUT,
		GPIO_CNF_INPUT_ANALOG,
		GPIO0|GPIO1);				// PA0 & PA1

        rcc_periph_clock_enable(RCC_GPIOC);		// Enable GPIOC for LED
	gpio_set_mode(GPIOC,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO13);				// PC13
	gpio_clear(GPIO_PORT_LED,GPIO_LED);		// LED off

	xTaskCreate(demo_task,"demo",300,NULL,1,NULL);

	// Initialize ADC:
	rcc_peripheral_enable_clock(&RCC_APB2ENR,RCC_APB2ENR_ADC1EN);
	adc_power_off(ADC1);
	rcc_peripheral_reset(&RCC_APB2RSTR,RCC_APB2RSTR_ADC1RST);
	rcc_peripheral_clear_reset(&RCC_APB2RSTR,RCC_APB2RSTR_ADC1RST);
	rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV6);	// Set. 12MHz, Max. 14MHz
	adc_set_dual_mode(ADC_CR1_DUALMOD_IND);		// Independent mode
	adc_disable_scan_mode(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_single_conversion_mode(ADC1);
	adc_set_sample_time(ADC1,ADC_CHANNEL_TEMP,ADC_SMPR_SMP_239DOT5CYC);
	adc_set_sample_time(ADC1,ADC_CHANNEL_VREF,ADC_SMPR_SMP_239DOT5CYC);
	adc_enable_temperature_sensor();
	adc_power_on(ADC1);
	adc_reset_calibration(ADC1);
	adc_calibrate_async(ADC1);
	while ( adc_is_calibrating(ADC1) );

	std_set_device(mcu_usb);
	usb_start(1,1);
	vTaskStartScheduler();
	for (;;);

	return 0;
}

// End main.c
