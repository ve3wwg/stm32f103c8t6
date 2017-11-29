/* A stm32f103 application I2C library
 * Warren W. Gay VE3WWG
 * Sat Nov 25 11:56:51 2017
 *
 * Notes:
 *	1. Master I2C mode only
 *	2. No interrupts are used
 *	3. ReSTART I2C is not supported
 *	4. Uses PB6=SCL, PB7=SDA
 *	5. Requires GPIOB clock enabled
 *	6. PB6+PB7 must be GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN
 *	7. Requires rcc_periph_clock_enable(RCC_I2C1);
 *	8. Requires rcc_periph_clock_enable(RCC_AFIO);
 *	9. 100 kHz
 */
#include "FreeRTOS.h"
#include "task.h"
#include "i2c.h"

#define systicks	xTaskGetTickCount

static const char *i2c_msg[] = {
	"OK",
	"Address timeout",
	"Address NAK",
	"Write timeout",
	"Read timeout"
};

jmp_buf i2c_exception;

/*********************************************************************
 * Compute the difference in ticks:
 *********************************************************************/

static inline TickType_t
diff_ticks(TickType_t early,TickType_t later) {

	if ( later >= early )
		return later - early;
	return ~(TickType_t)0 - early + 1 + later;
}

/*********************************************************************
 * Return a character string message for I2C_Fails code
 *********************************************************************/

const char *
i2c_error(I2C_Fails fcode) {
	int icode = (int)fcode;

	if ( icode < 0 || icode > (int)I2C_Read_Timeout )
		return "Bad I2C_Fails code";
	return i2c_msg[icode];
}

/*********************************************************************
 * Configure I2C device for 100 kHz, 7-bit addresses
 *********************************************************************/

void
i2c_configure(I2C_Control *dev,uint32_t i2c,uint32_t ticks) {

	dev->device = i2c;
	dev->timeout = ticks;

	i2c_peripheral_disable(dev->device);
	i2c_reset(dev->device);
	I2C_CR1(dev->device) &= ~I2C_CR1_STOP;	// Clear stop
	i2c_set_standard_mode(dev->device);	// 100 kHz mode
	i2c_set_clock_frequency(dev->device,I2C_CR2_FREQ_36MHZ); // APB Freq
	i2c_set_trise(dev->device,36);		// 1000 ns
	i2c_set_dutycycle(dev->device,I2C_CCR_DUTY_DIV2);
	i2c_set_ccr(dev->device,180);		// 100 kHz <= 180 * 1 /36M
	i2c_set_own_7bit_slave_address(dev->device,0x23); // Necessary?
	i2c_peripheral_enable(dev->device);
}

/*********************************************************************
 * Return when I2C is not busy
 *********************************************************************/

void
i2c_wait_busy(I2C_Control *dev) {

	while ( I2C_SR2(dev->device) & I2C_SR2_BUSY )
		taskYIELD();			// I2C Busy

}

/*********************************************************************
 * Start I2C Read/Write Transaction with indicated 7-bit address:
 *********************************************************************/

void
i2c_start_addr(I2C_Control *dev,uint8_t addr,enum I2C_RW rw) {
	TickType_t t0 = systicks();

	i2c_wait_busy(dev);			// Block until not busy
	I2C_SR1(dev->device) &= ~I2C_SR1_AF;	// Clear Acknowledge failure
	i2c_clear_stop(dev->device);		// Do not generate a Stop
	if ( rw == Read )
		i2c_enable_ack(dev->device);
	i2c_send_start(dev->device);		// Generate a Start/Restart

	// Loop until ready:
        while ( !((I2C_SR1(dev->device) & I2C_SR1_SB) 
	  && (I2C_SR2(dev->device) & (I2C_SR2_MSL|I2C_SR2_BUSY))) ) {
		if ( diff_ticks(t0,systicks()) > dev->timeout )
			longjmp(i2c_exception,I2C_Addr_Timeout);
		taskYIELD();
	}

	// Send Address & R/W flag:
	i2c_send_7bit_address(dev->device,addr,
		rw == Read ? I2C_READ : I2C_WRITE);

	// Wait until completion, NAK or timeout
	t0 = systicks();
	while ( !(I2C_SR1(dev->device) & I2C_SR1_ADDR) ) {
		if ( I2C_SR1(dev->device) & I2C_SR1_AF ) {
			i2c_send_stop(dev->device);
			(void)I2C_SR1(dev->device);
			(void)I2C_SR2(dev->device); 	// Clear flags
			// NAK Received (no ADDR flag will be set here)
			longjmp(i2c_exception,I2C_Addr_NAK); 
		}
		if ( diff_ticks(t0,systicks()) > dev->timeout )
			longjmp(i2c_exception,I2C_Addr_Timeout); 
		taskYIELD();
	}

	(void)I2C_SR2(dev->device);		// Clear flags
}

