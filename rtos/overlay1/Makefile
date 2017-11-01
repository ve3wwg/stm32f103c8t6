######################################################################
#  Project Makefile
######################################################################

BINARY		= main
SRCFILES	= main.c rtos/heap_4.c rtos/list.c rtos/port.c rtos/queue.c rtos/tasks.c rtos/opencm3.c
LDSCRIPT	= stm32f103c8t6.ld

CLOBBER	+= 	*.ov

include ../../Makefile.incl
include ../Makefile.rtos

main.elf: $(OBJS)
	$(LD) $(TGT_LDFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o main.elf
	@rm -f *.ov all.hex
	for v in fee fie foo fum ; do \
		$(OBJCOPY) -O ihex -j.$$v main.elf $$v.ov ; \
		cat $$v.ov | sed '/^:04000005/d;/^:00000001/d' >>all.hex ; \
	done
	$(OBJCOPY) -Obinary -R.fee -R.fie -R.foo -R.fum main.elf main.bin

######################################################################
#  NOTES:
#	1. remove any modules you don't need from SRCFILES
#	2. "make clean" will remove *.o etc., but leaves *.elf, *.bin
#	3. "make clobber" will "clean" and remove *.elf, *.bin etc.
#	4. "make flash" will perform:
#	   st-flash write main.bin 0x8000000
######################################################################
