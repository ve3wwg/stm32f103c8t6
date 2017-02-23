/* usartlib.h -- Interrupt driven USART 
 * Date: Tue Feb 21 20:44:52 2017   (C) Warren Gay ve3wwg
 */
#ifndef UARTLIB_H
#define UARTLIB_H

int open_usart(uint32_t usart,uint32_t baud,const char *cfg,const char *mode,int rts,int cts);
void close_usart(uint32_t usart);

int put_usart_nb(uint32_t usart,char ch);	/* non-blocking */
void put_usart(uint32_t usart,char ch);		/* blocking */
void write_usart(uint32_t usart,const char *buf,uint32_t size); /* blocking */
void puts_usart(uint32_t usart,const char *buf); /* blocking */
int getc_usart_nb(uint32_t usart);		/* non-blocking */
char getc_usart(uint32_t usart);		/* blocking */

#endif // UARTLIB_H

/* End usartlib.h */
