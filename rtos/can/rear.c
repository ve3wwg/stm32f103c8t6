/* rear.c : CAN rear contoller
 * Mon Jul  3 08:49:40 2017	Warren W. Gay VE3WWG
 * Uses CAN on PA11/PA12:
 *
 * GPIO		Description
 * ----		-----------
 * PB14		Left turn signal
 * PB13		Right turn signal
 * PB12		Parking lights
 * PA11		CAN_RX (NOTE: Differs from main.c)
 * PA12		CAN_TX (NOTE: Differs from main.c)
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/adc.h>

#include "FreeRTOS.h"
#include "mcuio.h"
#include "miniprintf.h"
#include "canmsgs.h"

#define GPIO_PORT_LED		GPIOC		// Builtin LED port
#define GPIO_LED		GPIO13		// Builtin LED

#define LAMP_PORT		GPIOB		// Lamp port
#define LAMP_LEFT		GPIO14		// Left turn signal
#define LAMP_RIGHT		GPIO13		// Right turn signal
#define LAMP_PARK		GPIO12		// Parking lights

static volatile struct s_lamp_status lamp_status = { 0, 0, 0, 0, 0, 0 };

/*********************************************************************
 * Turn the given lamp(s) on/off
 *********************************************************************/
static void
lamp_on(bool on,uint16_t gpios) {

	if ( on )
		gpio_clear(LAMP_PORT,gpios);
	else	gpio_set(LAMP_PORT,gpios);
}

/*********************************************************************
 * Lamp Enables
 *********************************************************************/
static void
lamp_enable(enum MsgID id,bool enable) {

	switch ( id ) {
	case ID_ParkEn:
		lamp_status.park = enable;
		lamp_on(enable,LAMP_PARK);
		return;
	case ID_LeftEn:
		lamp_status.left = enable;
		if ( !enable )
			lamp_status.flash = false;
		break;
	case ID_RightEn:
		lamp_status.right = enable;
		if ( !enable )
			lamp_status.flash = false;
		break;
	case ID_BrakeEn:
		lamp_status.brake = enable;
		break;
	case ID_Flash:
		lamp_status.flash ^= true;
		break;
	default:
		break;
	}

	if ( lamp_status.brake ) {
		lamp_on(!lamp_status.left || (lamp_status.left & !lamp_status.flash),LAMP_LEFT);
		lamp_on(!lamp_status.right || (lamp_status.right & !lamp_status.flash),LAMP_RIGHT);
	} else	{
		lamp_on(lamp_status.left & !lamp_status.flash,LAMP_LEFT);
		lamp_on(lamp_status.right & !lamp_status.flash,LAMP_RIGHT);
	}
}

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
	static const int v25 = 143;	// 1.43 V
	int vtemp;

	vtemp = (int)read_adc(ADC_CHANNEL_TEMP) * 3300 / 4095;

	return (v25 - vtemp) / 45 + 2500; // temp = (1.43 - Vtemp) / 4.5 + 25.00
}

/*********************************************************************
 * CAN Receive Callback
 *********************************************************************/
void
can_recv(struct s_canmsg *msg) {
	union u_msg {
		struct s_lamp_en	lamp;
	} *msgp = (union u_msg *)msg->data;
	struct s_temp100 temp_msg;

	gpio_toggle(GPIO_PORT_LED,GPIO_LED);

	if ( !msg->rtrf ) {
		// Received commands:
		switch ( msg->msgid ) {
		case ID_LeftEn:
		case ID_RightEn:
		case ID_ParkEn:
		case ID_BrakeEn:
		case ID_Flash:
			lamp_enable((enum MsgID)msg->msgid,msgp->lamp.enable);
			break;
		default:
			break;
		}
	} else	{
		// Requests:
		switch ( msg->msgid ) {
		case ID_Temp:
			temp_msg.celciusx100 = degrees_C100();
			can_xmit(ID_Temp,false,false,sizeof temp_msg,&temp_msg);
			break;
		default:
			break;
		}
	}
}

/*********************************************************************
 * Monitor task:
 *********************************************************************/
static void
controller_task(void *arg __attribute((unused))) {

        for (;;) {
		vTaskDelay(pdMS_TO_TICKS(500));
		can_xmit(ID_HeartBeat2,false,false,sizeof lamp_status,(void *)&lamp_status);
		gpio_toggle(GPIO_PORT_LED,GPIO_LED);
        }
}

/*********************************************************************
 * Main program: Device initialization etc.
 *********************************************************************/
int
main(void) {

        rcc_clock_setup_in_hse_8mhz_out_72mhz();        // Use this for "blue pill"

        rcc_periph_clock_enable(RCC_GPIOA);
        rcc_periph_clock_enable(RCC_GPIOB);
        rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set_mode(LAMP_PORT,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,LAMP_LEFT);
	gpio_set_mode(LAMP_PORT,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,LAMP_RIGHT);
	gpio_set_mode(LAMP_PORT,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,LAMP_PARK);
	gpio_set_mode(GPIO_PORT_LED,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO_LED);

	lamp_enable(ID_LeftEn,false);
	lamp_enable(ID_RightEn,false);
	lamp_enable(ID_ParkEn,false);

	gpio_clear(GPIO_PORT_LED,GPIO_LED);

	// Initialize CAN
	initialize_can(false,true,false);		// !nart, locked, altcfg=false PA11/PA12

	xTaskCreate(controller_task,"rear",300,NULL,configMAX_PRIORITIES-1,NULL);

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
	adc_enable_temperature_sensor();
	adc_power_on(ADC1);
	adc_reset_calibration(ADC1);
	adc_calibrate_async(ADC1);
	while ( adc_is_calibrating(ADC1) );

	vTaskStartScheduler();
	for (;;);
}

// End main.c
