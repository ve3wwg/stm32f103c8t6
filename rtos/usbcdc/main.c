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
	int		width : 6;
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

static int
dump_hdrs(int x,const struct bdesc *desc,int n) {
	int w, tw, b;
	bool firstf = !x;

	usb_printf("  ");
	for ( ; x<n; ++x ) {
		w = desc[x].width;
		tw = strlen(desc[x].desc);

		if ( firstf && w < 0 ) {
			usb_printf("|\n");
			return x;
		} else if ( w < 0 )
			w = -w;

		if ( tw > w )
			b = tw - w;
		else	b = w - tw;

		while ( b-- > 0 )
			usb_putch(' ');

		usb_printf("%s%s",
			desc[x].desc,
			x+1 < n ? "|" : "\n");
	}
	return 0;
}

static void
dump_vals(int x,uint32_t reg,const struct bdesc *desc,int n) {
	char buf[40];
	uint32_t v, mask, shiftr;
	int w, tw, b;
	bool firstf = !x;

	usb_printf("  ");
	for ( ; x<n; ++x ) {
		w = desc[x].width;
		if ( firstf && w < 0 ) {
			usb_printf("|\n");
			break;
		} else if ( w < 0 )
			w = -w;

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
dump_reg(volatile uint32_t *raddr,const char *descrip,const struct bdesc *desc,int n) {
	uint32_t reg = *raddr;
	int x, x2;

	usb_printf("\n%-12s: $%08X @ $%08X\n",descrip,reg,(uint32_t)raddr);

	x = dump_hdrs(0,desc,n);
	dump_vals(0,reg,desc,n);

	for ( ; x > 0; x = x2 ) {
		x2 = dump_hdrs(x,desc,n);
		dump_vals(x,reg,desc,n);
	}

#if 0
	usb_printf("  ");
	for ( x=0; x<n; ++x ) {
		if ( first && desc[x].width < 0 )
			break;
		if ( !first && desc[x].width < 0 ) {
		}

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
#endif
}

static void
dump_rcc(void) {
	static const struct bdesc rcc_cr[] = {
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
	static const struct bdesc rcc_cfgr[] = {
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
	static const struct bdesc rcc_cir[] = {
		{ 31, 8, Binary, 8, "res" },
		{ 23, 1, Binary, 4, "CSSC" },
		{ 22, 2, Binary, 3, "res" },
		{ 20, 1, Binary, 7, "PLLRDYC" },
		{ 19, 1, Binary, 7, "HSERDYC" },
		{ 18, 1, Binary, 7, "HSIRDYC" },
		{ 17, 1, Binary, 7, "LSERDYC" },
		{ 16, 1, Binary, 7, "LSIRDYC" },
		{ 15, 3, Binary, -3, "res" },
		{ 12, 1, Binary, 8, "PLLRDYIE" },
		{ 11, 1, Binary, 8, "HSERDYIE" },
		{ 10, 1, Binary, 8, "HSIRDYIE" },
		{ 9, 1, Binary, 8, "LSERDYIE" },
		{ 8, 1, Binary, 8, "LSIRDYIE" },
		{ 7, 1, Binary, 4, "CSSF" },
		{ 6, 2, Binary, 3, "res" },
		{ 4, 1, Binary, 7, "PLLRDYF" },
		{ 3, 1, Binary, 7, "HSERDYF" },
		{ 2, 1, Binary, 7, "HSIRDYF" },
		{ 1, 1, Binary, 7, "LSERDYF" },
		{ 0, 1, Binary, 7, "LSIRDYF" },
	};
	static const struct bdesc rcc_apb2rstr[] = {
		{ 31, 10, Binary, 10, "res" },
		{ 21, 1, Binary, 8, "TIM11RST" },
		{ 20, 1, Binary, 8, "TIM10RST" },
		{ 19, 1, Binary, 7, "TIM9RST" },
		{ 18, 3, Binary, 3, "res" },
		{ 15, 1, Binary, -7, "ADC3RST" },
		{ 14, 1, Binary, 9, "USART1RST" },
		{ 13, 1, Binary, 7, "TIM8RST" },
		{ 12, 1, Binary, 7, "SPI1RST" },
		{ 11, 1, Binary, 7, "TIM1RST" },
		{ 10, 1, Binary, 7, "ADC2RST" },
		{ 9, 1, Binary, 7, "ADC1RST" },
		{ 8, 1, Binary, 7, "IOPGRST" },
		{ 7, 1, Binary, 7, "IOPFRST" },
		{ 6, 1, Binary, 7, "IOPERST" },
		{ 5, 1, Binary, 7, "IOPDRST" },
		{ 4, 1, Binary, 7, "IOPCRST" },
		{ 3, 1, Binary, 7, "IOPBRST" },
		{ 2, 1, Binary, 7, "IOPARST" },
		{ 1, 1, Binary, 3, "res" },
		{ 0, 1, Binary, 7, "AFIORST" }
	};
	static const struct bdesc rcc_ahbenr[] = {
		{ 31, 21, Binary, 21, "res" },
		{ 10, 1, Binary, 6, "SDIOEN" },
		{ 9, 1, Binary, 3, "res" },
		{ 8, 1, Binary, 6, "FSMCEN" },
		{ 7, 1, Binary, 3, "res" },
		{ 6, 1, Binary, 5, "CRCEN" },
		{ 5, 1, Binary, 3, "res" },
		{ 4, 1, Binary, 7, "FLITFEN" },
		{ 3, 1, Binary, 3, "res" },
		{ 2, 1, Binary, 6, "SRAMEN" },
		{ 1, 1, Binary, 6, "DMA2EN" },
		{ 0, 1, Binary, 6, "DMA1EN" }
	};
	static const struct bdesc rcc_apb2enr[] = {
		{ 31, 10, Binary, 10, "res" },
		{ 21, 1, Binary,  7, "TIM11EN" },
		{ 20, 1, Binary,  7, "TIM10EN" },
		{ 19, 1, Binary,  6, "TIM9EN" },
		{ 18, 3, Binary,  3, "res" },
		{ 15, 1, Binary,  -6, "ADC3EN" },
		{ 14, 1, Binary,  8, "USART1EN" },
		{ 13, 1, Binary,  6, "TIM8EN" },
		{ 12, 1, Binary,  6, "SPI1EN" },
		{ 11, 1, Binary,  6, "TIM1EN" },
		{ 10, 1, Binary,  6, "ADC2EN" },
		{  9, 1, Binary,  6, "ADC1EN" },
		{  8, 1, Binary,  6, "IOPGEN" },
		{  7, 1, Binary,  6, "IOPFEN" },
		{  6, 1, Binary,  6, "IOPEEN" },
		{  5, 1, Binary,  6, "IOPDEN" },
		{  4, 1, Binary,  6, "IOPCEN" },
		{  3, 1, Binary,  6, "IOPBEN" },
		{  2, 1, Binary,  6, "IOPAEN" },
		{  1, 1, Binary,  3, "res" },
		{  0, 1, Binary,  6, "AFIOEN" }
	};
	static const struct bdesc rcc_apb1enr[] = {
		{ 31, 2, Binary, 3, "res" },
		{ 29, 1, Binary,  5, "DACEN" },
		{ 28, 1, Binary,  5, "PWREN" },
		{ 27, 1, Binary,  5, "BCKEN" },
		{ 26, 1, Binary,  3, "res" },
		{ 25, 1, Binary,  5, "CANEN" },
		{ 24, 1, Binary,  3, "res" },
		{ 23, 1, Binary,  5, "USBEN" },
		{ 22, 1, Binary,  6, "I2C2EN" },
		{ 21, 1, Binary,  6, "I2C1EN" },
		{ 20, 1, Binary,  7, "UART5EN" },
		{ 19, 1, Binary,  7, "UART4EN" },
		{ 18, 1, Binary,  8, "USART3EN" },
		{ 17, 1, Binary,  8, "USART2EN" },
		{ 16, 1, Binary,  -3, "res" },
		{ 15, 1, Binary,  6, "SPI3EN" },
		{ 14, 1, Binary,  6, "SPI2EN" },
		{ 13, 2, Binary,  3, "res" },
		{ 11, 1, Binary,  6, "WWDGEN" },
		{ 10, 2, Binary,  3, "res" },
		{  8, 1, Binary,  7, "TIM14EN" },
		{  7, 1, Binary,  7, "TIM13EN" },
		{  6, 1, Binary,  7, "TIM12EN" },
		{  5, 1, Binary,  6, "TIM7EN" },
		{  4, 1, Binary,  6, "TIM6EN" },
		{  3, 1, Binary,  6, "TIM5EN" },
		{  2, 1, Binary,  6, "TIM4EN" },
		{  1, 1, Binary,  6, "TIM3EN" },
		{  0, 1, Binary,  6, "TIM2EN" },
	};
	static const struct bdesc rcc_bdcr[] = {
		{ 31, 15, Binary, 15, "res" },
		{ 16,  1, Binary,  5, "BD$ST" },
		{ 15,  1, Binary,  5, "res" },
		{  9,  2, Binary,  6, "RTCSEL" },
		{  7,  5, Binary,  5, "res" },
		{  2,  1, Binary,  6, "LSEBYP" },
		{  1,  1, Binary,  6, "LSERDY" },
		{  0,  1, Binary,  5, "LSEON" },
	};
	static const struct bdesc rcc_csr[] = {
		{ 31,  1, Binary,  8, "LPWRRSTF" },
		{ 30,  1, Binary,  8, "WWDGRSTF" },
		{ 29,  1, Binary,  8, "IWDGRSTF" },
		{ 28,  1, Binary,  7, "SFTRSTF" },
		{ 27,  1, Binary,  7, "PORRSTF" },
		{ 26,  1, Binary,  7, "PINRSTF" },
		{ 25,  1, Binary,  3, "res" },
		{ 24,  1, Binary,  4, "RMVF" },
		{ 23,  8, Binary,  8, "res" },
		{ 15, 14, Binary, 14, "res" },
		{  1,  1, Binary,  6, "LSIRDY" },
		{  0,  1, Binary,  5, "LSION" },
	};

	dump_reg(&RCC_CR,"RCC_CR",rcc_cr,13);
	dump_reg(&RCC_CFGR,"RCC_CFGR",rcc_cfgr,13);
	dump_reg(&RCC_CIR,"RCC_CIR",rcc_cir,21);
	dump_reg(&RCC_APB2RSTR,"RCC_APB2RSTR",rcc_apb2rstr,21);
	dump_reg(&RCC_AHBENR,"RCC_AHBENR",rcc_ahbenr,12);
	dump_reg(&RCC_APB2ENR,"RCC_APB2ENR",rcc_apb2enr,21);
	dump_reg(&RCC_APB1ENR,"RCC_APB1ENR",rcc_apb1enr,29);
	dump_reg(&RCC_BDCR,"RCC_BDCR",rcc_bdcr,8);
	dump_reg(&RCC_CSR,"RCC_CSR",rcc_csr,12);
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
