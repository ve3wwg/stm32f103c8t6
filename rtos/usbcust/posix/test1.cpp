//////////////////////////////////////////////////////////////////////
// test1.cpp -- POSIX test program to exercise control and bulk ep
// Date: Fri Mar 24 22:05:37 2017  (C) Warren W. Gay VE3WWG 
///////////////////////////////////////////////////////////////////////
//
// This test program:
//
//	1)  Enumerates devices and locates the Vendor and Product
//	2)  Opens the USB device
//	3)  Claims interface 0
//	4)  Turns on the remote device's LED
//	5)  Sends three bulk messages and then
//	6)  Receives the echoed messages (with case inverted)
//	7)  Turns off the remote device's LED 
//	8)  Releases usb interface
//	9)  Closes usb interface
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

#define VEND_ID		0x16C0		// This is a V-USB vendor ID
#define PROD_ID		0x0001

static struct usb_device *usb_dev = 0;
static struct usb_dev_handle *handle = 0;

//////////////////////////////////////////////////////////////////////
// Release interface and close USB device upon exit
//////////////////////////////////////////////////////////////////////

static void
exit_cleanup() {

	if ( handle ) {
		usb_release_interface(handle,0);
		usb_close(handle);
	}
}

//////////////////////////////////////////////////////////////////////
// Main program: No command line arguments supported
//////////////////////////////////////////////////////////////////////

int
main(int argc,char **argv) {
	static const char *msg[] = { "Message One", "Message Two.", "Message Three" };
	const char *p;
	char rxbuf[65];
	int rc, sent;

	usb_init();             	// initialize libusb
	usb_set_debug(7);		// useful when things don't work

	//////////////////////////////////////////////////////////////
	// Locate our USB device:  Set usb_dev when found.
	//////////////////////////////////////////////////////////////

	usb_find_busses();
	usb_find_devices();

	{
		struct usb_bus *bus;
		struct usb_device *dev;

		for ( bus=usb_get_busses(); bus && !usb_dev; bus=bus->next ) {
			for( dev=bus->devices; dev && !usb_dev; dev=dev->next ) {
				struct usb_device_descriptor& desc = dev->descriptor;

				if ( desc.idVendor == VEND_ID && desc.idProduct == PROD_ID )
					usb_dev = dev;
			}
		}
	}

	if ( !usb_dev ) {
		fprintf(stderr,"USB device was not found (plugged in?).\n");
		exit(1);
	}

	//////////////////////////////////////////////////////////////
	// Open USB device
	//////////////////////////////////////////////////////////////

	handle = usb_open(usb_dev);
	if ( !handle ) {
		fprintf(stderr,"Unable to open device: usb_open(%p)\n",usb_dev);
		exit(2);
	}

	atexit(exit_cleanup);				// Close usb if we exit early

	//////////////////////////////////////////////////////////////
	// Choose configuration and claim interface:
	//////////////////////////////////////////////////////////////

	rc = usb_set_configuration(handle,1);
	if ( rc )
		fprintf(stderr,"%s: usb_set_configuration(1)\n",strerror(-rc));

	rc = usb_claim_interface(handle,0);		// Claim interface # 0
	if ( rc )
		fprintf(stderr,"%s: usb_claim_interface(0)\n",strerror(-rc));;

	//////////////////////////////////////////////////////////////
	// Send a control message via endpoint 0:
	//
	// wValue and wIndex can be any application specific data,
	// in addition to the data buffer (not used here). Here we
	// set wValue = 0, to turn off the remove LED
	//////////////////////////////////////////////////////////////

	rc = usb_control_msg(
		handle,
		USB_TYPE_VENDOR|USB_RECIP_DEVICE|USB_ENDPOINT_OUT,
		USB_REQ_SET_FEATURE,
		1,	          	// wValue (LED on)
		0, 	         	// wIndex
		0,			// pointer to buffer containing data (no data here)
		0,			// wLength (no data here)
		100			// timeout ms
	);
	if ( rc < 0 )
		fprintf(stderr,"%s: usb_control_msg()\n",strerror(-rc));
	else	printf("Sent control, %d bytes: LED is ON.\n",rc);

	//////////////////////////////////////////////////////////////
	// Bulk endpoint test:
	//////////////////////////////////////////////////////////////

	puts("\nSending three bulk messages to be echoed back..");

	for ( int x=0; x<3; ++x ) {

		// Send bulk message:

		for ( p = msg[x]; p && *p; ) {
			rc = usb_bulk_write(
				handle,
				0x01,
				p,
				strlen(p),
				10000);
			if ( rc >= 0 ) {
				p += rc;
			} else	{
				fprintf(stderr,"%s: usb_bulk_write()\n",
					strerror(-rc));
				exit(3);
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
				fprintf(stderr,"%s: usb_bulk_read()\n",strerror(-rc));
				exit(4);
			}			
		}
	}

	//////////////////////////////////////////////////////////////
	// Another control message to turn off LED:
	//////////////////////////////////////////////////////////////

	rc = usb_control_msg(
		handle,
		USB_TYPE_VENDOR|USB_RECIP_DEVICE|USB_ENDPOINT_OUT, // bRequestType
		USB_REQ_SET_FEATURE,
		0,	          	// wValue (LED off)
		0, 	         	// wIndex
		0,			// pointer to buffer containing data (no data here)
		0,			// wLength (no data here)
		1000			// timeout ms
	);

	if ( rc < 0 )
		fprintf(stderr,"%s: usb_control_msg()\n",strerror(-rc));
	else	printf("\nSent control, %d bytes: LED is OFF.\n",rc);

	return 0;			// cleanup_exit() will close
}

// End test1.cpp

