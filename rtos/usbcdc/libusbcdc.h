/* libusbcdc header
 * Warren W. Gay VE3WWG
 */
#ifndef LIBUSBCDC_H
#define LIBUSBCDC_H

extern void usb_putch(char ch);
extern void usb_puts(const char *buf);
extern void usb_start(bool gpio_init);

extern int usb_getch(void);
extern int usb_peek(void);
extern int usb_gets(char *buf,unsigned maxbuf);

#endif /* LIBUSBCDC_H */

/* End libusbcdc.h */
