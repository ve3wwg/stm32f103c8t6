/* Winbond W25Qxx Routines
 * Warren Gay Sat Oct 28 19:22:56 2017
 */
#include <string.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#include "winbond.h"

/*********************************************************************
 * Read status register 1
 *********************************************************************/

uint8_t
w25_read_sr1(uint32_t spi) {
	uint8_t sr1;

	spi_enable(spi);
	spi_xfer(spi,W25_CMD_READ_SR1);
	sr1 = spi_xfer(spi,DUMMY);
	spi_disable(spi);
	return sr1;
}

/*********************************************************************
 * Read status register 2
 *********************************************************************/

uint8_t
w25_read_sr2(uint32_t spi) {
	uint8_t sr1;

	spi_enable(spi);
	spi_xfer(spi,W25_CMD_READ_SR2);
	sr1 = spi_xfer(spi,0x00);
	spi_disable(spi);
	return sr1;
}

/*********************************************************************
 * Wait until not busy
 *********************************************************************/

void
w25_wait(uint32_t spi) {

	while ( w25_read_sr1(spi) & W25_SR1_BUSY )
		taskYIELD();
}

/*********************************************************************
 * Test if write protected
 *********************************************************************/

bool
w25_is_wprotect(uint32_t spi) {

	w25_wait(spi);
	return !(w25_read_sr1(spi) & W25_SR1_WEL);
}

/*********************************************************************
 * Write enable/disable:
 *********************************************************************/

void
w25_write_en(uint32_t spi,bool en) {

	w25_wait(spi);

	spi_enable(spi);
	spi_xfer(spi,en ? W25_CMD_WRITE_EN : W25_CMD_WRITE_DI);
	spi_disable(spi);
}

/*********************************************************************
 * Read Manufacturer & device
 *********************************************************************/

uint16_t
w25_manuf_device(uint32_t spi) {
	uint16_t info;

	w25_wait(spi);
	spi_enable(spi);
	spi_xfer(spi,W25_CMD_MANUF_DEVICE);	// Byte 1
	spi_xfer(spi,DUMMY);			// Dummy1 (2)
	spi_xfer(spi,DUMMY);			// Dummy2 (3)
	spi_xfer(spi,0x00);			// Byte 4
	info = spi_xfer(spi,DUMMY) << 8;	// Byte 5
	info |= spi_xfer(spi,DUMMY);		// Byte 6
	spi_disable(spi);
	return info;
}

/*********************************************************************
 * Read JEDEC ID
 *********************************************************************/

uint32_t
w25_JEDEC_ID(uint32_t spi) {
	uint32_t info;

	w25_wait(spi);
	spi_enable(spi);
	spi_xfer(spi,W25_CMD_JEDEC_ID);
	info = spi_xfer(spi,DUMMY);		 // Manuf.
	info = (info << 8) | spi_xfer(spi,DUMMY);// Memory Type
	info = (info << 8) | spi_xfer(spi,DUMMY);// Capacity
	spi_disable(spi);

	return info;
}

/*********************************************************************
 * Read Unique Device ID
 *********************************************************************/

void
w25_read_uid(uint32_t spi,void *buf,uint16_t bytes) {
	uint8_t *udata = (uint8_t*)buf;

	if ( bytes > 8 )
		bytes = 8;
	else if ( bytes <= 0 )
		return;

	w25_wait(spi);
	spi_enable(spi);
	spi_xfer(spi,W25_CMD_READ_UID);
	for ( uint8_t ux=0; ux<4; ++ux )
		spi_xfer(spi,DUMMY);
	for ( uint8_t ux=0; ux<bytes; ++ux )
		udata[ux] = spi_xfer(spi,DUMMY);
	spi_disable(spi);
}

/*********************************************************************
 * Power On/Off
 *********************************************************************/

void
w25_power(uint32_t spi,bool on) {

	if ( !on )
		w25_wait(spi);
	spi_enable(spi);
	spi_xfer(spi,on ? W25_CMD_PWR_ON : W25_CMD_PWR_OFF);
	spi_disable(spi);
}

/*********************************************************************
 * Chip Erase;
 *********************************************************************/

bool
w25_chip_erase(uint32_t spi) {

	if ( w25_is_wprotect(spi) )
		return false;

	spi_enable(spi);
	spi_xfer(spi,W25_CMD_CHIP_ERASE);
	spi_disable(spi);

	return w25_is_wprotect(spi);	// True if successful 
}

/*********************************************************************
 * Read Data
 *********************************************************************/

