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

#define mainECHO_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )

#define PCF8574_ADDR(n)		(0x20|((n)&7))	// PCF8574
// #define PCF8574_ADDR(n)	(0x38|((n)&7))	// PCF8574A

static void
led(int on) {

	if ( on )
		gpio_clear(GPIOC,GPIO13);
	else	gpio_set(GPIOC,GPIO13);
}

#if 0
/* PECERR: PEC Error in reception */
#define I2C_SR1_PECERR                  (1 << 12)

/* OVR: Overrun/Underrun */
#define I2C_SR1_OVR                     (1 << 11)

/* AF: Acknowledge failure */
#define I2C_SR1_AF                      (1 << 10)

/* ARLO: Arbitration lost (master mode) */
#define I2C_SR1_ARLO                    (1 << 9)

/* BERR: Bus error */
#define I2C_SR1_BERR                    (1 << 8)

/* TxE: Data register empty (transmitters) */
#define I2C_SR1_TxE                     (1 << 7)

/* RxNE: Data register not empty (receivers) */
#define I2C_SR1_RxNE                    (1 << 6)

/* Note: Bit 5 is reserved, and forced to 0 by hardware. */

/* STOPF: STOP detection (slave mode) */
#define I2C_SR1_STOPF                   (1 << 4)

/* ADD10: 10-bit header sent (master mode) */
#define I2C_SR1_ADD10                   (1 << 3)

/* BTF: Byte transfer finished */
#define I2C_SR1_BTF                     (1 << 2)

/* ADDR: Address sent (master mode) / address matched (slave mode) */
#define I2C_SR1_ADDR                    (1 << 1)

/* SB: Start bit (master mode) */
#define I2C_SR1_SB                      (1 << 0)
#endif

static void
i2c_start_nwait(uint32_t i2c) {

	i2c_send_start(i2c);

	// Wait for start & master mode
	while (!((I2C_SR1(i2c) & I2C_SR1_SB) & (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))))
		taskYIELD();
}

static void
i2c_address_nwait(uint32_t i2c,uint8_t addr) {
	uint32_t reg32 __attribute__((unused));

	i2c_send_7bit_address(i2c,addr,I2C_WRITE);
	while (!(I2C_SR1(i2c) & I2C_SR1_ADDR))
		taskYIELD();
	reg32 = I2C_SR2(i2c); 	// Clear flag
}
	
static void
i2c_write_nwait(uint32_t i2c,uint8_t byte) {

	i2c_send_data(i2c,byte);
	while (!(I2C_SR1(i2c) & (I2C_SR1_BTF | I2C_SR1_TxE)))
		taskYIELD();
}

#if 0
static uint8_t
i2c_read_nwait(uint32_t i2c) {

	I2C_CR1(i2c) &= ~I2C_CR1_ACK;
	while (!(I2C_SR1(i2c) & I2C_SR1_BTF))
		taskYIELD();
	return I2C_DR(i2c);
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

	(void)args;

	for (;;) {
		led(1);				// LED on
		i2c_start_nwait(I2C1);
		i2c_address_nwait(I2C1,PCF8574_ADDR(0));
		i2c_write_nwait(I2C1,pat++);
		i2c_send_stop(I2C1);

		vTaskDelay(pdMS_TO_TICKS(500));
		led(0);				// LED off
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

int
main(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"

	rcc_periph_clock_enable(RCC_GPIOB);		// I2C
	rcc_periph_clock_enable(RCC_GPIOC);		// LED

	gpio_set_mode(GPIOB,GPIO_MODE_OUTPUT_50_MHZ,GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,GPIO6|GPIO7);	// I2C
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);			// LED

	rcc_periph_clock_enable(RCC_I2C1);

	i2c_reset(I2C1);
	i2c_peripheral_disable(I2C1);
	gpio_primary_remap(0,0);		// AFIO_MAPR_I2C1_REMAP=0, PB6+PB7

	i2c_set_standard_mode(I2C1);		// 100 kHz mode
	i2c_set_clock_frequency(I2C1,I2C_CR2_FREQ_8MHZ); // APB Freq
	i2c_set_ccr(I2C1,0x28);			// 100 kHz
	i2c_set_trise(I2C1,9);			// 5000 ns
	i2c_set_dutycycle(I2C1,I2C_CCR_DUTY_DIV2);

	i2c_peripheral_enable(I2C1);

	xTaskCreate(task1,"I2C",200,NULL,configMAX_PRIORITIES-1,NULL);
	vTaskStartScheduler();
	for (;;)
		;
	return 0;
}

// End 
