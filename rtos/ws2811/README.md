NOTES:
------

This is a demo of the Timer capability, to generate from the timer a
data signal for the WS2811 LED string. The only software involvement
is to change the timer for each bit inside of the ISR.

Presently the usbcdc monitor program is included to allow examination
of the registers and to control when to execute the setup and the
timer start (this will change later).
