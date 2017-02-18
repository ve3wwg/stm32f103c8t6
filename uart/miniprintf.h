/* Minimal printf() facility for MCUs
 * Warren W. Gay VE3WWG, Sun Feb 12 2017
 * 
 * This work is placed into the public domain. No warranty, or guarantee
 * is expressed or implied. When uou use this source code, you do so
 * with full responsibility.
 */
#ifndef MINIPRINTF_H
#define MINIPRINTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

int mini_vprintf_cooked(void (*putc)(char),const char *format,va_list args);
int mini_vprintf_uncooked(void (*putc)(char),const char *format,va_list args);

int mini_snprintf(char *buf,unsigned maxbuf,const char *format,...)
	__attribute((format(printf,3,4)));

#ifdef __cplusplus
}
#endif

#endif // MINIPRINTF_H

#if 0
//////////////////////////////////////////////////////////////////////
// From the README file:
//////////////////////////////////////////////////////////////////////

Tested and estimated to require about 640 bytes of code for STM32F103C8T6. Should
be usable on any MCU platform that supports:

	#include <stdarg.h>
	#include <string.h>

SUPPORT:

Each format item %+0wd, %0wx and for strings %-ws, the following
applies:

    +   Optional: Indicates sign should always print (d and x)
    -   Optional: Indicates field should be left justified (s)
    0   Optional: Pad with leading zeros (d and x)
    w   Optional: Decimal field width
	
    Formats %c, %d, %x and %s are supported (only). '%%' prints as '%'.

    Floating point is not supported, keeping this library minimal.

FORMAT EXAMPLES:

    %+05d   '+00009'    int is 9.
    %d      '9'
    %03d    '009'
    %04x    '001F'      int is 31
    %x      '1F'
    %-9s    'abc      ' string was 'abc'
    %9s     '      abc'
    %s      'abc'

STRING FORMATTING:

    int mini_snprintf(char *buf,unsigned maxbuf,const char *format,...);

    See standard snprintf(3). Note that the output is null terminated
    when the buffer size permits.

DEVICE FORMATTING HOWTO:

    int mini_vprintf_cooked(void (*putc)(char),const char *format,va_list args);
    int mini_vprintf_uncooked(void (*putc)(char),const char *format,va_list args);

    (0) Decide: cooked or uncooked output?

        COOKED means that a CR is sent after every LF is sent out,
        like UNIX terminal output.

        UNCOOKED means no CR processing is performed. Like snprintf,
        what you format is what you get.

    (1) Declare your own putc function, something like:

        static void uart_putc(char ch) {
            usart_send_blocking(USART1,ch); // libopencm3
        }

    (2) Declare your own printf function:

        int uart_printf(const char *format,...)
            __attribute((format(printf,1,2)));

        int uart_printf(const char *format,...) {
            va_list args;
            int rc;

            va_start(args,format);
            rc = mini_vprintf_cooked(uart_putc,format,args);
            va_end(args);
            return rc;
        }

	The attribute clause is optional, but when provided can only
        appear in the function prototype. It allows the compiler to
	check that you have appropriate arguments for each format item.

    (3) Use it:

        int flea_count = 45;

        uart_printf("My dog has %d fleas.\n",flea_count);

NOTES:
    1.  Stack usage is minimal (perhaps 256 bytes).
    2.  No malloc/realloc/free calls (no heap usage)
    3.  Re-entrant (no static storage used)
    4.  Compromizes favoured smaller code over speed.

#endif
/* End miniprintf.h */
