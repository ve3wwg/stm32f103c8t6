s|\(#include *"stm32f10x_lib.h"\)|/* \1 */|
/#define configUSE_TICK_HOOK/s|1|0|
/#define configCPU_CLOCK_HZ/s|$|\
#define configSYSTICK_CLOCK_HZ ( configCPU_CLOCK_HZ / 8 )  /* fix for vTaskDelay() */|
