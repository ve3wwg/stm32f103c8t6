NOTES:
------

This is a demo of a USB CDC device for the STM32F103C8T6 (tested using
MacOS). Any input data that is alphabetic, will be echoed back with 
the case inverted: lower case becomes upper and vice versa. All other
input data is echoed back to the terminal as is.

This demo performs data flow control, by not reading the USB device
when input capacity has been reached (FreeRTOS rx queue is full). Once
the rxtx_task() has caught up, the remaining input will be read without
data loss. An easy way to test this is to paste text into your terminal
window (minicom etc.)

This project makes use of libwwg/usbcdc.c (from libwwg.a)
