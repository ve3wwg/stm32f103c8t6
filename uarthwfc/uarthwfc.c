/* uarthwfc.cpp -- UART1 Demo using libopencm3, with hardware flow control
 * Date: Sat Feb 11 15:38:36 2017  (C) Warren W. Gay VE3WWG 
 */
#include "miniprintf.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

static void
init_clock(void) {

	rcc_clock_setup_in_hse_8mhz_out_24mhz();
	rcc_periph_clock_enable(RCC_GPIOC);

	// Clock for GPIO port A: GPIO_USART1_TX, USART1
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USART1);
}

static void
init_usart(void) {

	//////////////////////////////////////////////////////////////
	// STM32F103C8T6:
	//	RX:	A9
	//	TX:	A10
	//	CTS:	A11
	//	RTS:	A12
	//	Baud:	115200
	//////////////////////////////////////////////////////////////

	// GPIO_USART1_TX/GPIO13 on GPIO port A for tx
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,GPIO_USART1_TX);

	usart_set_baudrate(USART1,115200);
	usart_set_databits(USART1,8);
	usart_set_stopbits(USART1,USART_STOPBITS_1);
	usart_set_mode(USART1,USART_MODE_TX);
	usart_set_parity(USART1,USART_PARITY_NONE);
	usart_set_flow_control(USART1,USART_FLOWCONTROL_RTS_CTS);
	usart_enable(USART1);
}

static void
init_gpio(void) {
	// C.GPIO13:
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);
}

static inline void
uart_putc(char ch) {
	usart_send_blocking(USART1,ch);
}

static int
uart_printf(const char *format,...) {
	va_list args;
	int rc;

	va_start(args,format);
	rc = mini_vprintf_cooked(uart_putc,format,args);
	va_end(args);
	return rc;
}

static void
pause(void) {
	int x;

	for ( x = 0; x < 800000; x++ )	// Wait
		__asm__("NOP");
}

int
main(void) {
	int y = 0, x = 0;
	int count = -5;

	init_clock();
	init_gpio();
	init_usart();

	int c = uart_printf("\nuart.c demo using mini_printf():\n");
	uart_printf("Count = %d\n",c);

#if 0
	{
		char temp[256];
		int c2;

		c2 = mini_snprintf(temp,sizeof temp,"[c = %d]",c);
		uart_printf("Formatted '%s', with c2=%d;\n",temp,c2);
	}
#endif

	for (;;) {
		gpio_toggle(GPIOC,GPIO13);	// Toggle LED
		usart_send_blocking(USART1,x+'0');
		x = (x+1) % 10;
		if ( ++y % 80 == 0 ) {
			c = uart_printf("\nLine # %d or %+05d (0x%x or 0x%08x): '%20s', '%-20s', '%s'\n",count,count,count,count,"oh lait!","ok?","etc.");
			uart_printf("Count = %d.\n",c);
			++count;
			pause();
		}
	}

	return 0;
}

/* End uart.cpp */
