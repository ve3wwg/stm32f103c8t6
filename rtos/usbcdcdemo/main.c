/* usbcdcdemo.c : USBCDC Example (adventure)
 * Warren Gay
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include "common.h"
#include "usbcdc.h"

static SemaphoreHandle_t sem_flash = 0;

static void
flasher(void *arg __attribute__((unused))) {

	for (;;) {
		xSemaphoreTake(sem_flash,portMAX_DELAY);
		gpio_toggle(GPIOC,GPIO13);
		xSemaphoreGive(sem_flash);
		vTaskDelay(pdMS_TO_TICKS(400));
	}
}

void
set_lamp(enum LampActions action) {
	static bool have = false;

	switch ( action ) {
	case Take:
		if ( have ) {
			xSemaphoreGive(sem_flash);
			have = false;
		}
		break;
	case Filled:
		if ( !have ) {
			xSemaphoreTake(sem_flash,portMAX_DELAY);
			gpio_clear(GPIOC,GPIO13); // LED on
			have = true;
		}
		break;
	case Drop:
		if ( !have ) {
			xSemaphoreTake(sem_flash,portMAX_DELAY);
			have = true;
		}
		gpio_set(GPIOC,GPIO13);		// LED off
		break;
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	usb_start();

	xTaskCreate(adventure,"game",300,NULL,configMAX_PRIORITIES-1,NULL);
	xTaskCreate(flasher,"flash",100,NULL,configMAX_PRIORITIES-1,NULL);

	sem_flash = xSemaphoreCreateMutex();
	set_lamp(Drop);

	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End main.c
