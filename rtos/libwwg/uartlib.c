/* Interrupt driven USART Library
 * Warren W. Gay VE3WWG		Tue Feb 21 20:35:54 2017
 */
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>

#include "uartlib.h"

/*********************************************************************
 * Receive buffers
 *********************************************************************/

#define USART_BUF_DEPTH	32

struct s_uart {
	uint16_t	head;					/* Buffer head index (pop) */
	uint16_t	tail;					/* Buffer tail index (push) */
	uint8_t		buf[USART_BUF_DEPTH];			/* Circular receive buffer */
};

struct s_uart_info {
	uint32_t	usart;			/* USART address */
	uint32_t	rcc;			/* RCC address */
	uint32_t	irq;			/* IRQ number */
};

static struct s_uart_info uarts[3] = {
	{ USART1, RCC_USART1, NVIC_USART1_IRQ },
	{ USART2, RCC_USART2, NVIC_USART2_IRQ },
	{ USART3, RCC_USART3, NVIC_USART3_IRQ }
};

/* static const uint32_t uarts[3] = { USART1, USART2, USART3 }; */
static struct s_uart *uart_data[3] = { 0, 0, 0 };

/*********************************************************************
 * Receive data for USART
 *********************************************************************/

static void
uart_common_isr(unsigned ux) {
	struct s_uart *uartp = uart_data[ux];			/* Access USART's buffer */
	uint32_t uart = uarts[ux].usart;			/* Lookup USART address */
	uint32_t ntail;						/* Next tail index */
	char ch;						/* Read data byte */

	if ( !uartp )
		return;						/* Not open for ISR receiving! */

	while ( USART_SR(uart) & USART_SR_RXNE ) {		/* Read status */
		ch = USART_DR(uart);				/* Read data */
		ntail = (uartp->tail + 1) % USART_BUF_DEPTH;	/* Calc next tail index */

		/* Save data if the buffer is not full */
		if ( ntail != uartp->head ) {			/* Not full? */
			uartp->buf[uartp->tail] = ch;		/* No, stow into buffer */
			uartp->tail = ntail;			/* Advance tail index */
		}
	}
}

/*********************************************************************
 * USART1 ISR
 *********************************************************************/

void
usart1_isr(void) {
	uart_common_isr(0);
}

/*********************************************************************
 * USART2 ISR
 *********************************************************************/

void
usart2_isr(void) {
	uart_common_isr(1);
}

/*********************************************************************
 * USART3 ISR
 *********************************************************************/

void
usart3_isr(void) {
	uart_common_isr(2);
}

/*********************************************************************
 * Examples:
 * open_uart(1,38400,"8N1","w",0,0);	UART1, TX, No RTS/CTS
 * open_uart(2,19200,"7E1","rw",0,0);	UART2, RX+TX, No RTS/CTS
 * open_uart(3,115200,"8N1","rw",1,1);	UART3, RX+TX, RTS/CTS
 *********************************************************************/

int
open_uart(uint32_t uartno,uint32_t baud,const char *cfg,const char *mode,int rts,int cts) {
	uint32_t uart, ux, stopb, iomode, parity, fc;
	struct s_uart_info *infop;
	bool rxintf = false;

	if ( uartno < 1 || uartno > 3 )
		return -1;			/* Invalid UART ref */

	infop = &uarts[ux = uartno-1];		/* USART parameters */
	uart = infop->usart;			/* USART address */
	usart_disable_rx_interrupt(uart);

	/*************************************************************
	 * Parity
	 *************************************************************/

	switch ( cfg[1] ) {
	case 'O':
		parity = USART_PARITY_ODD;
		break;
	case 'E':
		parity = USART_PARITY_EVEN;
		break;
	case 'N':
		parity = USART_PARITY_NONE;
		break;
	/* No Mark parity? Use 2-stop bits for that? */
	default:
		return -1;		// Bad parity
	}

	/*************************************************************
	 * Stop bits
	 *************************************************************/

	stopb = USART_STOPBITS_1;

	switch ( cfg[2] ) {
	case '.':
	case '0':
		stopb = USART_STOPBITS_0_5;
		break;
	case '1':
		if ( cfg[3] == '.' )
			stopb = USART_STOPBITS_1_5;
		else	stopb = USART_STOPBITS_1;
		break;
	case '2':
		stopb = USART_STOPBITS_2;
	}

	/*************************************************************
	 * Transmit mode: "r", "w" or "rw"
	 *************************************************************/

	if ( mode[0] == 'r' && mode[1] == 'w' ) {
		iomode = USART_MODE_TX_RX;
		rxintf = true;
	} else if ( mode[0] == 'r' ) {
		iomode = USART_MODE_RX;
		rxintf = true;
	} else if ( mode[0] == 'w' )
		iomode =  USART_MODE_TX;
	else	return -1;		/* Mode fail */

	/*************************************************************
	 * Setup RX ISR
	 *************************************************************/

	if ( rxintf ) {
		if ( uart_data[ux] == 0 )
			uart_data[ux] = malloc(sizeof(struct s_uart));
		uart_data[ux]->head = 	uart_data[ux]->tail = 0;
	}	

	/*************************************************************
	 * Flow control mode:
	 *************************************************************/

	fc = USART_FLOWCONTROL_NONE;
	if ( rts ) {
		if ( cts )
			fc = USART_FLOWCONTROL_RTS_CTS;
		else	fc = USART_FLOWCONTROL_RTS;
	} else if ( cts ) {
		fc = USART_FLOWCONTROL_CTS;
	}

	/*************************************************************
	 * Establish settings:
	 *************************************************************/

	rcc_periph_clock_enable(infop->rcc);
	usart_set_baudrate(uart,baud);
	usart_set_databits(uart,cfg[0]&0x0F);
	usart_set_stopbits(uart,stopb);
	usart_set_mode(uart,iomode);
	usart_set_parity(uart,parity);
	usart_set_flow_control(uart,fc);

	nvic_enable_irq(infop->irq);
	usart_enable(uart);
	usart_enable_rx_interrupt(uart);

	return 0;		/* Success */
}

