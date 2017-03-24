///////////////////////////////////////////////////////////////////////
//
// This test program:
//
//	1)  Enumerates devices and locates the Vendor and Product
//	2)  Then opens the device
//	3)  Turns off the remote device's LED (or on if active low)
//	4)  Sleeps 2 seconds
//	5)  Turns on the remote device's LED 
//	6)  Closes and exits.
//
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <usb.h>

// Device: 005 16c0:0001, manuf 1 (VE3WWG module)

#define VEND_ID		0x16C0
#define PROD_ID		0x0001

int
main(int argc,char **argv) {
	struct usb_bus *bus;
	struct usb_device *dev, *usb_dev = 0;
	int rc;

	usb_init();             	// initialize libusb
usb_set_debug(7);
	usb_find_busses();
	usb_find_devices();

	for ( bus=usb_get_busses(); bus && !usb_dev; bus=bus->next ) {
		for( dev=bus->devices; dev && !usb_dev; dev=dev->next ) {
			struct usb_device_descriptor& desc = dev->descriptor;
#if 0
			usb_dev_handle *dh = usb_open(dev);
			char buf[50];

			if ( dh ) {
				if ( usb_get_string_simple(dh,desc.iProduct,buf,sizeof buf) <= 0 )
					strcpy(buf,"?");
				usb_close(dh);
				dh = 0;
			} else	*buf = 0;

			printf("Device: %s %04x:%04x, manuf %u (%s)\n",
				dev->filename,
				desc.idVendor,
				desc.idProduct,
				desc.iManufacturer,
				buf);
#endif
			if ( desc.idVendor == VEND_ID && desc.idProduct == PROD_ID ) {
#if 0
				printf("  Device %s is the test USB device!\n",
					dev->filename);
#endif
				usb_dev = dev;
			}
		}
	}

	if ( !usb_dev ) {
		// Bail out if the device was not found:
		fprintf(stderr,"USB device was not found.\n");
		return 1;
	}

	//////////////////////////////////////////////////////////////
	// Open and communicate with the device
	//////////////////////////////////////////////////////////////

	usb_dev_handle *handle = usb_open(usb_dev);

	if ( !handle ) {
		fprintf(stderr,"Unable to open device.\n");
		return 4;
	}

	printf("USB Device opened..\n");

	rc = usb_set_configuration(handle,1);
	if ( rc )
		printf("%s: rc=%d, usb_set_configuration(1)\n",strerror(-rc),rc);

	rc = usb_claim_interface(handle,0);			// Claim it
	if ( rc ) printf("%s: rc=%d, usb_claim_interface(0)\n",strerror(-rc),rc);

	rc = usb_set_altinterface(handle,0);
	if ( rc ) printf("%s: rc=%d, usb_set_altinterface(0)\n",strerror(-rc),rc);

	//////////////////////////////////////////////////////////////
	// wValue and wIndex can be any application specific data,
	// in addition to the data buffer (not used here). Here we
	// set wValue = 0, to turn off the remove LED
	//////////////////////////////////////////////////////////////

	printf("Sending to Test device..\n");

	int txbytes = usb_control_msg(
		handle,
		USB_TYPE_VENDOR|USB_RECIP_DEVICE|USB_ENDPOINT_OUT, // bRequestType
		USB_REQ_SET_FEATURE,
		1,	          	// wValue (LED on)
		0, 	         	// wIndex
		0,			// pointer to buffer containing data (no data here)
		0,			// wLength (no data here)
		1000			// timeout ms
	);

	printf("Sent %d bytes: LED should be ON.\n",txbytes);

	sleep(2);

	//////////////////////////////////////////////////////////////
	// Test bulk endpoints, using simple synchronous calls:
	//////////////////////////////////////////////////////////////

	puts("Sending three bulk messages to be echoed back..");

	static const char *msg[] = { "Message One", "Message Two.", "Message Three" };
	const char *p;
	char rxbuf[65];
	int sent;

	for ( int x=0; x<3; ++x ) {
		for ( p = msg[x]; p && *p; ) {
			printf("usb_bulk_write()..\n");
			rc = usb_bulk_write(
				handle,
				0x01,
				p,
				strlen(p),
				10000);
			if ( rc >= 0 ) {
				p += rc;
			} else	{
				printf("%s: rc = %d, usb_bulk_write()\n",strerror(-rc),rc);
				exit(1);
			}
		}
		printf("#%d sent: '%s'\n",x,msg[x]);

		// Receive message back:

		for ( sent = strlen(msg[x]); sent > 0; ) {
			rc = usb_bulk_read(
				handle,
				0x82,			// Endpoint IN
				rxbuf,
				64,
				10000);
			if ( rc >= 0 ) {
				rxbuf[rc] = 0;
				printf("Recv '%s' (%d)\n",rxbuf,rc);
				sent -= rc;
			} else	{
				printf("%s: rc = %d, usb_bulk_read()\n",strerror(-rc),rc);
				exit(2);
			}			
		}
	}

	//////////////////////////////////////////////////////////////
	// Send wValue = 1 to turn on the remote LED
	//////////////////////////////////////////////////////////////

	txbytes = usb_control_msg(
		handle,
		USB_TYPE_VENDOR|USB_RECIP_DEVICE|USB_ENDPOINT_OUT, // bRequestType
		USB_REQ_SET_FEATURE,
		0,	          	// wValue (LED off)
		0, 	         	// wIndex
		0,			// pointer to buffer containing data (no data here)
		0,			// wLength (no data here)
		1000			// timeout ms
	);

	printf("Sent %d bytes: LED should be OFF again.\n",txbytes);

	//////////////////////////////////////////////////////////////
	// Release the Text device
	//////////////////////////////////////////////////////////////

	usb_release_interface(handle,0);
	usb_close(handle);

	return 0;
}

// End test2.cpp
