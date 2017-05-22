/* main.c : CAN
 * Warren W. Gay VE3WWG
 * Tue May  9 20:58:59 2017
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/f1/bkp.h>
#include <libopencm3/stm32/f1/nvic.h>
#include <libopencm3/stm32/can.h>

#include "FreeRTOS.h"
#include "mcuio.h"
#include "miniprintf.h"
#include "canmsgs.h"
#include "monitor.h"

/*********************************************************************
 * CAN Receive Callback
 *********************************************************************/
void
can_rx_callback(struct s_canmsg *msg) {
        uint32_t *counter = (uint32_t*)msg->data;

	std_printf("[%4u(%d/%u):%c,%u]\n",
		(unsigned)msg->msgid,
		msg->fifo,(unsigned)msg->fmi,
		msg->rtrf ? 'R' : 'D',
		(unsigned)*counter);
}

/*
 * Monitor task:
 */
static void
monitor_task(void *arg __attribute((unused))) {

        for (;;) {
                monitor();
		can_tx_queue(32,false,false,3,"Hi!");
        }
}

/*
 * Main program: Device initialization etc.
 */
int
main(void) {

        rcc_clock_setup_in_hse_8mhz_out_72mhz();        // Use this for "blue pill"

        rcc_periph_clock_enable(RCC_GPIOC);
        gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

        rcc_periph_clock_enable(RCC_GPIOA);
        gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO_USART1_TX);
        gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO11);

        gpio_set_mode(GPIOA,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO_USART1_RX);
        gpio_set_mode(GPIOA,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO12);

	std_set_device(mcu_uart1);			// Use UART1 for std I/O
        open_uart(1,115200,"8N1","rw",1,1);
	initialize_can(false,true);			// !nart, locked

	gpio_clear(GPIOC,GPIO13);

	xTaskCreate(monitor_task,"monitor",300,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;);
}

// End main.c
