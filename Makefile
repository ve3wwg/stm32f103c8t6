######################################################################
#  Top Level: STM32F103C8T6 Projects
######################################################################

PROJECTS	= miniblink uart uarthwfc

all:	libopencm3/lib/libopencm3_stm32f1.a
	for d in $(PROJECTS) ; do \
		$(MAKE) -C $$d ; \
	done
	$(MAKE) -$(MAKEFLAGS) -C ./rtos 

clean:
	for d in $(PROJECTS) ; do \
		$(MAKE) -C $$d clean ; \
	done
	$(MAKE) -$(MAKEFLAGS) -C ./rtos clean

clobber:
	for d in $(PROJECTS) ; do \
		$(MAKE) -C $$d clobber ; \
	done
	$(MAKE) -$(MAKEFLAGS) -C ./rtos clobber
	rm -f libopencm3/lib/libopencm3_stm32f1.a

libopencm3/lib/libopencm3_stm32f1.a:
	$(MAKE) -C libopencm3

MAKE	= gmake

# End
