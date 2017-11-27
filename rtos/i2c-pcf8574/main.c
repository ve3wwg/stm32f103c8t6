/* Example of I2C to PCF8574 device
 *
 */
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#include "usbcdc.h"
#include "i2c.h"

#define PCF8574_ADDR(n)		(0x20|((n)&7))	// PCF8574
// #define PCF8574_ADDR(n)	(0x38|((n)&7))	// PCF8574A

static I2C_Control i2c;		// I2C Control struct

/*********************************************************************
 * Demo task 1
 *********************************************************************/

static void
task1(void *args __attribute__((unused))) {
	TickType_t t0;
	uint8_t addr = PCF8574_ADDR(0);	// I2C Address
	volatile uint16_t value = 0u;	// PCF8574P value
	volatile bool startedf = false;
	uint8_t byte;
	I2C_Fails fc;

	for (;;) {
		// Eat any pending chars
		while ( usb_peek() > 0 )
			usb_getc();

		// Prompt and await any key:
		do	{
			if ( !startedf && usb_peek() <= 0 ) {
				usb_printf("\n\nTask1 begun.\n\n"
					"Press any key to begin.\n");
				t0 = xTaskGetTickCount();
				do	{
					taskYIELD();
				} while ( xTaskGetTickCount() - t0 < 2000
				       && usb_peek() <= 0 );
			} else	taskYIELD();
		} while ( usb_peek() <= 0 );

		startedf = true;	// Note that we've started
		while ( usb_peek() > 0 )
			usb_getc();	// Eat any key(s)

		usb_puts("\nI2C Demo Begins "
			"(Press any key to stop)\n\n");

		// Configure I2C1
		i2c_configure(&i2c,I2C1,1000);

		// Until a key is pressed:
		while ( usb_peek() <= 0 ) {
			vTaskDelay(pdMS_TO_TICKS(1000));

			if ( (fc = setjmp(i2c_exception)) != I2C_Ok ) {
				// I2C Exception occurred:
				usb_printf("I2C Fail code %d\n\n",fc,i2c_error(fc));
				continue;
			}

			// Left four bits for input, are set to 1-bits
			// Right four bits being used for output:

			value = (value & 0xFF) | 0xF0;
			usb_printf("Writing $%02X -> I2C @ $%02X\n",value,addr);

			gpio_clear(GPIOC,GPIO13);	// PC13 LED lit

			// Write to PCF8574P
			i2c_start_addr(&i2c,addr,Write);
			i2c_write(&i2c,value&0x0FF);
			i2c_stop(&i2c);

			gpio_set(GPIOC,GPIO13);		// PC13 LED dark

			// Read back from PCF8574P
			usb_printf("Reading     <- I2C @ $%02X\n",addr);

			i2c_start_addr(&i2c,addr,Read);
			byte = i2c_read(&i2c,true);
			i2c_stop(&i2c);

			// Report what we read back:
			usb_printf("Read byte = $%02X vs $%02X\n",
				byte,value&0xFF);

			// Increment the four output bits
			value = (value+1) & 0x0F;
		}

		usb_printf("\nPress any key to restart.\n");
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();// Use this for "blue pill"
	rcc_periph_clock_enable(RCC_GPIOB);	// I2C
	rcc_periph_clock_enable(RCC_GPIOC);	// LED
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_I2C1);

	gpio_set_mode(GPIOB,
		GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
		GPIO6|GPIO7);			// I2C
	gpio_set(GPIOB,GPIO6|GPIO7);

	gpio_set_mode(GPIOC,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO13);			// LED on PC13
	gpio_set(GPIOC,GPIO13);			// PC13 LED dark
			     
	gpio_primary_remap(0,0); // AFIO_MAPR_I2C1_REMAP=0, PB6+PB7

	xTaskCreate(task1,"task1",800,NULL,1,NULL);
	usb_start(1,1);

	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End main.c
