/* Intel Hex Routines
 * Warren W. Gay VE3WWG
 * Sat Oct 28 19:52:52 2017
 */
#include <stdlib.h>
#include <string.h>

#include "intelhex.h"

/*********************************************************************
 * Initialize struct s_ihex
 *********************************************************************/

void
ihex_init(s_ihex *ihex) {
	memset(ihex,0,sizeof *ihex);
}

/*********************************************************************
 * Internal: Parse starting from text, for n chars.
 *********************************************************************/

static uint32_t
to_hex(const char *text,unsigned n,const char **rp) {
	char buf[n+1];

	strncpy(buf,text,n)[n] = 0;
	*rp = text + strlen(buf);
	return strtoul(buf,0,16);
}

/*********************************************************************
 * Parse intel hex line into struct s_ihex.
 * RETURNS:
 *	Intel Hex record type else IHEX_FAIL.
 *********************************************************************/

unsigned
ihex_parse(s_ihex *ihex,const char *text) {
	const char *cp = strchr(text,':');
	unsigned csum;

	if ( !cp )
		return IHEX_FAIL;

	memset(ihex->data,0,sizeof ihex->data);

	++cp;
	ihex->length = to_hex(cp,2,&cp);
	ihex->addr   = to_hex(cp,4,&cp);
	ihex->rtype  = to_hex(cp,2,&cp);

	if ( ihex->length > sizeof ihex->data )
		return IHEX_FAIL;

	csum = ihex->length + ((ihex->addr >> 8) & 0xFF) + (ihex->addr & 0xFF)
		+ ihex->rtype;

	for ( unsigned ux=0; ux<ihex->length; ++ux ) {
		ihex->data[ux] = to_hex(cp,2,&cp);
		csum += ihex->data[ux];
	}
	ihex->checksum = to_hex(cp,2,&cp);
	ihex->compcsum = (-(int)(csum & 0x0FF)) & 0xFF;
	if ( ihex->compcsum != ihex->checksum )
		return IHEX_FAIL;

	if ( ihex->rtype == IHEX_RT_XLADDR ) {
		ihex->baseaddr = (uint32_t)ihex->data[0] << 24
			| (uint32_t)ihex->data[1] << 16;
	} else if ( ihex->rtype == IHEX_RT_SLADDR ) {
		ihex->compaddr = (uint32_t)ihex->data[0] << 24
			| (uint32_t)ihex->data[1] << 16
			| (uint32_t)ihex->data[2] << 8
			| (uint32_t)ihex->data[3];
	} else	{
		ihex->compaddr = ihex->baseaddr + ihex->addr;
	}
	return ihex->rtype;
}

// End intelhex.c
