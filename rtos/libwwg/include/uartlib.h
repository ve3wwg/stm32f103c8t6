/* uartlib.h -- Interrupt driven USART 
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
 *	(2) These routines all use a "uart number", with 1 == USART1, 2==USART2
 *	    etc. This approach provided some opportunity for code optimization.
 *	(3) open_uart() will start the peripheral RCC.
 *	(4) open_uart() enables rx interrupts, when required.
 *
 */
#ifndef UARTLIB_H
#define UARTLIB_H

#include <stdarg.h>

int open_uart(uint32_t uartno,uint32_t baud,const char *cfg,const char *mode,int rts,int cts);
void close_uart(uint32_t uartno);

int putc_uart_nb(uint32_t uartno,char ch);			/* non-blocking */
void putc_uart(uint32_t uartno,char ch);			/* blocking */
void write_uart(uint32_t uartno,const char *buf,uint32_t size); /* blocking */
void puts_uart(uint32_t uartno,const char *buf);		/* blocking */
int getc_uart_nb(uint32_t uartno);				/* non-blocking */
char getc_uart(uint32_t uartno);				/* blocking */
int getline_uart(uint32_t uartno,char *buf,uint32_t bufsiz);	/* blocking */

void uart1_putc(char ch);
void uart1_puts(const char *buf);
int uart1_vprintf(const char *format,va_list ap);
int uart1_printf(const char *format,...) __attribute((format(printf,1,2)));
int uart1_getc(void);
int uart1_peek(void);
int uart1_gets(char *buf,unsigned bufsiz);
void uart1_write(const char *buf,unsigned bytes);
int uart1_getline(char *buf,unsigned bufsiz);

void uart2_putc(char ch);
void uart2_puts(const char *buf);
int uart2_vprintf(const char *format,va_list ap);
int uart2_printf(const char *format,...) __attribute((format(printf,1,2)));
int uart2_getc(void);
int uart2_peek(void);
int uart2_gets(char *buf,unsigned bufsiz);
void uart2_write(const char *buf,unsigned bytes);
int uart2_getline(char *buf,unsigned bufsiz);

void uart3_putc(char ch);
void uart3_puts(const char *buf);
int uart3_vprintf(const char *format,va_list ap);
int uart3_printf(const char *format,...) __attribute((format(printf,1,2)));
int uart3_getc(void);
int uart3_peek(void);
int uart3_gets(char *buf,unsigned bufsiz);
void uart3_write(const char *buf,unsigned bytes);
int uart3_getline(char *buf,unsigned bufsiz);

#endif // UARTLIB_H

/* End uartlib.h */
