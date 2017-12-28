/* Demo program for OLED 128x64 SSD1306 controller
 *		using DMA transfers.
 * Warren Gay   Mon Dec 25 10:22:33 2017
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
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
	
static TaskHandle_t h_spidma = NULL;
static volatile bool dma_busy = false;
static volatile bool dma_idle = true;
static volatile bool dma_more = false;
static volatile uint8_t *pixmapp = NULL;
static volatile uint8_t pageno = 0;

static void start_dma(void);

/*********************************************************************
 * DMA ISR Routine
 *********************************************************************/

void
dma1_channel3_isr(void) {
	BaseType_t woken __attribute__((unused)) = pdFALSE;

	if ( dma_get_interrupt_flag(DMA1,DMA_CHANNEL3,DMA_TCIF) )
		dma_clear_interrupt_flags(DMA1,DMA_CHANNEL3,DMA_TCIF);

        spi_disable_tx_dma(SPI1);

	// Notify spidma_task to start another:
	vTaskNotifyGiveFromISR(h_spidma,&woken);
}

/*********************************************************************
 * Enable DMA1, Channel 3 for SPI1 I/O
 *********************************************************************/

static void
spi_dma_transmit(volatile uint8_t *tx_buf,int tx_len) {

	dma_idle = false;
	dma_busy = true;

	dma_disable_channel(DMA1,DMA_CHANNEL3);
        dma_set_memory_address(DMA1,DMA_CHANNEL3,(uint32_t)tx_buf);
        dma_set_number_of_data(DMA1,DMA_CHANNEL3,tx_len);
	dma_enable_channel(DMA1,DMA_CHANNEL3);
	spi_enable_tx_dma(SPI1);
	spi_enable(SPI1);
}

/*********************************************************************
 * Task to manage SPI1 & DMA1
 *********************************************************************/

static void
spidma_task(void *arg __attribute((unused))) {
	static uint8_t cmds[] = {
		0x20, 0x02,	// 0: Page mode
		0x40,		// 2: Display start line
		0xD3, 0x00,	// 3: Display offset
		0xB0,		// 5: Page #
		0x00,		// 6: Lo col
		0x10		// 7: Hi Col
	};

	for (;;) {
		// Block until ISR notifies
		ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
		if ( dma_busy ) {
			spi_clean_disable(SPI1);
			dma_busy = false;
			if ( gpio_get(GPIOB,GPIO10) ) {
				// Advance data
				pixmapp += 128;
				++pageno;
			}
			// Toggle between Command/Data
			gpio_toggle(GPIOB,GPIO10);
		}

		if ( pageno >= 8 ) {
			// All OLED pages sent:
			dma_idle = true;
			if ( dma_more ) {
				// Restart update
				dma_more = false;
				start_dma();
			}
		} else	{
			// Another page to send:
			cmds[5] = 0xB0 | pageno;
			if ( !gpio_get(GPIOB,GPIO10) ) {
				// Send commands:
				if ( !pageno )
					spi_dma_transmit(&cmds[0],8);
				else	spi_dma_transmit(&cmds[5],3);
			} else	{
				// Send page data:
				spi_dma_transmit(pixmapp,128);
			}
		}
	}
}

/*********************************************************************
 * Start DMA transfer from OLED Page 0
 *********************************************************************/

static void
start_dma(void) {
	extern uint8_t pixmap[128*64/8];

	pageno = 0;
	pixmapp = &pixmap[0];
	gpio_clear(GPIOB,GPIO10); // Cmd mode
	xTaskNotifyGive(h_spidma);
}

/*********************************************************************
 * Initiate a DMA OLED update or Queue repeat update
 *********************************************************************/

void
spi_dma_xmit_pixmap(void) {
	bool prime = false;

	taskENTER_CRITICAL();
	if ( !dma_idle ) {
		// Restart dma at DMA completion
		dma_more = true;// Restart upon completion
	} else	{
		prime = true;	// Start from idle
	}
	taskEXIT_CRITICAL();

	if ( prime )
		start_dma();	// Start from idle
}

/*********************************************************************
 * Reset the OLED device
 *********************************************************************/