/*********************************************************************
 * Write one byte of data
 *********************************************************************/

void
i2c_write(I2C_Control *dev,uint8_t byte) {
	TickType_t t0 = systicks();

	i2c_send_data(dev->device,byte);
	while ( !(I2C_SR1(dev->device) & (I2C_SR1_BTF)) ) {
		if ( diff_ticks(t0,systicks()) > dev->timeout )
			longjmp(i2c_exception,I2C_Write_Timeout);
		taskYIELD();
	}
}

/*********************************************************************
 * Read one byte of data. Set lastf=true, if this is the last/only
 * byte being read.
 *********************************************************************/

uint8_t
i2c_read(I2C_Control *dev,bool lastf) {
	TickType_t t0 = systicks();

	if ( lastf )
		i2c_disable_ack(dev->device);	// Reading last/only byte

	while ( !(I2C_SR1(dev->device) & I2C_SR1_RxNE) ) {
		if ( diff_ticks(t0,systicks()) > dev->timeout )
			longjmp(i2c_exception,I2C_Read_Timeout);
		taskYIELD();
	}
	
	return i2c_get_data(dev->device);
}

/*********************************************************************
 * Write one byte of data, then initiate a repeated start for a
 * read to follow.
 *********************************************************************/

void
i2c_write_restart(I2C_Control *dev,uint8_t byte,uint8_t addr) {
	TickType_t t0 = systicks();

	taskENTER_CRITICAL();
	i2c_send_data(dev->device,byte);
	// Must set start before byte has written out
	i2c_send_start(dev->device);
	taskEXIT_CRITICAL();

	// Wait for transmit to complete
	while ( !(I2C_SR1(dev->device) & (I2C_SR1_BTF)) ) {
		if ( diff_ticks(t0,systicks()) > dev->timeout )
			longjmp(i2c_exception,I2C_Write_Timeout);
		taskYIELD();
	}

	// Loop until restart ready:
	t0 = systicks();
        while ( !((I2C_SR1(dev->device) & I2C_SR1_SB) 
	  && (I2C_SR2(dev->device) & (I2C_SR2_MSL|I2C_SR2_BUSY))) ) {
		if ( diff_ticks(t0,systicks()) > dev->timeout )
			longjmp(i2c_exception,I2C_Addr_Timeout);
		taskYIELD();
	}

	// Send Address & Read command bit
	i2c_send_7bit_address(dev->device,addr,I2C_READ);

	// Wait until completion, NAK or timeout
	t0 = systicks();
	while ( !(I2C_SR1(dev->device) & I2C_SR1_ADDR) ) {
		if ( I2C_SR1(dev->device) & I2C_SR1_AF ) {
			i2c_send_stop(dev->device);
			(void)I2C_SR1(dev->device);
			(void)I2C_SR2(dev->device); 	// Clear flags
			// NAK Received (no ADDR flag will be set here)
			longjmp(i2c_exception,I2C_Addr_NAK); 
		}
		if ( diff_ticks(t0,systicks()) > dev->timeout )
			longjmp(i2c_exception,I2C_Addr_Timeout); 
		taskYIELD();
	}

	(void)I2C_SR2(dev->device);		// Clear flags
}

// i2c.c
