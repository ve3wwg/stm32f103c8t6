/* Winbond W25Q32 Code
 * Warren Gay Fri Oct 27 23:52:33 2017
 *
 * Important!  	You must have a pullup resistor on the NSS
 * 	       	line in order that the NSS (/CS) SPI output
 *		functions correctly as a chip select. The
 *		SPI peripheral configures NSS pin as an
 *		open drain output.
 *
 * PINS:
 *	PC13	LED
 *	PA4	/CS (NSS, with 10k pullup)
 *	PA5	SCK
 *	PA6	MISO
 *	PA7	MOSI
 */
#include <string.h>
#include <ctype.h>

#include "mcuio.h"
#include "miniprintf.h"
#include "intelhex.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#define W25_CMD_MANUF_DEVICE	0x90
#define W25_CMD_JEDEC_ID	0x9F
#define W25_CMD_WRITE_EN	0x06
#define W25_CMD_WRITE_DI	0x04
#define W25_CMD_READ_SR1	0x05
#define W25_CMD_READ_SR2	0x35
#define W25_CMD_CHIP_ERASE	0xC7
#define W25_CMD_READ_DATA	0x03
#define W25_CMD_FAST_READ	0x0B
#define W25_CMD_WRITE_DATA	0x02
#define W25_CMD_READ_UID	0x4B
#define W25_CMD_PWR_ON		0xAB
#define W25_CMD_PWR_OFF		0xB9
#define W25_CMD_ERA_SECTOR	0x20
#define W25_CMD_ERA_32K		0x52
#define W25_CMD_ERA_64K		0xD8

#define DUMMY			0x00

#define W25_SR1_BUSY		0x01
#define W25_SR1_WEL		0x02

static const char *cap[3] = {
	"W25X16",	// 14
	"W25X32",	// 15
	"W25X64"	// 16
};	

static uint8_t
w25_read_sr1(uint32_t spi) {
	uint8_t sr1;

	spi_enable(spi);
	spi_xfer(spi,W25_CMD_READ_SR1);
	sr1 = spi_xfer(spi,DUMMY);
	spi_disable(spi);
	return sr1;
}

static uint8_t
w25_read_sr2(uint32_t spi) {
	uint8_t sr1;

	spi_enable(spi);
	spi_xfer(spi,W25_CMD_READ_SR2);
	sr1 = spi_xfer(spi,DUMMY);
	spi_disable(spi);
	return sr1;
}

static void
w25_wait(uint32_t spi) {

	while ( w25_read_sr1(spi) & W25_SR1_BUSY )
		taskYIELD();
}

static bool
w25_is_wprotect(uint32_t spi) {

	w25_wait(spi);
	return !(w25_read_sr1(spi) & W25_SR1_WEL);
}

static void
w25_write_en(uint32_t spi,bool en) {

	w25_wait(spi);

	spi_enable(spi);
	spi_xfer(spi,en ? W25_CMD_WRITE_EN : W25_CMD_WRITE_DI);
	spi_disable(spi);

	w25_wait(spi);
}

static uint16_t
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

static uint32_t
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

static void
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

static void
w25_power(uint32_t spi,bool on) {

	if ( !on )
		w25_wait(spi);
	spi_enable(spi);
	spi_xfer(spi,on ? W25_CMD_PWR_ON : W25_CMD_PWR_OFF);
	spi_disable(spi);
}

static bool
w25_chip_erase(uint32_t spi) {

	if ( w25_is_wprotect(spi) ) {
		std_printf("Not Erased! Chip is not write enabled.\n");
		return false;
	}

	spi_enable(spi);
	spi_xfer(spi,W25_CMD_CHIP_ERASE);
	spi_disable(spi);

	std_printf("Erasing chip..\n");

	if ( !w25_is_wprotect(spi) ) {
		std_printf("Not Erased! Chip erase failed.\n");
		return false;
	}

	std_printf("Chip erased!\n");
	return true;
}

static uint32_t		// New address is returned
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
		*udata++ = spi_xfer(spi,DUMMY);

	spi_disable(spi);
	return addr;	
}

