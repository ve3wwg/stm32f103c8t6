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

#include "usartlib.h"

#define IRQ_PRIORITY 	4

#define USART_BUF_DEPTH	32

struct s_usart {
	uint32_t	usart;
	uint16_t	head;
	uint16_t	tail;
	uint8_t		buf[USART_BUF_DEPTH];
};

static struct s_usart *usart_data[3] = { 0, 0, 0 };
static int count = 0;

/*********************************************************************
 * Receive data for USART
 *********************************************************************/

static void
usart_common_isr(struct s_usart *usartp) {
	uint32_t usart;

	if ( !usartp )
		return;
	usart = usartp->usart;

	while ( USART_SR(usart) & USART_SR_RXNE ) {
		usartp->buf[usartp->tail] = USART_DR(usart);
		usartp->tail = (usartp->tail + 1) % USART_BUF_DEPTH;
	}
}

void
usart1_isr(void) {
	++count;
	usart_common_isr(usart_data[0]);
}

void
usart2_isr(void) {
	usart_common_isr(usart_data[1]);
}

void
usart3_isr(void) {
	usart_common_isr(usart_data[2]);
}

/*********************************************************************
 * Examples:
 * open_usart(USART1,38400,"8N1","w",0,0);	TX, No RTS/CTS
 * open_usart(USART2,19200,"7E1","rw",0,0);	RX+TX, No RTS/CTS
 * open_usart(USART3,115200,"8N1","rw",1,1);	RX+TX, RTS/CTS
 *********************************************************************/

int
open_usart(uint32_t usart,uint32_t baud,const char *cfg,const char *mode,int rts,int cts) {
	uint32_t ux=0, rcc=RCC_USART1, stopb, iomode, parity, fc, irq = 0;
	bool rxintf = false;

	usart_disable_rx_interrupt(usart);

	/*************************************************************
	 * Which USART?
	 *************************************************************/

	switch ( usart ) {
	case USART1:
		ux = 0;
		rcc = RCC_USART1;
		irq = NVIC_USART1_IRQ;
		break;
	case USART2:
		ux = 1;
		rcc = RCC_USART2;
		irq = NVIC_USART2_IRQ;
		break;
	case USART3:
		ux = 2;
		rcc = RCC_USART3;
		irq = NVIC_USART3_IRQ;
		break;
	}

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
		if ( usart_data[ux] == 0 )
			usart_data[ux] = malloc(sizeof(struct s_usart));
		usart_data[ux]->usart = usart;
		usart_data[ux]->head = 	usart_data[ux]->tail = 0;
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

	rcc_periph_clock_enable(rcc);
	usart_set_baudrate(usart,baud);
	usart_set_databits(usart,cfg[0]&0x0F);
	usart_set_stopbits(usart,stopb);
	usart_set_mode(usart,iomode);
	usart_set_parity(usart,parity);
	usart_set_flow_control(usart,fc);

	nvic_enable_irq(irq);
	usart_enable(usart);

//	nvic_set_priority(irq,IRQ_PRIORITY);
//	nvic_set_priority(irq,0);
//	nvic_clear_pending_irq(irq);
	usart_enable_rx_interrupt(usart);

	return 0;		/* Success */
}

/*********************************************************************
 * Put one character to device, non-blocking
 *********************************************************************/
int
putc_usart_nb(uint32_t usart,char ch) {

	if ( (USART_SR(usart) & USART_SR_TXE) == 0 )
		return -1;	/* Busy */
	usart_send_blocking(usart,ch);
	return 0;		/* Success */
}

/*********************************************************************
 * Put one character to device, block (yield) until TX ready (blocking)
 *********************************************************************/
void
putc_usart(uint32_t usart,char ch) {

	while ( (USART_SR(usart) & USART_SR_TXE) == 0 )
		taskYIELD();	
	usart_send_blocking(usart,ch);
}

/*********************************************************************
 * Write size bytes to TX, yielding until all is sent (blocking)
 *********************************************************************/

void
write_usart(uint32_t usart,const char *buf,uint32_t size) {

	for ( ; size > 0; --size ) {
		while ( (USART_SR(usart) & USART_SR_TXE) == 0 )
			taskYIELD();	
		usart_send_blocking(usart,*buf++);
	}
}

/*********************************************************************
 * Send Null terminated string, yeilding (blocking)
 *********************************************************************/

void
puts_usart(uint32_t usart,const char *buf) {

	while ( *buf ) {
		while ( (USART_SR(usart) & USART_SR_TXE) == 0 )
			taskYIELD();	
		usart_send_blocking(usart,*buf++);
	}
}

/*********************************************************************
 * Internal: Locate s_usart * for usart ref
 *********************************************************************/

static struct s_usart *
find_usart(uint32_t usart,int *ux) {
	int x;

	for ( x=0; x<3; ++x ) {
		*ux = x;
		if ( usart_data[x]->usart == usart )
			return usart_data[x];
	}
	return 0;
}

/*********************************************************************
 * Internal: Return data given s_usart *
 *********************************************************************/

static int
get_char(struct s_usart *uptr) {
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
getc_usart_nb(uint32_t usart) {
	int ux;
	struct s_usart *uptr = find_usart(usart,&ux);

	if ( !uptr )
		return -1;	// No known usart
	return get_char(uptr);
}

/*********************************************************************
 * Receive a byte, blocking
 *********************************************************************/

char
getc_usart(uint32_t usart) {
	int ux;
	struct s_usart *uptr = find_usart(usart,&ux);
	int rch;

	if ( !uptr )
		return -1;	// No known usart

	while ( (rch = get_char(uptr)) == -1 )
		taskYIELD();
	return (char)rch;
}

/*********************************************************************
 * Close USART (frees RAM)
 *********************************************************************/

void
close_usart(uint32_t usart) {
	int ux;
	struct s_usart *uptr = find_usart(usart,&ux);

	usart_enable_rx_interrupt(usart);

	if ( uptr && usart_data[ux] ) {
		free(usart_data[ux]);
		usart_data[ux] = 0;
	}
}

/* End usartlib.c */
