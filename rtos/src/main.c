/* Simple LED task demo:
 *
 * The LED on PC13 is toggled in task1.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#define mainECHO_TASK_PRIORITY				( tskIDLE_PRIORITY + 1 )

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,signed portCHAR *pcTaskName);

void
vApplicationStackOverflowHook(xTaskHandle *pxTask,signed portCHAR *pcTaskName) {
	(void)pxTask;
	(void)pcTaskName;
	for(;;);
}

static void
gpio_setup(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);
}

static void
task1(void *args) {
	int i;

	(void)args;

	for (;;) {
		gpio_toggle(GPIOC,GPIO13);
		for (i = 0; i < 300000; i++)
			__asm__("nop");
	}
}

int
main(void) {

	gpio_setup();
	xTaskCreate(task1,"LED",100,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;)
		;
	return 0;
}

// End
