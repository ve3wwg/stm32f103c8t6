######################################################################
#  Top Level: STM32F103C8T6 Projects
######################################################################

PROJECTS = miniblink uart uarthwfc

.PHONY = libopencm3 clobber_libopencm3 clean_libopencm3

all:	libopencm3
	for d in $(PROJECTS) ; do \
		$(MAKE) -C $$d ; \
	done
	$(MAKE) -$(MAKEFLAGS) -C ./rtos 

clean:	clean_libopencm3
	for d in $(PROJECTS) ; do \
		$(MAKE) -C $$d clean ; \
	done
	$(MAKE) -$(MAKEFLAGS) -C ./rtos clean

clobber: clobber_libopencm3
	for d in $(PROJECTS) ; do \
		$(MAKE) -C $$d clobber ; \
	done
	$(MAKE) -$(MAKEFLAGS) -C ./rtos clobber

clean_libopencm3: clobber_libopencm3

clobber_libopencm3:
	rm -f libopencm3/lib/libopencm3_stm32f1.a
	-$(MAKE) -$(MAKEFLAGS) -C ./libopencm3 clean

libopencm3: libopencm3/lib/libopencm3_stm32f1.a

libopencm3/lib/libopencm3_stm32f1.a:
	$(MAKE) -C libopencm3

MAKE	= gmake

# End
