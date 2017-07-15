/* utils.h - Utiltiies for stm32/FreeRTOS programming
 * Warren W. Gay VE3WWG	Fri Jul 14 20:14:45 2017
 */
#ifdef UTILS_H
#define UTILS_H

inline TickType_t
ticksdiff(TickType_t first,TickType_t last) {

	if ( last > first )
		return last - first;
	else	return (~(TickType_t)0 - first) + 1 + last;
}

#endif // UTILS_H

// End utils.h