/*********************************************************************
 * Put one character to device, non-blocking
 *********************************************************************/
int
putc_uart_nb(uint32_t uartno,char ch) {
	uint32_t uart = uarts[uartno-1].usart;

	if ( (USART_SR(uart) & USART_SR_TXE) == 0 )
		return -1;	/* Busy */
	usart_send_blocking(uart,ch);
	return 0;		/* Success */
}

/*********************************************************************
 * Put one character to device, block (yield) until TX ready (blocking)
 *********************************************************************/
void
putc_uart(uint32_t uartno,char ch) {
	uint32_t uart = uarts[uartno-1].usart;

	while ( (USART_SR(uart) & USART_SR_TXE) == 0 )
		taskYIELD();	
	usart_send_blocking(uart,ch);
}

/*********************************************************************
 * Write size bytes to TX, yielding until all is sent (blocking)
 *********************************************************************/

void
write_uart(uint32_t uartno,const char *buf,uint32_t size) {
	uint32_t uart = uarts[uartno-1].usart;

	for ( ; size > 0; --size ) {
		while ( (USART_SR(uart) & USART_SR_TXE) == 0 )
			taskYIELD();	
		usart_send_blocking(uart,*buf++);
	}
}

/*********************************************************************
 * Send Null terminated string, yeilding (blocking)
 *********************************************************************/

void
puts_uart(uint32_t uartno,const char *buf) {
	uint32_t uart = uarts[uartno-1].usart;

	while ( *buf ) {
		while ( (USART_SR(uart) & USART_SR_TXE) == 0 )
			taskYIELD();	
		usart_send_blocking(uart,*buf++);
	}
}

/*********************************************************************
 * Internal: Return data given s_uart *
 *********************************************************************/

static int
get_char(struct s_uart *uptr) {
	char rch;

	if ( uptr->head == uptr->tail )
		return -1;	// No data available
	rch = uptr->buf[uptr->head];	
	uptr->head = ( uptr->head + 1 ) % USART_BUF_DEPTH;
	return rch;
}

/*********************************************************************
 * Receive a byte, non-blocking
 *********************************************************************/

int
getc_uart_nb(uint32_t uartno) {
	struct s_uart *uptr = uart_data[uartno-1];

	if ( !uptr )
		return -1;	// No known uart
	return get_char(uptr);
}

/*********************************************************************
 * Receive a byte, blocking
 *********************************************************************/

char
getc_uart(uint32_t uartno) {
	struct s_uart *uptr = uart_data[uartno-1];
	int rch;

	if ( !uptr )
		return -1;	// No known uart
	while ( (rch = get_char(uptr)) == -1 )
		taskYIELD();
	return (char)rch;
}

/*********************************************************************
 * Close USART (frees RAM)
 *********************************************************************/

void
close_uart(uint32_t uartno) {
	uint32_t ux = uartno - 1;
	struct s_uart *uptr = uart_data[ux];

	usart_disable_rx_interrupt(uarts[ux].usart);

	if ( uptr && uart_data[ux] ) {
		free(uart_data[ux]);
		uart_data[ux] = 0;
	}
}

/* End uartlib.c */
