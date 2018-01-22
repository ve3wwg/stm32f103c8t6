/* front.c : CAN front contoller
 * Mon May 22 08:45:51 2017	Warren W. Gay VE3WWG
 * Uses CAN on PA11/PA12
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
 * Turn the given lamp(s) on/off:
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
		if ( !enable ) {
			lamp_status.flash = false;
			lamp_on(false,LAMP_LEFT);
		} else	{
			lamp_on(!lamp_status.flash,LAMP_LEFT);
		}
		break;
	case ID_RightEn:
		lamp_status.right = enable;
		if ( !enable ) {
			lamp_status.flash = false;
			lamp_on(false,LAMP_RIGHT);
		} else	{
			lamp_on(!lamp_status.flash,LAMP_RIGHT);
		}
		break;
	case ID_Flash:
		lamp_status.flash ^= true;
		lamp_on(lamp_status.left & !lamp_status.flash,LAMP_LEFT);
		lamp_on(lamp_status.right & !lamp_status.flash,LAMP_RIGHT);
		break;
	case ID_BrakeEn:
		break;
	default:
		break;
	}
}

/*********************************************************************
 * CAN Receive Callback
 *********************************************************************/
void
can_recv(struct s_canmsg *msg) {
	union u_msg {
		struct s_lamp_en	lamp;
	} *msgp = (union u_msg *)msg->data;

	gpio_toggle(GPIO_PORT_LED,GPIO_LED);

	if ( !msg->rtrf ) {
		switch ( msg->msgid ) {
		case ID_LeftEn:
		case ID_RightEn:
		case ID_ParkEn:
		case ID_Flash:
			lamp_enable((enum MsgID)msg->msgid,msgp->lamp.enable);
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
		can_xmit(ID_HeartBeat,false,false,sizeof lamp_status,(void *)&lamp_status);
		gpio_toggle(GPIO_PORT_LED,GPIO_LED);
        }
}

/*********************************************************************
 * Main program: Device initialization etc.
 *********************************************************************/
int
main(void) {

        rcc_clock_setup_in_hse_8mhz_out_72mhz();        // Use this for "blue pill"

	// LED setup
        rcc_periph_clock_enable(RCC_GPIOA);
        rcc_periph_clock_enable(RCC_GPIOB);
        rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(LAMP_PORT,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,LAMP_LEFT);
	gpio_set_mode(LAMP_PORT,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,LAMP_RIGHT);
	gpio_set_mode(LAMP_PORT,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,LAMP_PARK);
	gpio_set_mode(GPIO_PORT_LED,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO_LED);

	gpio_set(LAMP_PORT,LAMP_LEFT|LAMP_RIGHT|LAMP_PARK);
	gpio_clear(GPIO_PORT_LED,GPIO_LED);

	// Initialize CAN
	initialize_can(false,true,false);		// !nart, locked, altcfg=false PA11/PA12

	xTaskCreate(controller_task,"front",300,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;);
}

// End main.c
