//////////////////////////////////////////////////////////////////////
// oled.hpp -- OLED Functions
// Date: Sat Dec 16 13:42:04 2017   (C) Warren Gay
///////////////////////////////////////////////////////////////////////

#ifndef OLED_H
#define OLED_H

void oled_command(uint8_t byte);
void oled_command2(uint8_t byte,uint8_t byte2);
void spi_dma_xmit_pixmap(void);

#endif // OLED_H

// End oled.h
