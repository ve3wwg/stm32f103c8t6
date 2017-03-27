/* Example of I2C to PCF8574 device
 *
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
// #include <libopencm3/cm3/nvic.h>

#include "usbcdc.h"

#define mainECHO_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )

#define PCF8574_ADDR(n)		(0x20|((n)&7))	// PCF8574
// #define PCF8574_ADDR(n)	(0x38|((n)&7))	// PCF8574A

static void
led(int on) {

	if ( on )
		gpio_clear(GPIOC,GPIO13);
	else	gpio_set(GPIOC,GPIO13);
}

static void
led2(int on) {

	if ( on )
		gpio_clear(GPIOB,GPIO9);
	else	gpio_set(GPIOB,GPIO9);
}

static void
led3(int on) {

	if ( on )
		gpio_clear(GPIOB,GPIO8);
	else	gpio_set(GPIOB,GPIO8);
}

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

static void
fail(const char *msg) {

	dump(msg);
	for (;;)
		taskYIELD();
}

static void
i2c_reset_nconfig(uint32_t i2c) {

	i2c_reset(i2c);
	i2c_peripheral_disable(i2c);
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

static void
i2c_start_nwait(uint32_t i2c) {
	ticktype_t t0 = systicks();
	ticktype_t t1;

	I2C_SR1(i2c) &= ~I2C_SR1_AF;	// Clear NAK

	i2c_send_start(i2c);
//	dump("Start");

	// Loop while !SB || !MSL || BUSY
	while ( !(I2C_SR1(i2c) & I2C_SR1_SB) || !(I2C_SR2(i2c) & I2C_SR2_MSL) ) {
		t1 = systicks();
		if ( diff_ticks(t0,t1) > 2000 ) {
			dump("Timeout");
			i2c_reset_nconfig(i2c);
			dump("AfterReset");
			usb_putch('\n');
			t0 = systicks();
			i2c_send_start(i2c);
			dump("ReStart");
		} else	{
			taskYIELD();
		}
	}
//	dump("Started!");
}

static int
i2c_address_nwait(uint32_t i2c,uint8_t addr) {
	uint32_t reg32 __attribute__((unused));

//	dump("Bef Address");

//	usb_printf("Writing address 0x%02X + Write (0x%02X)\n",addr,(addr<<1)|I2C_WRITE);

	i2c_send_7bit_address(i2c,addr,I2C_WRITE);

	while ( !(I2C_SR1(i2c) & I2C_SR1_ADDR) ) {
		if ( I2C_SR1(i2c) & I2C_SR1_AF )
			return -1;		// NAK
		taskYIELD();
	}

//	dump("Addr sent");

	reg32 = I2C_SR1(i2c);
	reg32 = I2C_SR2(i2c); 	// Clear flag

//	dump("Wtg SR1_ADDR");

	if ( I2C_SR1(i2c) & I2C_SR1_ADDR )
		fail("ADDR");

//	usb_printf("Wrote address 0x%02X + Write\n",addr);

	if ( I2C_SR1(i2c) & I2C_SR1_AF )
		fail("AF: NAK");
	return 0;
}
	
static void
i2c_write_nwait(uint32_t i2c,uint8_t byte) {
	ticktype_t t0 = systicks(), tn;

	i2c_send_data(i2c,byte);
	while ( !(I2C_SR1(i2c) & I2C_SR1_TxE) ) {
		taskYIELD();
		if ( diff_ticks(t0,tn = systicks()) > 2000 ) {
			dump("HungWrite");
			t0 = tn;
		}
	}
	usb_printf("Wrote byte 0x%02X\n",byte);
//	dump("AftWrite");
}

#if 0
static uint8_t
i2c_read_nwait(uint32_t i2c) {
	uint8_t b;

	I2C_CR1(i2c) &= ~I2C_CR1_ACK;
	while (!(I2C_SR1(i2c) & I2C_SR1_BTF))
		taskYIELD();
	
	b = I2C_DR(i2c);
	usb_printf("Read byte 0x%02X\n",b);
	return b;
}
#endif

#if 0
uint16_t stts75_read_temperature(uint32_t i2c, uint8_t sensor)
{
	uint32_t reg32 __attribute__((unused));
	uint16_t temperature;

	/* Send START condition. */
	i2c_send_start(i2c);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(i2c) & I2C_SR1_SB)
		& (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	/* Yes, WRITE is correct - for selecting register in STTS75. */
	i2c_send_7bit_address(i2c, sensor, I2C_WRITE);

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(i2c) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(i2c);

	i2c_send_data(i2c, 0x0); /* temperature register */
	while (!(I2C_SR1(i2c) & (I2C_SR1_BTF | I2C_SR1_TxE)));

	/*
	 * Now we transferred that we want to ACCESS the temperature register.
	 * Now we send another START condition (repeated START) and then
	 * transfer the destination but with flag READ.
	 */

	/* Send START condition. */
	i2c_send_start(i2c);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(i2c) & I2C_SR1_SB)
		& (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	i2c_send_7bit_address(i2c, sensor, I2C_READ); 

	/* 2-byte receive is a special case. See datasheet POS bit. */
	I2C_CR1(i2c) |= (I2C_CR1_POS | I2C_CR1_ACK);

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(i2c) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(i2c);

	/* Cleaning I2C_SR1_ACK. */
	I2C_CR1(i2c) &= ~I2C_CR1_ACK;

	/* Now the slave should begin to send us the first byte. Await BTF. */
	while (!(I2C_SR1(i2c) & I2C_SR1_BTF));
	temperature = (uint16_t)(I2C_DR(i2c) << 8); /* MSB */

	/*
	 * Yes they mean it: we have to generate the STOP condition before
	 * saving the 1st byte.
	 */
	I2C_CR1(i2c) |= I2C_CR1_STOP;

	temperature |= I2C_DR(i2c); /* LSB */

	/* Original state. */
	I2C_CR1(i2c) &= ~I2C_CR1_POS;

	return temperature;
}
#endif

