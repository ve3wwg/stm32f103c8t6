/* Packaged FreeRTOS usbcdc library
 * Warren W. Gay VE3WWG
 */
#include <stdlib.h>
#include <string.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/scb.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <usbcdc.h>
#include <miniprintf.h>
#include <getline.h>

static volatile char initialized = 0;			// True when USB configured
static QueueHandle_t usb_txq;				// USB transmit queue
static QueueHandle_t usb_rxq;				// USB receive queue

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_CDC,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec it's
 * optional, but its absence causes a NULL pointer dereference in the
 * Linux cdc_acm driver. (Gareth McMullin <gareth@blacksphere.co.nz>)
 */
static const struct usb_endpoint_descriptor comm_endp[] = {
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x83,
		.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
		.wMaxPacketSize = 16,
		.bInterval = 255,
	}
};

static const struct usb_endpoint_descriptor data_endp[] = {
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x01,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1,
	}, {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x82,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1,
	}
};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	 }
};

static const struct usb_interface_descriptor comm_iface[] = {
	{
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 1,
		.bInterfaceClass = USB_CLASS_CDC,
		.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
		.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
		.iInterface = 0,

		.endpoint = comm_endp,

		.extra = &cdcacm_functional_descriptors,
		.extralen = sizeof(cdcacm_functional_descriptors)
	}
};

static const struct usb_interface_descriptor data_iface[] = {
	{
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 1,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = USB_CLASS_DATA,
		.bInterfaceSubClass = 0,
		.bInterfaceProtocol = 0,
		.iInterface = 0,

		.endpoint = data_endp,
	}
};

static const struct usb_interface ifaces[] = {
	{
		.num_altsetting = 1,
		.altsetting = comm_iface,
	}, {
		.num_altsetting = 1,
		.altsetting = data_iface,
	}
};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char * usb_strings[] = {
	"libusbcdc.c driver",
	"CDC-ACM module",
	"WGDEMO",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes
cdcacm_control_request(
  usbd_device *usbd_dev,
  struct usb_setup_data *req,
  uint8_t **buf,
  uint16_t *len,
  void (**complete)(usbd_device *usbd_dev,struct usb_setup_data *req)
) {
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		return USBD_REQ_HANDLED;
		}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding)) {
			return USBD_REQ_NOTSUPP;
		}

		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

static void
cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
	unsigned rx_avail = uxQueueSpacesAvailable(usb_rxq);	/* How much queue capacity left? */
	char buf[64];						/* rx buffer */
	int len, x;

	(void)ep;

	if ( rx_avail <= 0 )
		return;						/* No space to rx */

	len = sizeof buf < rx_avail ? sizeof buf : rx_avail;	/* Bytes to read */
	len = usbd_ep_read_packet(usbd_dev,0x01,buf,len);	/* Read what we can, leave the rest */

	for ( x=0; x<len; ++x )
		xQueueSend(usb_rxq,&buf[x],0);			/* Send data to the rx queue */
}

static void
cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue) {
	(void)wValue;

	usbd_ep_setup(usbd_dev,0x01,USB_ENDPOINT_ATTR_BULK,64,cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev,0x82,USB_ENDPOINT_ATTR_BULK,64,NULL);
	usbd_ep_setup(usbd_dev,0x83,USB_ENDPOINT_ATTR_INTERRUPT,16,NULL);

	usbd_register_control_callback(
		usbd_dev,
		USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		cdcacm_control_request);

	initialized = 1;
}

/*
 * USB Driver task:
 */
static void
usb_task(void *arg) {
	usbd_device *udev = (usbd_device *)arg;
	char txbuf[64];
	unsigned txlen = 0;

	for (;;) {
		usbd_poll(udev);			/* Allow driver to do it's thing */
		if ( initialized ) {
			while ( txlen < sizeof txbuf && xQueueReceive(usb_txq,&txbuf[txlen],0) == pdPASS )
				++txlen;		/* Read data to be sent */
			if ( txlen > 0 ) {
				if ( usbd_ep_write_packet(udev,0x82,txbuf,txlen) != 0 )
					txlen = 0;	/* Reset if data sent ok */
			} else	{
				taskYIELD();		/* Then give up CPU */
			}
		} else	{
			taskYIELD();
		}
	}
}

