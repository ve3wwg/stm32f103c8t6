/* Example of I2C to PCF8574 device
 *
 */
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/timer.h>
// #include <libopencm3/cm3/nvic.h>

#include "usbcdc.h"

#define mainECHO_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )

#define PCF8574_ADDR(n)		(0x20|((n)&7))	// PCF8574
// #define PCF8574_ADDR(n)	(0x38|((n)&7))	// PCF8574A

typedef TickType_t ticktype_t;

static inline void
nap(ticktype_t ticks) {
	vTaskDelay(pdMS_TO_TICKS(ticks));
}

static inline ticktype_t
systicks(void) {
	return xTaskGetTickCount();
}

static ticktype_t
diff_ticks(ticktype_t early,ticktype_t later) {

	if ( later >= early )
		return later - early;
	return ~(ticktype_t)0 - early + 1 + later;
}

static void
program_pulse(void) {

	usb_printf("Pulse..\n");
	gpio_set(GPIOC,GPIO13);
	timer_enable_counter(TIM2);
	vTaskDelay(pdMS_TO_TICKS(200));
	gpio_clear(GPIOC,GPIO13);
	usb_printf("Done pulse..\n");
}

#if 0
static void
dump(const char *msg) {

	usb_printf("%12s SR1: %08x, SR2: %08x, CR1: %08x, CR2: %08x, OAR1: %08X, CCR: %08X, TRISE: %08X\n",
		msg,
		I2C1_SR1,
		I2C1_SR2,
		I2C1_CR1,
		I2C1_CR2,
		I2C1_OAR1,
		I2C1_CCR,
		I2C1_TRISE);
}
#endif

enum I2C_RW {
	Read = 1,
	Write = 0
};

static void
i2c_reset_nconfig(uint32_t i2c) {

	i2c_peripheral_disable(i2c);
	i2c_reset(i2c);
	I2C_CR1(i2c) &= ~I2C_CR1_STOP;		// Clear stop
	i2c_set_standard_mode(i2c);		// 100 kHz mode
	i2c_set_clock_frequency(i2c,I2C_CR2_FREQ_36MHZ); // APB Freq
	i2c_set_trise(i2c,36);			// 1000 ns
	i2c_set_dutycycle(i2c,I2C_CCR_DUTY_DIV2);
	i2c_set_ccr(i2c,180);			// 100 kHz <= 180 * 1 /36M
	i2c_set_own_7bit_slave_address(i2c,0x23); // Necessary?
	i2c_peripheral_enable(i2c);
}

static void
i2c_wait_nbusy(uint32_t i2c) {

	while ( I2C_SR2(i2c) & I2C_SR2_BUSY )
		taskYIELD();			// I2C Busy

}

static bool
i2c_start_naddrrw(uint32_t i2c,uint8_t addr,enum I2C_RW rw) {
	ticktype_t t0 = systicks();
	ticktype_t t1;
	uint32_t reg32 __attribute__((unused));

	I2C_SR1(i2c) &= ~I2C_SR1_AF;	// Clear NAK
	I2C_CR1(i2c) &= ~I2C_CR1_STOP;	// Clear stop
	i2c_send_start(i2c);		// SB=1

	// Loop while !SB || !MSL || BUSY
	while ( !(I2C_SR1(i2c) & I2C_SR1_SB) || !(I2C_SR2(i2c) & I2C_SR2_MSL) ) {
		t1 = systicks();
		if ( diff_ticks(t0,t1) > 500 ) {
			i2c_reset_nconfig(i2c);
			return false;
		} else	taskYIELD();
	}

	// Send Address & R/W flag:
	i2c_send_7bit_address(i2c,addr,rw == Read ? I2C_READ : I2C_WRITE);

	while ( !(I2C_SR1(i2c) & I2C_SR1_ADDR) ) {
		if ( I2C_SR1(i2c) & I2C_SR1_AF ) {
			i2c_send_stop(i2c);
			reg32 = I2C_SR1(i2c);
			reg32 = I2C_SR2(i2c); 	// Clear flags
			return false;		// NAK Received (no ADDR flag will be set here)
		}
		taskYIELD();
	}

	reg32 = I2C_SR1(i2c);
	reg32 = I2C_SR2(i2c); 			// Clear flags

	return !(I2C_SR1(i2c) & I2C_SR1_AF );
}

