/* usbcdcdemo.c : USBCDC Example
 *
 * Warren Gay
 */
#include <FreeRTOS.h>
#include <task.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include <usbcdc.h>

static void
demotask(void *args __attribute__((unused))) {
	char name[40];
	int namelen;

	for (;;) {
		gpio_toggle(GPIOC,GPIO13);

		usb_puts("\nEnter your name: ");
		usb_gets(name,sizeof name);

		if ( name[0] == '\n' ) {
			usb_printf("No name entered. Please try again.\n");
			continue;
		}

		namelen = strlen(name);
		if ( namelen > 0 && name[namelen-1] == '\n' )
			name[namelen-1] = 0;		// Stomp out newline

		usb_printf("Hello %s!\n",name);
		gpio_toggle(GPIOC,GPIO13);
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	usb_start(true);

	xTaskCreate(demotask,"demo",100,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End
