/* Demo of I2C I/O with PCF8574 device
 * Warren Gay Sat Dec  9 17:36:29 2017
 *
 *	PCF8574 /INT	on PC14
 *	LED		on PC13
 *	I2C		on PB6, PB7
 */
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "usbcdc.h"
#include "i2c.h"

#define PCF8574_ADDR(n)		(0x20|((n)&7))	// PCF8574
// #define PCF8574_ADDR(n)	(0x38|((n)&7))	// PCF8574A

static I2C_Control i2c;			// I2C Control struct
static volatile bool readf = false; 	// ISR flag
static volatile int isr_count = 0;	// ISR count

/*********************************************************************
 * EXTI15 ISR
 *********************************************************************/

void
exti15_10_isr() {

	++isr_count;
	gpio_toggle(GPIOC,GPIO13);
	exti_reset_request(EXTI14);	// Reset cause of ISR
	readf = true;			// Indicate data change
}

/*********************************************************************
 * Consume any pending keyboard input, if any:
 *********************************************************************/

static void
eat_pending(void) {

	// Eat any pending chars
	while ( usb_peek() > 0 )
		usb_getc();
}

/*********************************************************************
 * Pause until a keyboard input has been pressed.
 *********************************************************************/

static void
wait_start(void) {
	static bool startedf = false;
	TickType_t t0;

	eat_pending();

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
	eat_pending();		// Consume any pending keystrokes
}

/*********************************************************************
 * Wait until:
 *	1. The ISR has set flag readf == True
 * or	2. One seconds has timed out
 *
 * RETURNS:
 *	Returns true if ISR flag is set.
 *********************************************************************/

static bool
wait_event(void) {
	TickType_t t0;

	if ( !readf ) {
		// Wait unless /INT or keypress
		t0 = xTaskGetTickCount();
		do	{
			taskYIELD();
			if ( readf )
				break;
		} while ( (xTaskGetTickCount()-t0 < 1000) && usb_peek() <= 0 );
	}
	if ( readf ) {
		readf = false;
		return true;	// ISR received interrupt
	}
	return false;		// No interrupt (timeout)
}

/*********************************************************************
 * Demo task 1
 *********************************************************************/

static void
task1(void *args __attribute__((unused))) {
	uint8_t addr = PCF8574_ADDR(0);	// I2C Address
	volatile unsigned line = 0u;	// Print line #
	volatile uint16_t value = 0u;	// PCF8574P value
	uint8_t byte = 0xFF;		// Read I2C byte
	volatile bool read_flag;	// True if Interrupted
	I2C_Fails fc;			// I2C fail code

	for (;;) {
		wait_start();
		usb_puts("\nI2C Demo Begins "
			"(Press any key to stop)\n\n");

		// Configure I2C1
		i2c_configure(&i2c,I2C1,1000);

		// Until a key is pressed:
		while ( usb_peek() <= 0 ) {
			if ( (fc = setjmp(i2c_exception)) != I2C_Ok ) {
				// I2C Exception occurred:
				usb_printf("I2C Fail code %d\n\n",fc,i2c_error(fc));
				break;
			}

			read_flag = wait_event();	// Interrupt or timeout

			// Left four bits for input, are set to 1-bits
			// Right four bits for output:

			value = (value & 0x0F) | 0xF0;
			usb_printf("Writing $%02X I2C @ $%02X\n",value,addr);
#if 0
			/*********************************************
			 * This example performs a write transaction,
			 * followed by a separate read transaction:
			 *********************************************/
			i2c_start_addr(&i2c,addr,Write);
			i2c_write(&i2c,value&0x0FF);
			i2c_stop(&i2c);

			i2c_start_addr(&i2c,addr,Read);
			byte = i2c_read(&i2c,true);
			i2c_stop(&i2c);
#else
			/*********************************************
			 * This example performs a write followed
			 * immediately by a read in one I2C transaction,
			 * using a "Repeated Start"
			 *********************************************/
			i2c_start_addr(&i2c,addr,Write);
			i2c_write_restart(&i2c,value&0x0FF,addr);
			byte = i2c_read(&i2c,true);
			i2c_stop(&i2c);
#endif
			if ( read_flag ) {
				// Received an ISR interrupt:
				if ( byte & 0b10000000 )
					usb_printf("%04u: BUTTON RELEASED: "
						"$%02X; wrote $%02X, ISR %d\n",
						++line,byte,value,isr_count);
				else	usb_printf("%04u: BUTTON PRESSED:  "
						"$%02X; wrote $%02X, ISR %d\n",
						++line,byte,value,isr_count);
			} else	{
				// No interrupt(s):
				usb_printf("%04u:           Read:  $%02X, "
					"wrote $%02X, ISR %d\n",
					++line,byte,value,isr_count);
			}
			value = (value + 1) & 0x0F;
		}

		usb_printf("\nPress any key to restart.\n");
	}
}

/*********************************************************************
 * Main routine and peripheral setup:
 *********************************************************************/

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();// For "blue pill"
	rcc_periph_clock_enable(RCC_GPIOB);	// I2C
	rcc_periph_clock_enable(RCC_GPIOC);	// LED
	rcc_periph_clock_enable(RCC_AFIO);	// EXTI
	rcc_periph_clock_enable(RCC_I2C1);	// I2C

	gpio_set_mode(GPIOB,
		GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
		GPIO6|GPIO7);			// I2C
	gpio_set(GPIOB,GPIO6|GPIO7);		// Idle high

	gpio_set_mode(GPIOC,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO13);			// LED on PC13
	gpio_set(GPIOC,GPIO13);			// PC13 LED dark
			     
	// AFIO_MAPR_I2C1_REMAP=0, PB6+PB7
	gpio_primary_remap(0,0); 

	gpio_set_mode(GPIOC,			// PCF8574 /INT
		GPIO_MODE_INPUT,		// Input
		GPIO_CNF_INPUT_FLOAT,
		GPIO14);			// on PC14

	exti_select_source(EXTI14,GPIOC);
	exti_set_trigger(EXTI14,EXTI_TRIGGER_FALLING);
	exti_enable_request(EXTI14);
	nvic_enable_irq(NVIC_EXTI15_10_IRQ);	// PC14 <- /INT

	xTaskCreate(task1,"task1",800,NULL,1,NULL);
	usb_start(1,1);

	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End main.c
