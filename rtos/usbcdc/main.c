/* USB CDC Demo, using the usbcdc library
 * Warren W. Gay VE3WWG
 * Wed Mar 15 21:56:50 2017
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>

#include "usbcdc.h"
#include "miniprintf.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

enum Format {
	Binary=0,
	Decimal,
	Hex,
};

struct bdesc {
	unsigned	sbit : 5;
	unsigned	bits : 5;
	enum Format	format : 3;
	unsigned	width : 5;
	const char	*desc;
};

static void
putbin(char *buf,uint32_t v,unsigned bits) {
	char temp[34], *p;

	p = temp + sizeof temp;
	for ( *--p = 0; bits > 0; --bits ) {
		*--p = (v & 1) ? '1' : '0';
		v >>= 1;
	}
	strcpy(buf,p);
}

static void
dump_reg(uint32_t reg,const char *descrip,struct bdesc *desc,int n) {
	char buf[40];
	uint32_t v, mask, shiftr;
	int x, w, tw, b;

	usb_printf("  %10s: $%08X\n",descrip,reg);

	usb_printf("  ");
	for ( x=0; x<n; ++x ) {
		w = desc[x].width;
		tw = strlen(desc[x].desc);

		if ( tw > w )
			b = tw - w;
		else	b = w - tw;

		while ( b-- > 0 )
			usb_putch(' ');

		usb_printf("%s%s",
			desc[x].desc,
			x+1 < n ? "|" : "\n");
	}

	usb_printf("  ");
	for ( x=0; x<n; ++x ) {
		mask = (1 << (31 - desc[x].sbit)) - 1;
		shiftr = desc[x].sbit - desc[x].bits + 1;
		v = (reg >> shiftr) & mask;
		switch ( desc[x].format ) {
		case Binary:
			putbin(buf,v,desc[x].bits);
			break;
		case Decimal:
			mini_snprintf(buf,sizeof buf,"%d",(int)v);
			break;
		case Hex:
			mini_snprintf(buf,sizeof buf,"$%X",(unsigned)v);
			break;
		default:
			buf[0] = '?';
			buf[1] = 0;
		}

		w = desc[x].width;
		tw = strlen(buf);

		if ( tw > w )
			b = tw - w;
		else	b = w - tw;

		while ( b-- > 0 )
			usb_putch(' ');

		usb_printf("%s%c",buf,x+1 < n ? '|' : '\n');
	}
}

static void
dump_rcc(void) {
	static struct bdesc rcc_cr[] = {
		{ 31, 6, Binary, 6, "res" },
		{ 25, 1, Binary, 6, "PLLRDY" },
		{ 24, 1, Binary, 5, "PLLON" },
		{ 23, 4, Binary, 4, "res" },
		{ 19, 1, Binary, 5, "CSSON" },
		{ 18, 1, Binary, 6, "HSEBYP" },
		{ 17, 1, Binary, 6, "HSERDY" },
		{ 16, 1, Binary, 5, "HSEON" },
		{ 15, 8, Hex, 6, "HSICAL" },
		{ 7, 5, Hex, 7, "HSITRIM" },
		{ 2, 1, Binary, 3, "res" },
		{ 1, 1, Binary, 6, "HSIRDY" },
		{ 0, 1, Binary, 5, "HSION" }
	};
	static struct bdesc rcc_cfgr[] = {
		{ 31, 5, Binary, 5, "res" },
		{ 26, 3, Binary, 3, "MCO" },
		{ 23, 1, Binary, 3, "res" },
		{ 22, 1, Binary, 6, "USBPRE" },
		{ 21, 4, Binary, 6, "PLLMUL" },
		{ 17, 1, Binary, 8, "PLLXTPRE" },
		{ 16, 1, Binary, 6, "PLLSRC" },
		{ 15, 2, Binary, 6, "ADCPRE" },
		{ 13, 3, Binary, 5, "PPRE2" },
		{ 10, 3, Binary, 5, "PPRE1" },
		{ 7, 4, Binary, 4, "HPRE" },
		{ 3, 2, Binary, 3, "SWS" },
		{ 1, 2, Binary, 2, "SW" }
	};

	dump_reg(RCC_CR,"RCC_CR",rcc_cr,13);
	dump_reg(RCC_CFGR,"RCC_CFGR",rcc_cfgr,13);
}

/*
 * I/O Task: Here we:
 *
 *	1) Read a character from USB
 *	2) Switch case, if character is alpha
 *	3) Echo character back to USB (and toggle LED)
 */
static void
rxtx_task(void *arg __attribute((unused))) {
	int ch;
	bool menuf = true;
	
	for (;;) {
		if ( menuf )
			usb_printf("\nMenu:\n  r) RCC Registers\n");

		usb_printf("\n: ");
		ch = usb_getch();

		if ( isalpha(ch) )
			ch = toupper(ch);
		usb_printf("%c\n",ch);

		switch ( ch ) {
		case '?':
		case '\r':
		case '\n':
			menuf = true;
			break;		

		case 'R':
			dump_rcc();
			break;

		default:
			usb_printf(" ???\n");
		}
#if 0
		gpio_toggle(GPIOC,GPIO13);
		if ( ch < ' ' && ch != '\r' && ch != '\n' ) {
			usb_printf("^%c (0x%02X)\n",ch+'@',ch);
		} else	{
			if ( ( ch >= 'A' && ch <= 'Z' ) || ( ch >= 'a' && ch <= 'z' ) )
				ch ^= 0x20;
			usb_putch(ch);
		}
#endif
	}
}

/*
 * Main program: Device initialization etc.
 */
int
main(void) {

	SCB_CCR &= ~SCB_CCR_UNALIGN_TRP;		// Make sure alignment is not enabled

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"

	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	xTaskCreate(rxtx_task,"RXTX",200,NULL,configMAX_PRIORITIES-1,NULL);

	usb_start(1);
	gpio_clear(GPIOC,GPIO13);

	vTaskStartScheduler();
	for (;;);
}

/* End main.c */