/*
 * Put character to USB (blocks):
 */
void
usb_putc(char ch) {
	static const char cr = '\r';

	while ( !usb_ready() )
		taskYIELD();

	if ( ch == '\n' )
		xQueueSend(usb_txq,&cr,portMAX_DELAY);
	xQueueSend(usb_txq,&ch,portMAX_DELAY);
}

/*
 * Put string to USB:
 */
void
usb_puts(const char *buf) {

	while ( *buf )
		usb_putc(*buf++);
}

/*
 * USB vprintf() interface:
 */
int
usb_vprintf(const char *format,va_list ap) {
	return mini_vprintf_cooked(usb_putc,format,ap);
}

/*
 * Printf to USB:
 */
int
usb_printf(const char *format,...) {
	int rc;
	va_list args;

	va_start(args,format);
	rc = mini_vprintf_cooked(usb_putc,format,args);
	va_end(args);
	return rc;
}

/*
 * Write (always) uncooked data:
 */
void
usb_write(const char *buf,unsigned bytes) {

	while ( bytes-- > 0 ) {
		xQueueSend(usb_txq,buf,portMAX_DELAY);
		++buf;
	}
}

/*
 * Get one character from USB (blocking):
 */
int
usb_getc(void) {
	char ch;

	if ( xQueueReceive(usb_rxq,&ch,portMAX_DELAY) != pdPASS )
		return -1;
	return ch;
}

/*
 * Peek to see if there is more input from USB:
 *
 * RETURNS:
 *	1	At least one character is waiting to be read
 *	0	No data to read.
 *	-1	USB error (disconnected?)
 */
int
usb_peek(void) {
	char ch;
	uint32_t rc;

	rc = xQueuePeek(usb_rxq,&ch,0);

	switch ( rc ) {
	case errQUEUE_EMPTY:
		return 0;
	case pdPASS:
		return 1;
	default:
		return -1;
	}
}

/*
 * Get a line of text ending with CF / LF,
 * (blocking). Input is echoed.
 *
 * ARGUMENTS:
 *	buf		Input buffer
 *	maxbuf		Maximum # of bytes
 *
 * RETURNS:
 *	# of bytes read
 *
 * NOTES:
 *	Reading stops with first CR/LF, or
 *	maximum length, whichever occurs
 *	first.
 */
int
usb_gets(char *buf,unsigned maxbuf) {
	unsigned bx = 0;
	int ch;
	
	while ( maxbuf > 0 && bx+1 < maxbuf ) {
		ch = usb_getc();
		if ( ch == -1 ) {
			if ( !bx )
				return -1;
			break;
		}
		if ( ch == '\r' || ch == '\n' ) {
			buf[bx++] = '\n';
			usb_putc('\n');
			break;
		}
		buf[bx++] = (char)ch;
		usb_putc(ch);
	}
	
	buf[bx] = 0;
	return bx;
}
 					
/*
 * Get an edited line:
 */
int
usb_getline(char *buf,unsigned maxbuf) {

	return getline(buf,maxbuf,usb_getc,usb_putc);
}

/*
 * Start USB driver:
 *
 * ARGUMENTS:
 *	gpio_init	When true, setup RCC and GPIOA for USB
 */
void
usb_start(bool gpio_init,unsigned priority) {
	usbd_device *udev = 0;

	usb_txq = xQueueCreate(128,sizeof(char));
	usb_rxq = xQueueCreate(128,sizeof(char));

	if ( gpio_init ) {
		rcc_periph_clock_enable(RCC_GPIOA);
		rcc_periph_clock_enable(RCC_USB);
	}

	udev = usbd_init(&st_usbfs_v1_usb_driver,&dev,&config,
		usb_strings,3,
		usbd_control_buffer,sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(udev,cdcacm_set_config);

	xTaskCreate(usb_task,"USB",300,udev,priority,NULL);
}

/*
 * Return True if the USB connection + driver initialized and ready.
 */
int
usb_ready(void) {
	return initialized;
}

/*
 * Yield until USB ready:
 */
void
usb_yield(void) {
	while ( !initialized )
		taskYIELD();
}

/* End libusbcdc.c */
