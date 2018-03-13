/* MCU I/O: UART or USB on STM32
 * Warren W. Gay VE3WWG
 * Sun Apr 30 16:42:59 2017
 */
#ifndef MCUIO_H
#define MCUIO_H

#include <stdint.h>
#include <stdbool.h>

#include <uartlib.h>
#include <usbcdc.h>
#include <getline.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * IMPORTANT NOTE:
 *
 * These I/O routines all assume that the necessary devices have been
 * initialized. USB has initialization requirements, while the
 * UART devices need baud rate etc. to be defined.
 *********************************************************************/

/*********************************************************************
 * MCU I/O Control Block Definitions
 *********************************************************************/

typedef void (*putch_t)(char ch);
typedef void (*puts_t)(const char *buf);
typedef int (*printf_t)(const char *format,...) __attribute((format(printf,1,2)));
typedef int (*vprintf_t)(const char *format,va_list ap);
typedef int (*getch_t)(void);
typedef int (*peek_t)(void);
typedef int (*gets_t)(char *buf,unsigned maxbuf);
typedef void (*write_t)(const char *buf,unsigned bytes);

struct s_mcuio {
	putch_t		putc;		// put character
	puts_t		puts;		// put string
	vprintf_t	vprintf;
	getch_t		getc;		// get character
	peek_t		peek;		// peek at a character
	gets_t		gets;		// get a line (string)
	write_t		write;		// Write uncooked data
	gets_t		getline;	// edited get line
};

/*********************************************************************
 * Supported MCU devices
 *********************************************************************/

extern const struct s_mcuio
	*mcu_uart1,		// UART1
	*mcu_uart2,		// UART2
	*mcu_uart3,		// UART3
	*mcu_usb;		// USBCDC
extern const struct s_mcuio
	*mcu_stdio;		// Chosen standard device

/*********************************************************************
 * Perform I/O to the chosen device:
 *********************************************************************/

static inline void mcu_putc(const struct s_mcuio *dev,char ch) { dev->putc(ch); }
static inline void mcu_puts(const struct s_mcuio *dev,const char *buf) { dev->puts(buf); }
static inline int mcu_vprintf(const struct s_mcuio *dev,const char *format,va_list ap) { return dev->vprintf(format,ap); }
int mcu_printf(const struct s_mcuio *dev,const char *format,...) __attribute((format(printf,2,3)));
static inline int mcu_getc(const struct s_mcuio *dev) { return dev->getc(); }
static inline int mcu_peek(const struct s_mcuio *dev) { return dev->peek(); }
static inline int mcu_gets(const struct s_mcuio *dev,char *buf,unsigned maxbuf) { return dev->gets(buf,maxbuf); }
static inline void mcu_write(const struct s_mcuio *dev,const char *buf,unsigned bytes) { dev->write(buf,bytes); }
static inline int mcu_getline(const struct s_mcuio *dev,char *buf,unsigned maxbuf) { return getline(buf,maxbuf,dev->getc,dev->putc); }

/*********************************************************************
 * These I/O to the currently set std_set_device() device:
 *********************************************************************/

static inline void std_set_device(const struct s_mcuio *device) { mcu_stdio = device; }

static inline void std_putc(char ch) { mcu_putc(mcu_stdio,ch); }
static inline void std_puts(const char *buf) { mcu_puts(mcu_stdio,buf); }
static inline int std_vprintf(const char *format,va_list ap) { return mcu_stdio->vprintf(format,ap); }
int std_printf(const char *format,...) __attribute((format(printf,1,2)));
static inline int std_getc(void) { return mcu_getc(mcu_stdio); }
static inline int std_peek(void) { return mcu_peek(mcu_stdio); }
static inline int std_gets(char *buf,unsigned maxbuf) { return mcu_gets(mcu_stdio,buf,maxbuf); }
static inline void std_write(const char *buf,unsigned bytes) { mcu_write(mcu_stdio,buf,bytes); }
static inline int std_getline(char *buf,unsigned maxbuf) { return mcu_getline(mcu_stdio,buf,maxbuf); }

#ifdef __cplusplus
}
#endif

#endif // MCUIO_H
// End mcuio.h
