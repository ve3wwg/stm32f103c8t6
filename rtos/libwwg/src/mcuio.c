/* MCU I/O: UART or USB on STM32 
 * Warren W. Gay VE3WWG
 * Sun Apr 30 16:46:11 2017
 */
#include <stdarg.h>
#include <mcuio.h>

static const struct s_mcuio dev_uart1 =
	{ uart1_putc, uart1_puts, uart1_vprintf, uart1_getc, uart1_peek, uart1_gets, uart1_write, uart1_getline };

static const struct s_mcuio dev_uart2 =
	{ uart2_putc, uart2_puts, uart2_vprintf, uart2_getc, uart2_peek, uart2_gets, uart2_write, uart2_getline };

static const struct s_mcuio dev_uart3 =
	{ uart3_putc, uart3_puts, uart3_vprintf, uart3_getc, uart3_peek, uart3_gets, uart3_write, uart3_getline };

static const struct s_mcuio dev_usb =
	{ usb_putc, usb_puts, usb_vprintf, usb_getc, usb_peek, usb_gets, usb_write, usb_getline };

const struct s_mcuio
	*mcu_uart1 = &dev_uart1,
	*mcu_uart2 = &dev_uart2,
	*mcu_uart3 = &dev_uart3,
	*mcu_usb = &dev_usb;

const struct s_mcuio
	*mcu_stdio = &dev_usb;		// By default

int
mcu_printf(const struct s_mcuio *dev,const char *format,...) {
	va_list ap;
	int rc;

	va_start(ap,format);
	rc = dev->vprintf(format,ap);
	va_end(ap);
	return rc;
}

int
std_printf(const char *format,...) {
	va_list ap;
	int rc;

	va_start(ap,format);
	rc = mcu_stdio->vprintf(format,ap);
	va_end(ap);
	return rc;
}

// End mcuio.c