static void
oled_reset(void) {

	gpio_clear(GPIOB,GPIO11);
	vTaskDelay(1);
	gpio_set(GPIOB,GPIO11);
}

/*********************************************************************
 * Configure DMA controller, except for address & length
 *********************************************************************/

static void
dma_init(void) {

	dma_channel_reset(DMA1,DMA_CHANNEL3);
        dma_set_peripheral_address(DMA1,DMA_CHANNEL3,(uint32_t)&SPI1_DR);
        dma_set_read_from_memory(DMA1,DMA_CHANNEL3);
        dma_enable_memory_increment_mode(DMA1,DMA_CHANNEL3);
        dma_set_peripheral_size(DMA1,DMA_CHANNEL3,DMA_CCR_PSIZE_8BIT);
        dma_set_memory_size(DMA1,DMA_CHANNEL3,DMA_CCR_MSIZE_8BIT);
        dma_set_priority(DMA1,DMA_CHANNEL3,DMA_CCR_PL_HIGH);
	dma_enable_transfer_complete_interrupt(DMA1,DMA_CHANNEL3);
}

/*********************************************************************
 * Initialize the OLED SSD1306 controller
 *********************************************************************/

static void
oled_init(void) {
	static uint8_t cmds[] = {
		0xAE, 0x00, 0x10, 0x40, 0x81, 0xCF, 0xA1, 0xA6, 
		0xA8, 0x3F, 0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1, 
		0xDA, 0x12, 0xDB, 0x40, 0x8D, 0x14, 0xAF, 0xFF };

	gpio_clear(GPIOC,GPIO13);
	oled_reset();

	gpio_clear(GPIOB,GPIO10);
	spi_enable(SPI1);
	for ( unsigned ux=0; cmds[ux] != 0xFF; ++ux )
		spi_xfer(SPI1,cmds[ux]);
	spi_disable(SPI1);

	gpio_set(GPIOC,GPIO13);
}

/*********************************************************************
 * Pummel the meter with updates. This tests that the DMA control task
 * does not get overwelmed or incapacitated with high levels of updates
 *********************************************************************/

static void
pummel_test(struct Meter *m1) {
	TickType_t t0 = xTaskGetTickCount();
	double v = 0.0;
	double incr = 0.05;

	meter_set_value(m1,v);
	meter_update();
	while ( (xTaskGetTickCount() - t0) < 5000 ) {
		vTaskDelay(6);
		v += incr;
		if ( v > 3.3 ) {
			incr = -0.05;
			v = 3.3;
		} else if ( v < 0.0 ) {
			v = 0.0;
			incr = 0.05;
		}
		meter_set_value(m1,v);
		meter_update();
	}
}

/*********************************************************************
 * Monitor task
 *********************************************************************/

static void
monitor_task(void *arg __attribute((unused))) {
	struct Meter m1;
	bool menuf = true;
	float v = 1.3;
	char ch;
	
	(void)std_getc();
	std_printf("\nMonitor Task Started.\n");

	oled_init();
	dma_init();
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
				"  p .. Meter pummel test\n"
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
		case 'P':
			std_printf("Meter pummel test..\n");
			pummel_test(&m1);
			std_printf("Test ended.\n");
			break;
		default:
			std_printf(" ???\n");
			menuf = true;
		}
	}
}

/*********************************************************************
 * Main program & initialization
 *********************************************************************/

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
                SPI_CR1_BAUDRATE_FPCLK_DIV_64, // 1.125 MHz
                SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
		SPI_CR1_CPHA_CLK_TRANSITION_1,
	        SPI_CR1_DFF_8BIT,
	        SPI_CR1_MSBFIRST
	);
	spi_disable_software_slave_management(SPI1);
	spi_enable_ss_output(SPI1);

	// DMA
	rcc_periph_clock_enable(RCC_DMA1);
	nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ,0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);

	usb_start(1,1);
	std_set_device(mcu_usb);			// Use USB for std I/O
	gpio_clear(GPIOC,GPIO13);			// PC13 = off

	xTaskCreate(monitor_task,"monitor",500,NULL,1,NULL);
	xTaskCreate(spidma_task,"spi_dma",100,NULL,1,&h_spidma);
	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End main.c
