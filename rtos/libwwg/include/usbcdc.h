/* libusbcdc header
 * Warren W. Gay VE3WWG
 */
#ifndef LIBUSBCDC_H
#define LIBUSBCDC_H

#include <stdlib.h>
#include <string.h>

extern void usb_start(bool gpio_init);
extern int usb_ready(void);
extern int usb_set_cooked(int cooked);

extern void usb_putc(char ch);
extern void usb_puts(const char *buf);
extern int usb_printf(const char *format,...);

extern int usb_getc(void);
extern int usb_peek(void);
extern int usb_gets(char *buf,unsigned maxbuf);

#endif /* LIBUSBCDC_H */

/* End libusbcdc.h */
