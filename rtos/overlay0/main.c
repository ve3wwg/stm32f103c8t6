/* Demonstration of overlays: proof of concept
 * Warren Gay ve3wwg@gmail.com
 * Sat Oct 28 20:32:50 2017
 *
 * This program simply demonstrates the mechanics of loading and
 * calling an overlay. All code for this demo still resides in
 * the stm32 flash. This is a convenient way to test the
 * overlay manager.
 *
 * Communication is via USB to minicom.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#include "mcuio.h"
#include "miniprintf.h"
#include "winbond.h"

/*********************************************************************
 * Overlayed functions
 *********************************************************************/

int fee(int arg) __attribute__((noinline,section(".ov_fee")));
int fie(int arg) __attribute__((noinline,section(".ov_fie")));
int foo(int arg) __attribute__((noinline,section(".ov_foo")));
int fum(int arg) __attribute__((noinline,section(".ov_fum")));

/*********************************************************************
 * Overlay Table and macros
 *********************************************************************/

typedef struct {
	short		regionx;// Overlay region index
	void		*vma;	// Overlay's mapped address
	char		*start;	// Load start address
	char		*stop;	// Load stop address
	unsigned long	size;	// Size in bytes
	void		*func;	// Function pointer
} s_overlay;

#define N_REGIONS	1	// # of overlay regions in RAM
#define N_OVLY		4	// Total # of overlays (in all regions)

#define OVERLAY(region,ov,sym)	{ region, &ov, &__load_start_ ## sym, &__load_stop_ ## sym, 0, sym }
#define LOADREF(sym) __load_start_ ## sym, __load_stop_ ## sym 

extern unsigned long overlay1;	// Provides address of overlay region 1

// Function load addresses
extern char LOADREF(fee), LOADREF(fie), LOADREF(foo), LOADREF(fum);

// Overlay table:
static s_overlay overlays[N_OVLY] = {
	OVERLAY(0,overlay1,fee),
	OVERLAY(0,overlay1,fie),
	OVERLAY(0,overlay1,foo),
	OVERLAY(0,overlay1,fum)
};

// Overlay cache:
static s_overlay *cur_overlay[N_REGIONS] = { 0 };

/*********************************************************************
 * Overlay lookup: Returns func ptr, after copying code if necessary
 *********************************************************************/

static void *
module_lookup(void *module) {
	unsigned regionx;
	s_overlay *ovl = 0;

	std_printf("module_lookup(%p):\n",module);

	for ( unsigned ux=0; ux<N_OVLY; ++ux ) {
		if ( overlays[ux].start == module ) {
			regionx = overlays[ux].regionx;
			ovl = &overlays[ux];
			break;
		}
	}

	if ( !ovl )
		return 0;		// Not found

	if ( !cur_overlay[regionx] || cur_overlay[regionx] != ovl ) {
		if ( ovl->size == 0 )
			ovl->size = (char *)ovl->stop - (char *)ovl->start;
		cur_overlay[regionx] = ovl;
		memcpy(ovl->vma,ovl->start,ovl->size);
	}
	return ovl->func;
}

/*********************************************************************
 * Overlay function fee()
 *********************************************************************/

int 
fee(int arg) {
	static const char format[] 
		__attribute__((section(".ov_fee_static")))
		= "***********\n"
		  "fee(0x%04X)\n"
		  "***********\n";

	std_printf(format,arg);
	return arg + 0x0001;
}

/*********************************************************************
 * Overlay function fie()
 *********************************************************************/

int 
fie(int arg) {
	std_printf("fie(0x%04X)\n",arg);
	return arg + 0x0010;
}

/*********************************************************************
 * Overlay function foo()
 *********************************************************************/

int 
foo(int arg) {
	std_printf("foo(0x%04X)\n",arg);
	return arg + 0x0200;
}

/*********************************************************************
 * Overlay function fum()
 *********************************************************************/

int 
fum(int arg) {
	std_printf("fum(0x%04X)\n",arg);
	return arg + 0x3000;
}

/*********************************************************************
 * Stub functions for calling the overlay functions:
 *********************************************************************/

static int
fee_stub(int arg) {
	int (*feep)(int arg) = module_lookup(&__load_start_fee);

	return feep(arg);
}

static int
fie_stub(int arg) {
	int (*fiep)(int arg) = module_lookup(&__load_start_fie);

	return fiep(arg);
}

static int
foo_stub(int arg) {
	int (*foop)(int arg) = module_lookup(&__load_start_foo);

	return foop(arg);
}

static int
fum_stub(int arg) {
	int (*fump)(int arg) = module_lookup(&__load_start_fum);

	return fump(arg);
}

/*********************************************************************
 * Launch a bunch of overlay calls and return the result:
 *********************************************************************/

static int 
fang(int arg) {

	std_printf("fang(0x%04X)\n",arg);
	arg = fee_stub(arg);
	arg = fie_stub(arg);
	arg = foo_stub(arg);
	return fum_stub(arg);
}

/*********************************************************************
 * Main interactive task (USB):
 *********************************************************************/

static void
task1(void *args __attribute((unused))) {
	int r;

	for (;;) {
		// Wait until user is ready:
		do	{
			std_printf("Enter R when ready: \n");
			r = std_getc();
		} while ( r != 'R' && r != 'r' );

		// Dump the overlay table:
		std_printf("OVERLAY TABLE:\n");
		for ( unsigned ux=0; ux<N_OVLY; ++ux ) {
			std_printf("[%u] { regionx=%u, vma=%p, start=%p, stop=%p, size=%u, func=%p }\n",
				ux, 
				overlays[ux].regionx,
				overlays[ux].vma,
				overlays[ux].start,
				overlays[ux].stop,
				(unsigned)overlays[ux].size,
				overlays[ux].func);
		}

		gpio_toggle(GPIOC,GPIO13);	// Toggle LED

		r = fang(0x0001);		// Exercise overlays
		std_printf("fang(0xA) returned 0x%04X\n",r);
	}
}

/*********************************************************************
 * Main program
 *********************************************************************/

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);

	usb_start(1,1);
	std_set_device(mcu_usb);			// Use USB for std I/O

	w25_spi_setup(SPI1,true,true,true,SPI_CR1_BAUDRATE_FPCLK_DIV_128);

	xTaskCreate(task1,"task1",100,NULL,1,NULL);
	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End
