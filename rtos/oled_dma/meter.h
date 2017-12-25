//////////////////////////////////////////////////////////////////////
// meter.hpp -- Base class for analog meter
// Date: Wed Dec  6 22:50:23 2017   (C) Warren W. Gay ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#ifndef METER_HPP
#define METER_HPP

struct Meter {
	float		range;
	float		value;		// Meter value (volts)
	short		cx, cy;		// Center
	short		rd;		// Radius difference
	short		cr;		// Circle radius
	short		ocr, icr;	// Outer and inner radius
	short		dx;		// Tick delta x
	short		dy;		// Tick delta y
	short		tw;		// Label text width
};

void meter_init(struct Meter *m,float range);
void meter_redraw(struct Meter *m);
void meter_set_value(struct Meter *m,float v);
void meter_update(void);

#endif // METER_HPP

// End meter.hpp
