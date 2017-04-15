PROJECT TIMER1:
---------------

This project implements a 100 usec timer visible on PC13 (LED output).
You will need a scope or logic analyzer to see this pulse.

This is a very simple application of Timer 2 (TIM2). GPIOC13 is
configured for output (as an LED), while TIM2 is configured to generate
a single 100 usec timer event.

The program sets GPIOC13 high when TIM2 is started. The GPIO remains
high until TIM2 expires, at which point the program resets PC13 low
again.

LIMITATIONS:

Having software set PC13 high, and then low around the timer leads to
some inaccuracy in timing. This is especially true in FreeRTOS where
other threads and premption may have a larger effect.