static bool
i2c_write_nwait(uint32_t i2c,uint8_t byte) {
	ticktype_t t0 = systicks();

	i2c_send_data(i2c,byte);
	while ( !(I2C_SR1(i2c) & I2C_SR1_TxE) ) {
		taskYIELD();
		if ( diff_ticks(t0,systicks()) > 500 )
			return false;		// Hung write
	}
	return !(I2C_SR1(i2c) & I2C_SR1_AF);
}

static bool
i2c_read_nwait(uint32_t i2c,uint8_t *byte) {
	ticktype_t t0 = systicks();

	while ( !(I2C_SR1(i2c) & I2C_SR1_RxNE) ) {
		if ( diff_ticks(t0,systicks()) > 500 )
			return false;		// Hung read
		taskYIELD();
	}
	
	*byte = I2C_DR(i2c);
	return !(I2C_SR1(i2c) & I2C_SR1_AF);
}

static void
fail(const char *msg) {
	usb_printf("FAIL!! %s.\n",msg);
	for(;;);
}

static void
task1(void *args) {
	uint8_t addr = PCF8574_ADDR(0);
	bool ack;
//	char first;
	uint16_t eaddr = 0u;
	uint8_t byte;

	vTaskDelay(pdMS_TO_TICKS(5000));
	usb_printf("Task1 begun:\nPress CR to begin:");
//	first = usb_getc();
	usb_getc();
	usb_putc('\n');
	usb_putc('\n');

	(void)args;
	vTaskDelay(pdMS_TO_TICKS(1000));

	ack = i2c_start_naddrrw(I2C1,addr+2,Write);
	if ( !ack ) fail("1st start");
	ack = i2c_write_nwait(I2C1,0xFF);
	if ( !ack ) fail("1st write 0xFF");
	i2c_send_stop(I2C1);

	for (;;) {
		program_pulse();

		i2c_wait_nbusy(I2C1);

		do	{
			ack = i2c_start_naddrrw(I2C1,addr,Write);
			if ( !ack ) fail("Start");
			ack = i2c_write_nwait(I2C1,byte=eaddr&0x0FF);
			if ( !ack ) fail("Lo Addr");

			ack = i2c_start_naddrrw(I2C1,addr+1,Write);
			if ( !ack ) fail("St Lo Addr");
			ack = i2c_write_nwait(I2C1,(eaddr>>8)&0x1F);

			vTaskDelay(2);

			ack = i2c_start_naddrrw(I2C1,addr+2,Read);
			if ( !ack ) fail("St Read");
			ack = i2c_read_nwait(I2C1,&byte);
			if ( !ack ) fail("Da Read");

		} while ( !ack );

		i2c_send_stop(I2C1);
		++eaddr;

		usb_printf("%04X  %02X '%c'\n",
			eaddr,byte,
			byte >= ' ' && byte < 0x7F ? byte : '.');

		if ( eaddr >= 0xFFFF )
			fail("End");

//		if ( first != 'F' )
//			vTaskDelay(pdMS_TO_TICKS(200));
//		gpio_toggle(GPIOC,GPIO13);
	}
}

static void
timer_setup(void) {

	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_reset_pulse(RST_TIM2);
	timer_set_mode(TIM2,TIM_CR1_CKD_CK_INT,TIM_CR1_CMS_EDGE,TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM2,rcc_apb1_frequency /* * 2*/ / 72000u);	// 1000 Hz
	timer_disable_preload(TIM2);
	// timer_continuous_mode(TIM2);
	timer_set_period(TIM2,100);		// 100 ms
	timer_set_oc_mode(TIM2,TIM_OCM_ACTIVE,TIM_OC1);
	timer_enable_oc_output(TIM2,TIM_OC1);
	timer_set_oc_value(TIM2,TIM_OC1,3);
//	timer_enable_counter(TIM2);
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);		// I2C
	rcc_periph_clock_enable(RCC_GPIOC);		// LED
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_I2C1);

	gpio_set_mode(GPIOB,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,GPIO6|GPIO7);	// I2C
	gpio_set(GPIOB,GPIO6|GPIO7);

	gpio_set_mode(GPIOA,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO0);			// Programming pulse
	gpio_clear(GPIOA,GPIO0);

	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);			// LED
			     
	gpio_primary_remap(0,0);		// AFIO_MAPR_I2C1_REMAP=0, PB6+PB7
	i2c_reset_nconfig(I2C1);

	timer_setup();

	xTaskCreate(task1,"I2C",800,NULL,1,NULL);
	usb_start(1,1);

	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End 
