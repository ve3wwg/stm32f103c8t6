NOTES:
------

This is a demo of a USB CDC device for the STM32F103C8T6 (tested using
MacOS). The demo provides a text menu driven system that allows the
user to display STM32F103 registers (the STM32F103 register set is
assumed). Pressing '?' or RETURN will prompt with a menu. Entering
a single character like 'g' will display [GPIO configuration] register
settings.

This project makes use of libwwg/src/usbcdc.c (from libwwg.a)