uint32_t		// New address is returned
w25_read_data(uint32_t spi,uint32_t addr,void *data,uint32_t bytes) {
	uint8_t *udata = (uint8_t*)data;

	w25_wait(spi);

	spi_enable(spi);
	spi_xfer(spi,W25_CMD_FAST_READ);
	spi_xfer(spi,addr >> 16);
	spi_xfer(spi,(addr >> 8) & 0xFF);
	spi_xfer(spi,addr & 0xFF);
	spi_xfer(spi,DUMMY);

	for ( ; bytes-- > 0; ++addr )
		*udata++ = spi_xfer(spi,0x00);

	spi_disable(spi);
	return addr;	
}

/*********************************************************************
 * Write data
 *********************************************************************/

unsigned		// New address is returned
w25_write_data(uint32_t spi,uint32_t addr,void *data,uint32_t bytes) {
	uint8_t *udata = (uint8_t*)data;

	w25_write_en(spi,true);
	w25_wait(spi);

	if ( w25_is_wprotect(spi) )
		return 0xFFFFFFFF;	// Indicate error

	while ( bytes > 0 ) {
		spi_enable(spi);
		spi_xfer(spi,W25_CMD_WRITE_DATA);
		spi_xfer(spi,addr >> 16);
		spi_xfer(spi,(addr >> 8) & 0xFF);
		spi_xfer(spi,addr & 0xFF);
		while ( bytes > 0 ) {
			spi_xfer(spi,*udata++);
			--bytes;
			if ( (++addr & 0xFF) == 0x00 )
				break;
		}
		spi_disable(spi);
	
		if ( bytes > 0 )
			w25_write_en(spi,true); // More to write
	}

	return addr;	
}

/*********************************************************************
 * Erase 4K/32K/64K block
 *********************************************************************/

bool
w25_erase_block(uint32_t spi,uint32_t addr,uint8_t cmd) {
	
	if ( w25_is_wprotect(spi) )
		return false;

	switch ( cmd ) {
	case W25_CMD_ERA_SECTOR:
		addr &= ~(4*1024-1);
		break;
	case W25_CMD_ERA_32K:
		addr &= ~(32*1024-1);
		break;
	case W25_CMD_ERA_64K:
		addr &= ~(64*1024-1);
		break;
	default:
		return false;
	}

	spi_enable(spi);
	spi_xfer(spi,cmd);
	spi_xfer(spi,addr >> 16);
	spi_xfer(spi,(addr >> 8) & 0xFF);
	spi_xfer(spi,addr & 0xFF);
	spi_disable(spi);

	return w25_is_wprotect(spi); // True if successful
}

/*********************************************************************
 * Setup SPI
 *********************************************************************/

void
w25_spi_setup(
  uint32_t spi,		// SPI1 or SPI2
  bool bits8,		// True for 8-bits else 16-bits
  bool msbfirst,	// True if MSB first else LSB first
  bool mode0,		// True if mode 0 else mode 3
  uint8_t fpclk_div	// E.g. SPI_CR1_BAUDRATE_FPCLK_DIV_256
) {

	rcc_periph_clock_enable(spi == SPI1 ? RCC_SPI1 : RCC_SPI2);
	if ( spi == SPI1 ) {
		gpio_set_mode(
			GPIOA,
	                GPIO_MODE_OUTPUT_50_MHZ,
	        	GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
	                GPIO4|GPIO5|GPIO7		// NSS,SCK,MOSI
		);
		gpio_set_mode(
			GPIOA,
			GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_FLOAT,
			GPIO6				// MISO
		);
		gpio_set(GPIOA,GPIO4);			// Set high (needed?)
	} else	{
		gpio_set_mode(
			GPIOB,
	                GPIO_MODE_OUTPUT_50_MHZ,
	        	GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
	                GPIO12|GPIO13|GPIO15		// NSS,SCK,MOSI
		);
		gpio_set_mode(
			GPIOB,
			GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_FLOAT,
			GPIO14				// MISO
		);
		gpio_set(GPIOB,GPIO12);			// Set high
	}

	spi_reset(spi);

	spi_init_master(
		spi,
                fpclk_div,				// SPI_CR1_BAUDRATE_FPCLK_DIV_256 etc.
		mode0 ? SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE : SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
		mode0 ? SPI_CR1_CPHA_CLK_TRANSITION_1 : SPI_CR1_CPHA_CLK_TRANSITION_2,
	        bits8 ? SPI_CR1_DFF_8BIT : SPI_CR1_DFF_16BIT,
	        msbfirst ? SPI_CR1_MSBFIRST : SPI_CR1_LSBFIRST
	);

	spi_disable_software_slave_management(spi);
	spi_enable_ss_output(spi);
}

// End
