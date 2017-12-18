/* Demo program for OLED 128x64 SSD1306 controller
 * Warren Gay   Sun Dec 17 22:49:14 2017
 *
 * Important!  	You must have a pullup resistor on the NSS
 * 	       	line in order that the NSS (/CS) SPI output
 *		functions correctly as a chip select. The
 *		SPI peripheral configures NSS pin as an
 *		open drain output.
 *
 * OLED		4-Wire SPI
 *
 * PINS:
 *	PC13	LED
 *	PA15	/CS (NSS, with 10k pullup)
 *	PB3	SCK
 *	PB5	MOSI (MISO not used)
 *	PB10	D/C
 *	PB11	/Reset
 */
#include <string.h>
#include <ctype.h>

#include "mcuio.h"
#include "miniprintf.h"
#include "intelhex.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "meter.h"
#include "oled.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
	
void
oled_command(uint8_t byte) {
	gpio_clear(GPIOB,GPIO10);
	spi_enable(SPI1);
	spi_xfer(SPI1,byte);
	spi_disable(SPI1);
}

void
oled_command2(uint8_t byte,uint8_t byte2) {
	gpio_clear(GPIOB,GPIO10);
	spi_enable(SPI1);
	spi_xfer(SPI1,byte);
	spi_xfer(SPI1,byte2);
	spi_disable(SPI1);
}

void
oled_data(uint8_t byte) {
	gpio_set(GPIOB,GPIO10);
	spi_enable(SPI1);
	spi_xfer(SPI1,byte);
	spi_disable(SPI1);
}

static void
oled_reset(void) {
	gpio_clear(GPIOB,GPIO11);
	vTaskDelay(1);
	gpio_set(GPIOB,GPIO11);
}

static void
oled_init(void) {
	static uint8_t cmds[] = {
		0xAE, 0x00, 0x10, 0x40, 0x81, 0xCF, 0xA1, 0xA6, 
		0xA8, 0x3F, 0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1, 
		0xDA, 0x12, 0xDB, 0x40, 0x8D, 0x14, 0xAF, 0xFF };

	gpio_clear(GPIOC,GPIO13);
	oled_reset();
	for ( unsigned ux=0; cmds[ux] != 0xFF; ++ux )
		oled_command(cmds[ux]);
	gpio_set(GPIOC,GPIO13);
}

/*
 * Monitor task:
 */
static void
monitor_task(void *arg __attribute((unused))) {
	struct Meter m1;
	bool menuf = true;
	float v = 1.3;
	char ch;
	
	std_printf("\nMonitor Task Started.\n");

	oled_init();
	meter_init(&m1,3.5);

	meter_set_value(&m1,v);
	meter_update();

	for (;;) {
		if ( menuf ) {
			std_printf(
				"\nTest Menu:\n"
				"  0 .. set to 0.0 volts\n"
				"  1 .. set to 1.0 volts\n"
				"  2 .. set to 2.0 volts\n"
				"  3 .. set to 3.0 volts\n"
				"  4 .. set to 3.5 volts\n"
				"  + .. increase by 0.1 volts\n"
				"  - .. decrease by 0.1 volts\n"
			);
		}
		menuf = false;

		std_printf("\n: ");
		ch = std_getc();

		if ( isalpha(ch) )
			ch = toupper(ch);
		std_printf("%c\n",ch);

		switch ( ch ) {
		case '?':
		case '\r':
		case '\n':
			menuf = true;
			break;		
		case '+':
			v += 0.1;
			meter_set_value(&m1,v);
			meter_update();
			break;
		case '-':
			v -= 0.1;
			meter_set_value(&m1,v);
			meter_update();
			break;
		case '0':
			v = 0.0;
			meter_set_value(&m1,v);
			meter_update();
			break;
		case '1':
			v = 1.0;
			meter_set_value(&m1,v);
			meter_update();
			break;
		case '2':
			v = 2.0;
			meter_set_value(&m1,v);
			meter_update();
			break;
		case '3':
			v = 3.0;
			meter_set_value(&m1,v);
			meter_update();
			break;
		case '4':
			v = 3.5;
			meter_set_value(&m1,v);
			meter_update();
			break;
		default:
			std_printf(" ???\n");
			menuf = true;
		}
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Blue pill

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_SPI1);

	// LED on PC13
	gpio_set_mode(GPIOC,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO13);
	gpio_set(GPIOC,GPIO13);	// PC13 = on

	// Put SPI1 on PB5/PB4/PB3/PA15
        gpio_primary_remap(
                AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_OFF, // Optional
		AFIO_MAPR_SPI1_REMAP);

	// PB10 -> D/C, PB11 -> RES
	gpio_set_mode(GPIOB,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO10|GPIO11);
	// Activate OLED Reset line
	gpio_clear(GPIOB,GPIO11); 

	// PB5=MOSI, PB3=SCK
	gpio_set_mode(
		GPIOB,
                GPIO_MODE_OUTPUT_50_MHZ,
        	GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
                GPIO5|GPIO3
	);
	// NSS=PA15
	gpio_set_mode(
		GPIOA,
                GPIO_MODE_OUTPUT_50_MHZ,
        	GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
                GPIO15
	);

	spi_reset(SPI1); 
	spi_init_master(
		SPI1,
                SPI_CR1_BAUDRATE_FPCLK_DIV_256,
                SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
		SPI_CR1_CPHA_CLK_TRANSITION_1,
	        SPI_CR1_DFF_8BIT,
	        SPI_CR1_MSBFIRST
	);
	spi_disable_software_slave_management(SPI1);
	spi_enable_ss_output(SPI1);

	usb_start(1,1);
	std_set_device(mcu_usb);			// Use USB for std I/O
	gpio_clear(GPIOC,GPIO13);			// PC13 = off

	xTaskCreate(monitor_task,"monitor",500,NULL,1,NULL);
	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End
