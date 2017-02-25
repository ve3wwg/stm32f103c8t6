/* usartlib.h -- Interrupt driven USART 
 * Date: Tue Feb 21 20:44:52 2017   (C) Warren Gay ve3wwg
 *
 * NOTES:
 *	(1) It is assumed that the caller has configured the participating GPIO
 *          lines for the USART, as well as the GPIO RCC.
 *
 *	    For example, for USART1:
 *		GPIOA, A9 is Output (TX)
 *		GPIOA, A10 is Input (RX)
 *		GPIOA, A11 is Output (RTS, when used)
 *		GPIOA, A12 is Input (CTS, when used)
 *
 *	(2) open_usart() will start the peripheral RCC.
 *	(3) open_usart() enables rx interrupts, when required.
 *
 */
#ifndef UARTLIB_H
#define UARTLIB_H

int open_usart(uint32_t usart,uint32_t baud,const char *cfg,const char *mode,int rts,int cts);
void close_usart(uint32_t usart);

int putc_usart_nb(uint32_t usart,char ch);			/* non-blocking */
void putc_usart(uint32_t usart,char ch);			/* blocking */
void write_usart(uint32_t usart,const char *buf,uint32_t size); /* blocking */
void puts_usart(uint32_t usart,const char *buf);		/* blocking */
int getc_usart_nb(uint32_t usart);				/* non-blocking */
char getc_usart(uint32_t usart);				/* blocking */

#endif // UARTLIB_H

/* End usartlib.h */
