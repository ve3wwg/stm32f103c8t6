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
