/* Task based USART demo:
 * Warren Gay VE3WWG
 *
 * This simple demonstration runs from task1, writing 012...XYZ lines
 * one after the other, at a rate of 5 characters/second. This demo
 * uses usart_send_blocking() to write characters.
 *
 * STM32F103C8T6:
 *	TX:	A9  <====> RX of TTL serial
 *	RX:	A10 <====> TX of TTL serial
 *	CTS:	A11 (not used)
 *	RTS:	A12 (not used)
 *	Config:	8N1
 *	Baud:	38400
 * Caution:
 *	Not all GPIO pins are 5V tolerant, so be careful to
 *	get the wiring correct.
 */
#include <FreeRTOS.h>
#include <task.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

/*********************************************************************
 * Setup the UART
 *********************************************************************/
static void
uart_setup(void) {

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USART1);

	// UART TX on PA9 (GPIO_USART1_TX)
	gpio_set_mode(GPIOA,
		GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
		GPIO_USART1_TX);

	usart_set_baudrate(USART1,38400);
	usart_set_databits(USART1,8);
	usart_set_stopbits(USART1,USART_STOPBITS_1);
	usart_set_mode(USART1,USART_MODE_TX);
	usart_set_parity(USART1,USART_PARITY_NONE);
	usart_set_flow_control(USART1,USART_FLOWCONTROL_NONE);
	usart_enable(USART1);
}

/*********************************************************************
 * Send one character to the UART
 *********************************************************************/
static inline void
uart_putc(char ch) {
	usart_send_blocking(USART1,ch);
}

/*********************************************************************
 * Send characters to the UART, slowly
 *********************************************************************/
static void
task1(void *args __attribute__((unused))) {
	int c = '0' - 1;

	for (;;) {
		gpio_toggle(GPIOC,GPIO13);
		vTaskDelay(pdMS_TO_TICKS(200));
		if ( ++c >= 'Z' ) {
			uart_putc(c);
			uart_putc('\r');
			uart_putc('\n');
			c = '0' - 1;
		} else	{
			uart_putc(c);
		}
	}
}

/*********************************************************************
 * Main program
 *********************************************************************/
int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz(); // Blue pill

	// PC13:
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(
		GPIOC,
                GPIO_MODE_OUTPUT_2_MHZ,
                GPIO_CNF_OUTPUT_PUSHPULL,
                GPIO13);

	uart_setup();

	xTaskCreate(task1,"task1",100,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();

	for (;;);
	return 0;
}

// End
