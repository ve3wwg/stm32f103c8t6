/* winbond.hpp -- Winbond Flash Devices w25qxx
 * Warren Gay  Sat Oct 28 19:23:36 2017   (C) datablocks.net
 */
#ifndef WINBOND_H
#define WINBOND_H

#include <stdint.h>
#include <stdbool.h>

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

uint8_t w25_read_sr1(uint32_t spi);
uint8_t w25_read_sr2(uint32_t spi);
void w25_wait(uint32_t spi);
bool w25_is_wprotect(uint32_t spi);
void w25_write_en(uint32_t spi,bool en);

uint16_t w25_manuf_device(uint32_t spi);
uint32_t w25_JEDEC_ID(uint32_t spi);
void w25_read_uid(uint32_t spi,void *buf,uint16_t bytes);

void w25_power(uint32_t spi,bool on);

uint32_t w25_read_data(uint32_t spi,uint32_t addr,void *data,uint32_t bytes);
unsigned w25_write_data(uint32_t spi,uint32_t addr,void *data,uint32_t bytes);

bool w25_chip_erase(uint32_t spi);
bool w25_erase_block(uint32_t spi,uint32_t addr,uint8_t cmd);

void w25_spi_setup(
  uint32_t spi,		// SPI1 or SPI2
  bool bits8,		// True for 8-bits else 16-bits
  bool msbfirst,	// True if MSB first else LSB first
  bool mode0,		// True if mode 0 else mode 3
  uint8_t fpclk_div	// E.g. SPI_CR1_BAUDRATE_FPCLK_DIV_256
);

#endif // WINBOND_H

// End winbond.h
