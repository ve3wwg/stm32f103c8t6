/* SIMPLE LED task demo:
 * Fri Mar 24 22:45:55 2017	Warren Gay Ve3WWG
 *
 * 1) The LED on PC13 is on/off by USB control messages.
 * 2) Bulk endpoint messages are received (EP 0x01)
 * 3) Case inverted message is echoed back on EP 0x82.
 */
#include <string.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/usbstd.h>
#include <libopencm3/stm32/st_usbfs.h>

#define mainECHO_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

static usbd_device *udev = NULL;	// USB Device

extern void led(int on);

void
led(int on) {
	if ( on )
		gpio_clear(GPIOC,GPIO13);
	else	gpio_set(GPIOC,GPIO13);
}

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_VENDOR,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x16c0,		// V-USB + libusb
	.idProduct = 1,			// Arbitrary
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor data_endp[] = {
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x01,		/* From Host */
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 0,				/* Ignored for bulk */
	}, {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x82,		/* To Host */
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 0,				/* Ignored for bulk */
	}
};

#if 0
static const struct usb_interface_descriptor iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = 0xFF,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
};
#endif

static const struct usb_interface_descriptor data_iface[] = {
	{
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = 0xFF,
		.bInterfaceSubClass = 0,
		.bInterfaceProtocol = 0,
		.iInterface = 0,

		.endpoint = data_endp,
	}
};

static const struct usb_interface ifaces[] = {
	{
		.num_altsetting = 1,
		.altsetting = data_iface,
	}
};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,
	.interface = ifaces,
};

static const char * usb_strings[] = {
	"Warren's custom usb device.",
	"VE3WWG module",
	"ve3wwg",
};

/*
 * Control Requests:
 */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes
custom_control_request(
  usbd_device *usbd_dev,
  struct usb_setup_data *req,
  uint8_t **buf,
  uint16_t *len,
  void (**complete)(usbd_device *usbd_dev,struct usb_setup_data *req)
) {
	(void)complete;
	(void)buf;
	(void)usbd_dev;
	(void)len;

	switch ( req->bRequest ) {
	case USB_REQ_GET_STATUS:
		return USBD_REQ_HANDLED;

	case USB_REQ_SET_FEATURE:
		led(req->wValue&1);	// Set/reset LED
		return USBD_REQ_HANDLED;
	default:
		;
	}

	return USBD_REQ_NOTSUPP;
}

/*
 * Bulk receive callback:
 */
static void
bulk_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
	static char in_rx = 0;			// Nest count to avoid recursion
	char buf[64], *bp;			// rx buffer & ptr
	int len, x, ch;				// Received len..

	if ( in_rx > 0 )
		return;				// Don't recurse
	++in_rx;				// In rx cb..

	// Read what we can, leave rest:
	len = usbd_ep_read_packet(usbd_dev,ep,buf,sizeof buf);

	// Invert case of message received:
	for ( x=0; x<len; ++x ) {
		ch = buf[x];
		if ( isalpha(ch) )
			buf[x] ^= 0x20;		// Invert case
	}

	// Echo back the message, until fully sent:
	bp = buf;
	do	{
		x = usbd_ep_write_packet(usbd_dev,0x82,bp,len);
		if ( x > 0 ) {
			len -= x;
			bp += x;
		}
		if ( x < len )
			usbd_poll(usbd_dev);
	} while ( len > 0 );

	--in_rx;				// Out of rx cb
}

/*
 * Configure:
 */
static void
set_config(usbd_device *usbd_dev, uint16_t wValue) {
	(void)wValue;

	usbd_ep_setup(usbd_dev,0x01,USB_ENDPOINT_ATTR_BULK,64,bulk_rx_cb);
	usbd_ep_setup(usbd_dev,0x82,USB_ENDPOINT_ATTR_BULK,64,NULL);
	usbd_register_control_callback(
		usbd_dev,
		USB_REQ_TYPE_VENDOR,
		USB_REQ_TYPE_TYPE,
		custom_control_request);
}

/*
 * Poll any USB device events:
 */
static void
usb_task(void *arg) {
	unsigned istr;
	(void)arg;

	for (;;) {
		istr = *USB_ISTR_REG;		// Capture USB interrupt flags
		usbd_poll(udev);		// Handle interrupt flags
		if ( *USB_ISTR_REG == istr )	// No change?
			taskYIELD();		// No events, so safe to yeild
	}
}

/*
 * Main program:
 */
int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	// For LED on C13
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	// For USB:
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_CRC);
	rcc_periph_clock_enable(RCC_USB);
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO11);
	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO12);

	led(0); // Turn off LED

	udev = usbd_init(&st_usbfs_v1_usb_driver,&dev,&config,
		usb_strings,3,
		usbd_control_buffer,sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(udev,set_config);

	xTaskCreate(usb_task,"USB",100,udev,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();

	for (;;);	// Should never get here
	return 0;
}

// End main.c
