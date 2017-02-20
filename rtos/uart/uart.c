/* Task based UART demo:
 *
 * This simple demonstration runs from task1, writing 012...XYZ lines
 * one after the other, at a rate of 5 characters/second. This demo
 * uses usart_send_blocking() to write characters.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

#define mainECHO_TASK_PRIORITY				( tskIDLE_PRIORITY + 1 )

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,signed portCHAR *pcTaskName);

void
vApplicationStackOverflowHook(xTaskHandle *pxTask,signed portCHAR *pcTaskName) {
	(void)pxTask;
	(void)pcTaskName;
	for(;;);
}

static void
gpio_setup(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

}

static void
uart_setup(void) {

	//////////////////////////////////////////////////////////////
	// STM32F103C8T6:
	//	RX:	A9  <====> TX of TTL serial
	//	TX:	A10 <====> RX of TTL serial
	//	CTS:	A11 (not used)
	//	RTS:	A12 (not used)
	//	Baud:	38400
	// Caution:
	//	Not all GPIO pins are 5V tolerant, so be careful to
	//	get the wiring correct.
	//////////////////////////////////////////////////////////////

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USART1);

	// GPIO_USART1_TX/GPIO13 on GPIO port A for tx
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO_USART1_TX);

	usart_set_baudrate(USART1,38400);
	usart_set_databits(USART1,8);
	usart_set_stopbits(USART1,USART_STOPBITS_1);
	usart_set_mode(USART1,USART_MODE_TX);
	usart_set_parity(USART1,USART_PARITY_NONE);
	usart_set_flow_control(USART1,USART_FLOWCONTROL_NONE);
	usart_enable(USART1);
}

static inline void
uart_putc(char ch) {
	usart_send_blocking(USART1,ch);
}

static void
task1(void *args) {
	int c = 'Z';

	(void)args;

	for (;;) {
		gpio_toggle(GPIOC,GPIO13);
		vTaskDelay(pdMS_TO_TICKS(200));
		if ( ++c >= 'Z' ) {
			uart_putc('\r');
			uart_putc('\n');
			c = '0';
		}		
		uart_putc(c);
	}
}

int
main(void) {

	gpio_setup();
	uart_setup();
	xTaskCreate(task1,"LED",100,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;)
		;
	return 0;
}

// End