static void
task1(void *args) {
	int pat = 0;

	vTaskDelay(pdMS_TO_TICKS(5000));
	usb_printf("Task1 begun:\nPress CR to begin:");
	usb_getch();
	usb_putch('\n');
	usb_putch('\n');
	dump("Config");

	(void)args;
	vTaskDelay(pdMS_TO_TICKS(1000));
	led(0);
	led2(0);
	led3(0);

	for (;;) {
		led(1);
		i2c_wait_nbusy(I2C1);

		usb_printf("i2c_start_nwait(I2C1)\n");
		i2c_start_nwait(I2C1);
		if ( !i2c_address_nwait(I2C1,PCF8574_ADDR(0)) ) {
			i2c_write_nwait(I2C1,pat++);
		} else	{
			usb_printf("*** NAK ***\n");
		}
		i2c_send_stop(I2C1);
		usb_putch('\n');

		vTaskDelay(pdMS_TO_TICKS(200));
		led(0);				// LED off
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"

	rcc_periph_clock_enable(RCC_GPIOB);		// I2C
	rcc_periph_clock_enable(RCC_GPIOC);		// LED
	rcc_periph_clock_enable(RCC_APB1ENR);
	rcc_periph_clock_enable(RCC_APB2ENR);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_CRC);
	rcc_periph_clock_enable(RCC_I2C1);

	gpio_set_mode(GPIOB,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,GPIO6|GPIO7);	// I2C
	gpio_set_mode(GPIOB,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO9|GPIO8);		// 2nd & 3rd LEDs
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);			// LED
			     
#if 0
	RCC_APB1RSTR |= RCC_APB1RSTR_I2C1RST;
//	rcc_peripheral_reset(RCC_APB1RSTR,RCC_APB1RSTR_I2C1RST);
	for ( volatile int x=0; x<1000; ++x);	// Delay
	RCC_APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
//	rcc_peripheral_clear(RCC_APB1RSTR,RCC_APB1RSTR_I2C1RST);
#endif

	gpio_primary_remap(0,0);		// AFIO_MAPR_I2C1_REMAP=0, PB6+PB7

	i2c_reset_nconfig(I2C1);

	led(0);
	led2(0);
	led3(0);

	xTaskCreate(task1,"I2C",800,NULL,configMAX_PRIORITIES-1,NULL);
	usb_start(1);

	vTaskStartScheduler();
	for (;;);
	return 0;
}

// End 
