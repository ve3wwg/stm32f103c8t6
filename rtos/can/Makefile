######################################################################
#  Project Makefile
######################################################################

.PSEUDO: all front clean clobber flash flash_front flash_rear

all:
	$(MAKE) -f Makefile.main -$(MAKEFLAGS)
	$(MAKE) -f Makefile.front -$(MAKEFLAGS)
	$(MAKE) -f Makefile.rear -$(MAKEFLAGS)

clean:
	$(MAKE) -f Makefile.main -$(MAKEFLAGS) clean
	$(MAKE) -f Makefile.front -$(MAKEFLAGS) clean
	$(MAKE) -f Makefile.rear -$(MAKEFLAGS) clean

clobber:
	$(MAKE) -f Makefile.main -$(MAKEFLAGS) clobber
	$(MAKE) -f Makefile.front -$(MAKEFLAGS) clobber
	$(MAKE) -f Makefile.rear -$(MAKEFLAGS) clobber

flash:
	$(MAKE) -f Makefile.main -$(MAKEFLAGS) flash

flash_front:
	$(MAKE) -f Makefile.front -$(MAKEFLAGS) flash

flash_rear:
	$(MAKE) -f Makefile.rear -$(MAKEFLAGS) flash

