/* Intel Hex Support
 * Warren W. Gay VE3WWG
 * Sat Oct 28 14:26:51 2017
 */

#ifndef INTELHEX_H
#define INTELHEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * Intel Hex Struct:
 *********************************************************************/

struct s_ihex {
	uint32_t	baseaddr;	// Extended base address
	uint32_t	addr;		// Current address
	uint8_t		length;		// Record length
	uint8_t		rtype;		// Record type
	uint8_t		checksum;	// Given checksum
	uint8_t		compcsum;	// Computed checksum
	uint8_t		data[128];	// Read data
	uint32_t	compaddr;	// Computed address
};

#define IHEX_RT_DATA	0x00	// data record
#define IHEX_RT_EOF	0x01	// end-of-file record
#define IHEX_RT_XSEG	0x02	// extended segment address record
#define IHEX_RT_XLADDR	0x04	// extended linear address record
#define IHEX_RT_SLADDR	0x05	// start linear address record (MDK-ARM only)

#define IHEX_FAIL	0x0100	// Parse failed

typedef struct s_ihex s_ihex;

void ihex_init(s_ihex *ihex);
unsigned ihex_parse(struct s_ihex *ihex,const char *text);

#ifdef __cplusplus
}
#endif

#endif // INTELHEX_H

// End intelhex.h
