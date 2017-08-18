/* common.h
 * Warren W. Gay VE3WWG
 */

extern void adventure(void *arg);

enum LampActions {
	Take,
	Filled,
	Drop
};

extern void set_lamp(enum LampActions action);

// End common.h
