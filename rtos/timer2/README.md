TIMER2 PROJECT
--------------

This project is an timer (TIM2) example, using the Output Compare
feature, outputting to TIM2.CH2 (GPIOA1). This highlights the timer's
ability to deactivate a GPIO output when the output compare value is
reached.

This example also illustrates the use of the TIM_OCM_FORCE_HIGH feature,
allowing the software to establish an initial state prior to starting
the counter. Once the counter is started, it will count up until the
compare value is reached to generate a 100us pulse. When that happens
the timer itself will set the GPIO line low again. In FreeRTOS, this
guarantees that the line is set low, even if task scheduling is delayed.

WEAKNESS:
---------

This example still has one timing vulnerability. The software sets the
GPIOA1 high using TIM_OCM_FORCE_HIGH, but may be preemptively delayed
before starting the timer. This can lead to a longer than expected 100us
pulse. This problem is circumvented by disabling interrupts briefly,
prior to starting the timer. Once the timer has been started, interrupts
are reenabled.

