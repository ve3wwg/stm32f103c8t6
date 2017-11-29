/* A stm32f103 library for user applications
 * Warren W. Gay VE3WWG
 * Sat Nov 25 11:53:53 2017
 */
#ifndef I2C_H
#define I2C_H

#include <stdbool.h>
#include <setjmp.h>

#include <libopencm3/stm32/i2c.h>

typedef enum {
	I2C_Ok = 0,
	I2C_Addr_Timeout,
	I2C_Addr_NAK,
	I2C_Write_Timeout,
	I2C_Read_Timeout
} I2C_Fails;

enum I2C_RW {
	Read = 1,
	Write = 0
};

typedef struct {
	uint32_t	device;		// I2C device
	uint32_t	timeout;	// Ticks
} I2C_Control;

extern jmp_buf i2c_exception;

const char *i2c_error(I2C_Fails fcode);

void i2c_configure(I2C_Control *dev,uint32_t i2c,uint32_t ticks);
void i2c_wait_busy(I2C_Control *dev);
void i2c_start_addr(I2C_Control *dev,uint8_t addr,enum I2C_RW rw);
void i2c_write(I2C_Control *dev,uint8_t byte);
void i2c_write_restart(I2C_Control *dev,uint8_t byte,uint8_t addr);
uint8_t i2c_read(I2C_Control *dev,bool lastf);

inline void i2c_stop(I2C_Control *dev) { i2c_send_stop(dev->device); }

#endif // I2C_H

// End i2c.h
