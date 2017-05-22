/* main.c : CAN
 * Warren W. Gay VE3WWG
 * Tue May  9 20:58:59 2017
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "FreeRTOS.h"
#include "mcuio.h"
#include "miniprintf.h"
#include "canmsgs.h"
#include "monitor.h"

#define GPIO_PORT_LED		GPIOC		// Builtin LED port
#define GPIO_LED		GPIO13		// Builtin LED

/*********************************************************************
 * Set LED On/Off
 *********************************************************************/
static void
led(bool on) {
	if ( on )
		gpio_clear(GPIO_PORT_LED,GPIO_LED);
	else	gpio_set(GPIO_PORT_LED,GPIO_LED);
}

/*********************************************************************
 * CAN Receive Callback
 *********************************************************************/
void
can_recv(struct s_canmsg *msg) {

	std_printf("[%4u(%d/%u):%c,$%02X]\n",
		(unsigned)msg->msgid,
		msg->fifo,(unsigned)msg->fmi,
		msg->rtrf ? 'R' : 'D',
		msg->data[0]);
}

/*********************************************************************
 * Enable/Disable a signal lamp or parking lamps
 *********************************************************************/

static void
lamp_enable(enum MsgID id,bool enable) {
	struct s_lamp_en msg;

	msg.enable = enable;
	msg.reserved = 0;
	can_xmit(id,false,false,sizeof msg,&msg);
}

/*********************************************************************
 * Console:
 *********************************************************************/
static void
console_task(void *arg __attribute((unused))) {
	char ch;

        for (;;) {
		std_printf("> ");
		ch = std_getc();
		std_printf("%c\n",ch);

		switch ( ch ) {
		case 'L':
		case 'l':
			lamp_enable(ID_LeftEn,ch == 'L');
			break;
		case 'R':
		case 'r':
			lamp_enable(ID_RightEn,ch == 'R');
			break;
		case 'P':
		case 'p':
			lamp_enable(ID_ParkEn,ch == 'P');
			break;
		default:
			std_printf("??\n");
		}
        }
}

/*
 * Main program: Device initialization etc.
 */
int
main(void) {

        rcc_clock_setup_in_hse_8mhz_out_72mhz();        // Use this for "blue pill"

        rcc_periph_clock_enable(RCC_GPIOC);
        gpio_set_mode(GPIO_PORT_LED,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO_LED);

	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO_USART1_TX);
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO11);

	std_set_device(mcu_uart1);			// Use UART1 for std I/O
        open_uart(1,115200,"8N1","rw",1,1);

	initialize_can(false,true);			// !nart, locked

	led(false);
	xTaskCreate(console_task,"console",300,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;);
}

// End main.c
