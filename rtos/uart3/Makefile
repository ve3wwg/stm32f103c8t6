######################################################################
#  Project Makefile
######################################################################

BINARY		= uart
SRCFILES	= uart.c uartlib.c rtos/heap_4.c rtos/list.c rtos/port.c rtos/queue.c rtos/tasks.c rtos/opencm3.c
LDSCRIPT	= stm32f103c8t6.ld
DEPS		= uartlib.c uartlib.h
CLOBBER 	+= uartlib.h uartlib.c

include ../../Makefile.incl
include ../Makefile.rtos

uartlib.c: ../libwwg/uartlib.c
	cp ../libwwg/uartlib.c .

uartlib.h: ../libwwg/uartlib.h
	cp ../libwwg/uartlib.h .

uartlib.o: uartlib.h


######################################################################
#  NOTES:
#
#	1. remove any modules you don't need from SRCFILES
#
#	2. "make clean" will remove *.o etc., but leaves *.elf, *.bin
#
#	3. "make clobber" will "clean" and remove *.elf, *.bin etc.
#
#	4. "make flash" will perform:
#	
#	   st-flash write main.bin 0x8000000
#
######################################################################
