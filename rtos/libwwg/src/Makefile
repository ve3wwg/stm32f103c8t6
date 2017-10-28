######################################################################
#  libwwg/src/Makefile for libwwg.a
######################################################################

SRCFILES	= usbcdc.c uartlib.o miniprintf.o mcuio.o getline.o \
		  monitor.o winbond.o intelhex.o

TEMP1 		= $(patsubst %.c,%.o,$(SRCFILES))
TEMP2		= $(patsubst %.asm,%.o,$(TEMP1))
OBJS 		= $(patsubst %.cpp,%.o,$(TEMP2))

../libwwg.a: $(OBJS)
	@rm -f ../libwwg.a
	$(AR) rv ../libwwg.a $(OBJS)

usbcdc.o: ../include/usbcdc.h
uartlib.o: ../include/uartlib.h
mcuio.o: ../include/mcuio.h
winbond.o: ../include/winbond.h
intelhex.o: ../include/intelhex.h

include ../../../Makefile.incl
include ../../Makefile.rtos

# End Makefile