static unsigned		// New address is returned
w25_write_data(uint32_t spi,uint32_t addr,void *data,uint32_t bytes) {
	uint8_t *udata = (uint8_t*)data;

	w25_write_en(spi,true);
	w25_wait(spi);

	if ( w25_is_wprotect(spi) ) {
		std_printf("Write disabled.\n");
		return 0xFFFFFFFF;	// Indicate error
	}

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

static void
w25_erase_block(uint32_t spi,uint32_t addr,uint8_t cmd) {
	const char *what;
	
	if ( w25_is_wprotect(spi) ) {
		std_printf("Write protected. Erase not performed.\n");
		return;
	}

	switch ( cmd ) {
	case W25_CMD_ERA_SECTOR:
		what = "sector";
		addr &= ~(4*1024-1);
		break;
	case W25_CMD_ERA_32K:
		what = "32K block";
		addr &= ~(32*1024-1);
		break;
	case W25_CMD_ERA_64K:
		what = "64K block";
		addr &= ~(64*1024-1);
		break;
	default:
		return;	// Should not happen
	}

	spi_enable(spi);
	spi_xfer(spi,cmd);
	spi_xfer(spi,addr >> 16);
	spi_xfer(spi,(addr >> 8) & 0xFF);
	spi_xfer(spi,addr & 0xFF);
	spi_disable(spi);

	std_printf("%s erased, starting at %06X\n",
		what,(unsigned)addr);
}

static void
flash_status(void) {
	uint8_t s;

	s = w25_read_sr1(SPI1);
	std_printf("SR1 = %02X (%s)\n",
		s,
		s & W25_SR1_WEL
			? "write enabled"
			: "write protected");
	std_printf("SR2 = %02X\n",w25_read_sr2(SPI1));
}

static unsigned
get_data24(const char *prompt) {
	unsigned v = 0u, count = 0u;
	char ch;

	std_printf("%s: ",prompt);

	while ( (ch = std_getc()) != '\r' && ch != '\n' ) {
		if ( ch == '\b' || ch == 0x7F ) {
			v >>= 4;
			std_puts("\b \b");
			if ( count > 0 )
				--count;
			continue;
		}
		if ( ch >= '0' && ch <= '9' ) {
			v <<= 4;
			v |= ch & 0x0F;
			std_putc(ch);
		} else 	{
			if ( isalpha(ch) )
				ch = toupper(ch);
			if ( ch >= 'A' && ch <= 'F' ) {
				v <<= 4;
				v |= ((ch & 0x0F) - 1 + 10);
				std_putc(ch);
			} else	{
				std_puts("?\b");
				continue;
			}
		}
		if ( ++count > 6 )
			break;
	}
	return v & 0xFFFFFF;
}

static unsigned
get_data8(const char *prompt) {
	unsigned v = 0u, count = 0u;
	char ch;

	if ( prompt )
		std_printf("%s: ",prompt);

	while ( (ch = std_getc()) != '\r' && ch != '\n' && !strchr(",./;\t",ch) ) {
		if ( ch == '"' || ch == '\'' ) {
			std_putc(ch);
			v = std_getc();
			std_putc(v);
			count = 1;
			break;
		}
		if ( ch == '\b' || ch == 0x7F ) {
			v >>= 4;
			std_puts("\b \b");
			if ( count > 0 )
				--count;
			continue;
		}
		if ( ch >= '0' && ch <= '9' ) {
			v <<= 4;
			v |= ch & 0x0F;
			std_putc(ch);
		} else 	{
			if ( isalpha(ch) )
				ch = toupper(ch);
			if ( ch >= 'A' && ch <= 'F' ) {
				v <<= 4;
				v |= ((ch & 0x0F) - 1 + 10);
				std_putc(ch);
			} else	{
				std_puts("?\b");
				continue;
			}
		}
		if ( ++count >= 2 )
			break;
	}
	if ( !count )
		return 0xFFFF;	// No data
	return v & 0xFF;
}

static uint32_t
dump_page(uint32_t spi,uint32_t addr) {
	char buf[17];

	addr &= ~0xFF;		// Start on page boundary

	for ( int x=0; x<16; ++x, addr += 16 ) {
		std_printf("%06X ",(unsigned)addr);
		w25_read_data(spi,addr,buf,16);
		for ( uint32_t offset=0; offset<16; ++offset )
			std_printf("%02X ",buf[offset]);
		for ( uint32_t offset=0; offset<16; ++offset ) {
			if ( buf[offset] < ' ' || buf[offset] >= 0x7F )
				std_putc('.');
			else	std_putc(buf[offset]);
		}
		std_putc('\n');
	}
	return addr;
}

static void
erase(uint32_t spi,uint32_t addr) {
	const char *what;
	char ch;

	if ( w25_is_wprotect(spi) ) {
		std_printf("Write protected. Erase not possible.\n");
		return;
	}

	std_printf(
		"\nErase what?\n"
		"  s ... Erase 4K sector\n"
		"  b ... Erase 32K block\n"
		"  z ... Erase 64K block\n"
		"  c ... Erase entire chip\n"
		"\nanything else to cancel\n: ");
		
	ch = std_getc();
	if ( isupper(ch) )
		ch = tolower(ch);

	std_putc(ch);
	std_putc('\n');

	switch ( ch ) {
	case 's':
		w25_erase_block(spi,addr,W25_CMD_ERA_SECTOR);
		what = "Sector";
		break;
	case 'b':
		w25_erase_block(spi,addr,W25_CMD_ERA_32K);
		what = "32K block";
		break;
	case 'z':
		w25_erase_block(spi,addr,W25_CMD_ERA_64K);
		what = "64K block";
		break;
	case 'c':
		w25_chip_erase(SPI1);
		return;
	default:
		std_printf("Erase CANCELLED.\n");
		return;
	}

	if ( w25_is_wprotect(spi) )
		std_printf("%s erased.\n",what);
	else	std_printf("%s FAILED.\n",what);
}

static void
load_ihex(uint32_t spi) {
	s_ihex ihex;
	char buf[200], ch;
	unsigned rtype, count = 0, ux;

	if ( w25_is_wprotect(spi) ) {
		std_printf("Flash is write protected.\n");
		return;
	}

	ihex_init(&ihex);
	std_printf("\nReady for Intel Hex upload:\n");

	for (;;) {
		std_printf("%08X ",(unsigned)ihex.compaddr);

		while ( (ch = std_getc()) != ':' ) {
			if ( ch == 0x1A || ch == 0x04 ) {
				std_printf("EOF\n");
				return;		// ^Z or ^D ends transmission
			}
		}
		buf[0] = ch;
		std_putc(ch);

		for (  ux=1; ux+1<sizeof buf; ++ux ) {
			buf[ux] = ch = std_getc();
			if ( ch == '\r' || ch == '\n' )
				break;
			if ( ch == 0x1A || ch == 0x04 ) {
				std_printf("(EOF)\n");
				return;		// ^Z or ^D ends transmission
			}
			std_putc(ch);
		}
		buf[ux] = 0;		
		std_putc('\n');

		if ( !strchr(buf,':') ) {
			// Skip line with no hex
			continue;
		}

		rtype = ihex_parse(&ihex,buf);
		
		switch ( rtype ) {
		case IHEX_RT_DATA:	// data record
			w25_write_data(spi,ihex.addr&0x00FFFFFF,ihex.data,ihex.length);
			ihex.compaddr += ihex.length;
			break;
		case IHEX_RT_EOF:	// end	// of-file record
			break;
		case IHEX_RT_XSEG:	// extended segment address record
			break;
		case IHEX_RT_XLADDR:	// extended linear address record
			ihex.compaddr = ihex.baseaddr + ihex.addr;
			break;
		case IHEX_RT_SLADDR:	// start linear address record (MDK-ARM)
			break;
		default:
			std_printf("Error %02X: '%s'\n",(unsigned)rtype,buf);
			continue;
		}
		++count;
		
		if ( rtype == IHEX_RT_EOF )
			break;
		if ( strchr(buf,0x1A) || strchr(buf,0x04) )
			break;			// EOF from ascii-xfr
	}
}

/*
 * Monitor task:
 */
static void
monitor_task(void *arg __attribute((unused))) {
	int ch, devx;
	unsigned addr = 0u;
	uint8_t data = 0, idbuf[8];
	uint32_t info;
	const char *device;
	bool menuf = true;
	
	std_printf("\nMonitor Task Started.\n");

	for (;;) {
		if ( menuf ) {
			std_printf(
				"\nWinbond Flash Menu:\n"
				"  0 ... Power down\n"
				"  1 ... Power on\n"
				"  a ... Set address\n"
				"  d ... Dump page\n"
				"  e ... Erase (Sector/Block/64K/Chip)\n"
				"  i ... Manufacture/Device info\n"
				"  h ... Ready to load Intel hex\n"
				"  j ... JEDEC ID info\n"
				"  r ... Read byte\n"
				"  p ... Program byte(s)\n"
				"  s ... Flash status\n"
				"  u ... Read unique ID\n"
				"  w ... Write Enable\n"
				"  x ... Write protect\n"
			);
			std_printf("\nAddress: %06X\n",addr);
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
		case '0':
			w25_power(SPI1,0);
			break;
		case '1':
			w25_power(SPI1,1);
			break;
		case 'I':
			info = w25_manuf_device(SPI1);
			devx = (int)(info & 0xFF)-0x14;
			if ( devx < 3 )
				device = cap[devx];
			else	device = "unknown";
			std_printf("Manufacturer $%02X Device $%02X (%s)\n",
				(uint16_t)info>>8,(uint16_t)info&0xFF,
				device);
			break;
		case 'J':
			info = w25_JEDEC_ID(SPI1);
			devx = (int)(info & 0xFF)-0x15;	// Offset is 1 higher here
			if ( devx < 3 )
				device = cap[devx];
			else	device = "unknown";
			std_printf("Manufacturer $%02X Type $%02X Capacity $%02X (%s)\n",
				(uint16_t)(info>>16),
				(uint16_t)((info>>8)&0xFF),
				(uint16_t)(info&0xFF),
				device);
			break;
		case 'U':
			w25_read_uid(SPI1,idbuf,sizeof idbuf);
			std_printf("Unique ID: $");
			for ( unsigned ux=0; ux<sizeof idbuf; ++ux )
				std_printf("%02X",idbuf[ux]);
			std_putc('\n');
			break;
		case 'S':
			flash_status();
			break;
		case 'E':
			erase(SPI1,addr);
			break;
		case 'A':
			addr = get_data24("Address");
			std_printf("\nAddress: %06X\n",addr);
			break;
		case 'D':
			addr = dump_page(SPI1,addr);
			std_printf("Address: %06X\n",addr);
			break;
		case 'R':
			addr = w25_read_data(SPI1,addr,(char*)&data,1);
			std_printf("$%06X %02X",addr,data);
			if ( data >= ' ' && data < 0x7F )
				std_printf(" '%c'\n",data);
			else	std_putc('\n');
			break;
		case 'P':
			{
				unsigned a;
				uint16_t d, count = 0u;

				std_printf("$%06X ",addr);
				while ( (d = get_data8(0)) != 0xFFFF ) {
					std_putc(' ');
					data = d & 0xFF;
					a = w25_write_data(SPI1,addr,&data,1);
					if ( a == 0xFFFFFFFF )
						break;
					addr = a;
					++count;
				}
				std_printf("\n$%06X %u bytes written.\n",addr,count);
			}
			break;
		case 'W':
			w25_write_en(SPI1,true);
			flash_status();
			break;
		case 'X':
			w25_write_en(SPI1,false);
			flash_status();
			break;
		case 'H':
			load_ihex(SPI1);
			vTaskDelay(pdMS_TO_TICKS(1500));
			break;
		default:
			std_printf(" ???\n");
			menuf = true;
		}
	}
}

static void
spi_setup(void) {

	rcc_periph_clock_enable(RCC_SPI1);
	gpio_set_mode(
		GPIOA,
                GPIO_MODE_OUTPUT_50_MHZ,
        	GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
                GPIO4|GPIO5|GPIO7		// NSS=PA4,SCK=PA5,MOSI=PA7
	);
	gpio_set_mode(
		GPIOA,
		GPIO_MODE_INPUT,
		GPIO_CNF_INPUT_FLOAT,
		GPIO6				// MISO=PA6
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
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Blue pill

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOC);

	// LED on PC13
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	spi_setup();
	gpio_set(GPIOC,GPIO13);				// PC13 = on

	usb_start(1,1);
	std_set_device(mcu_usb);			// Use USB for std I/O
	gpio_clear(GPIOC,GPIO13);			// PC13 = off

	xTaskCreate(monitor_task,"monitor",500,NULL,1,NULL);
	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End
