PROJECT INVENTORY:
------------------

blinky
------

Your most basic blink program, running from a FreeRTOS task.
Uses LED on PC13 (blue pill)

blinky2
-------
A blink function, running as a task, but using vTaskDelay()
without burning CPU cycles. This should blink precisely
at one second intervals: 500 ms off and 500 ms on.

uart
----

A simple USART demo that prints lines of text, 0123...Z and
repeats. It operates at 38400 baud, 8N1 and no flow control.

uart2
-----

This demo makes use of a FreeRTOS queue, which allows a 
task to queue up characters to be sent from the UART. The
serial parameters are 38400, 8N1 with no flow control.

usbcdc
------

This demo uses the USB peripheral to communicate as if it 
were a serial port.
