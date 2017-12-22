///////////////////////////////////////////////////////////////////////
// meter.c -- Implementation for Analog Meter
// Date: Wed Dec  6 22:51:32 2017   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <math.h>
#include <string.h>

#include "ugui.h"
#include "meter.h"
#include "miniprintf.h"
#include "oled.h"

#define ABS(x) (x < 0 ? -(x) : (x))
#define SGN(x) (x < 0 ? -1 : 1)

static UG_GUI gui;
static float Pi = 3.14159265;
static uint8_t pixmap[128*64/8];
static uint8_t dummy;

static uint8_t *
to_pixel(short x,short y,unsigned *bitno) {
	*bitno = 7 - y % 8;	// Inverted

	if ( x < 0 || x >= 128
	  || y < 0 || y >= 64 )
	  	return &dummy;

	unsigned inv_y = 63 - y;
	unsigned pageno = inv_y / 8;
	unsigned colno = x % 128;

	return &pixmap[pageno * 128 + colno];
}

static void
draw_point(short x,short y,short pen) {

	if ( x < 0 || x >= 128 || y < 0 || y >= 64 )
		return;

	unsigned bitno;
	uint8_t *byte = to_pixel(x,y,&bitno);
	uint8_t mask = 1 << bitno;
	
	switch ( pen ) {
	case 0:
		*byte &= ~mask;
		break;
	case 1:
		*byte |= mask;
		break;
	default:
		*byte ^= mask;
	}
}

static int
ug_to_pen(UG_COLOR c) {

	switch ( c ) {
	case C_BLACK:
		return 0;
	case C_RED:
		return 2;
	default:
		return 1;
	}
}

static UG_COLOR
pen_to_ug(int pen) {

	switch ( pen ) {
	case 0:
		return C_BLACK;
	case 2:
		return C_RED;
	default:
		return C_WHITE;
	}	
}

static void
local_draw_point(UG_S16 x,UG_S16 y,UG_COLOR c) {
	draw_point(x,y,ug_to_pen(c));
}

void
meter_init(struct Meter *m,float range) {

	memset(pixmap,0,128*64/8);

	m->value = 0.0;
	m->rd = 6;
	m->cr = 40;
	m->dx = 1;
	m->dy = 4;
	m->tw = 14;
	m->range = range;

	UG_Init(&gui,local_draw_point,128,64);
	m->cx = 128 / 2;
	m->cy = 64 - 1;
	m->ocr = m->cr + m->rd;
	m->icr = m->cr - m->rd;
	UG_SetBackcolor(pen_to_ug(1));
	UG_SetForecolor(pen_to_ug(0));
	meter_redraw(m);
}

static void
ticks(struct Meter *m,int t) {
	float theta = (t * Pi)/8.0;
	int x1, y1, x2, y2, x, y;
	float incr = m->range / 8;
	int fm, fr;
	char buf[16];

	x1 = m->icr * cos(theta);
	y1 = m->icr * sin(theta);
	x2 = m->ocr * cos(theta);
	y2 = m->ocr * sin(theta);

	UG_DrawLine(m->cx-x1,m->cy-y1,x=m->cx-x2,y=m->cy-y2,pen_to_ug(2));
	if ( t != 4 ) {
		fm = incr * t * 10.0;
		fr = fm % 10;
		fm /= 10;
		mini_snprintf(buf,sizeof buf,"%d.%1d",fm,fr);
		UG_PutString(x-m->tw,y-m->dy,buf);

		UG_DrawLine(m->cx+x1,m->cy-y1,x=m->cx+x2,y=m->cy-y2,pen_to_ug(2));

		fm = incr * (8 - t) * 10.0;
		fr = fm % 10;
		fm /= 10;
		mini_snprintf(buf,sizeof buf,"%d.%1d",fm,fr);
		UG_PutString(x+3,y-m->dy,buf);
	}
}

void
meter_redraw(struct Meter *m) {

	UG_FillScreen(pen_to_ug(1));
	UG_FillCircle(m->cx,m->cy,m->cr,pen_to_ug(0));
	UG_DrawCircle(m->cx,m->cy,m->icr,pen_to_ug(1));
	UG_FontSelect(&FONT_4X6);
	UG_FontSetHSpace(0);

	for ( int x=0; x<=4; ++x )
		ticks(m,x);
	UG_FillFrame(0,0,127,15,pen_to_ug(1));
	meter_set_value(m,m->value);
}

static void
draw_pointer(struct Meter *m,float v,int pen) {
	int pr = m->cr-m->rd-2;		// Pointer radius
	float theta = v / m->range * Pi;
	int x2 = pr * cos(theta);
	int y2 = pr * sin(theta);
	int dy = y2 < 5 ? 1 : 0;

	for ( int x=0; x < 3; ++x ) {
		if ( m->cy+dy >= m->cy )
			UG_DrawLine(m->cx-x,m->cy+dy,m->cx-x2,m->cy-y2,pen_to_ug(pen));
		if ( x > 0 && m->cy+dy >= m->cy )
			UG_DrawLine(m->cx+x,m->cy+dy,m->cx-x2,m->cy-y2,pen_to_ug(pen));
	}
}

void
meter_set_value(struct Meter *m,float v) {
	char buf[16];
	int fm, fr;

	draw_pointer(m,m->value,0);
	UG_FillFrame(0,0,127,15,pen_to_ug(1));
	m->value = v > m->range ? m->range : v;
	if ( m->value < 0.0 )
		m->value = 0.0;

	draw_pointer(m,m->value,1);
	UG_FontSelect(&FONT_8X12);
	UG_FontSetHSpace(0);

	fm = m->value * 100.0;
	fr = fm % 100;
	fm /= 100;
	int slen = mini_snprintf(buf,sizeof buf,"%d.%02d Volts",fm,fr);
	UG_PutString(m->cx-8*slen/2,2,buf);
}

void
meter_update(void) {
	uint8_t *pp = pixmap;

	oled_command2(0x20,0x02);// Page mode
	oled_command(0x40);
	oled_command2(0xD3,0x00);
	for ( uint8_t px=0; px<8; ++px ) {
		oled_command(0xB0|px);
		oled_command(0x00); // Lo col
		oled_command(0x10); // Hi col
		for ( unsigned bx=0; bx<128; ++bx )
			oled_data(*pp++);
	}
}

// End meter.c
