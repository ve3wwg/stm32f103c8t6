######################################################################
#  Project.mk is used for creating new FreeRTOS projects. See the
#  associated README.md file in this directory for usage info.
######################################################################

# Edit this variable if your release differs from what is shown here:

FREERTOS	?= FreeRTOSv10.0.1

######################################################################
#  Internal variables
######################################################################

PROJECT		?= newproject
RTOSDIR 	:= $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
FREERTOSDIR	:= $(abspath $(RTOSDIR)/$(FREERTOS))
PROJRTOS	= $(PROJECT)/rtos

.PHONY:	rsrc all group1 group2 group3 group4 license setup

all:	check rsrc group1 group2 group3 group4 license \
	  $(PROJECT)/stm32f103c8t6.ld setup \
	  $(PROJECT)/FreeRTOSConfig.h
	@echo "****************************************************************"
	@echo "Your project in subdirectory $(PROJECT) is now ready."
	@echo
	@echo "1. Edit FreeRTOSConfig.h per project requirements."
	@echo "2. Edit Makefile SRCFILES as required. This also"
	@echo "   chooses which heap_*.c to use."
	@echo "3. Edit stm32f103c8t6.ld if necessary."
	@echo "4. make"
	@echo "5. make flash"
	@echo "6. make clean or make clobber as required"
	@echo "****************************************************************"

check:
	@if [ "$(PROJECT)" = "newproject" ] ; then \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		echo "Please supply PROJECT='<projectname>' on the" ; \
		echo "make command line." ; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		exit 2 ; \
	fi
	@if [ -d $(PROJECT) ] ; then \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		echo "Subdirectory $(PROJECT) already exists!" ; \
		echo "Cowardly refusing to delete/update it." ; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		exit 2 ; \
	fi

setup:
	cp $(RTOSDIR)/src/opencm3.c $(PROJRTOS)/.
	cp $(RTOSDIR)/src/main.c $(PROJECT)/.
	cp $(RTOSDIR)/src/Makefile $(PROJECT)/.

rsrc:
	@if [ ! -d "$(FREERTOSDIR)" ] ; then \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		echo "FreeRTOS subdirectory $(FREERTOS) does not exist." ; \
		echo "( $(FREERTOSDIR) )" ; \
		echo ; \
		echo "Download and unzip the FreeRTOS zip file here. It should" ; \
		echo "produce a subdirectory like ./$(FREERTOS). If the downloaded" ; \
		echo "release differs, then also edit the variable FREERTOS= at the top" ; \
		echo "of this Makefile." ; \
		echo ; \
		echo "Then try again." ; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		exit 2; \
	fi 
	@if [ ! -d "$(FREERTOSDIR)/FreeRTOS" ] ; then \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		echo "FreeRTOS is improperly unzipped. There should be a" ; \
		echo "subdirectory ./FreeRTOS under $(FREERTOSDIR)." ; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		exit 2; \
	fi
	@if [ ! -d ../libopencm3 -o ! -f ../libopencm3/Makefile ] ; then \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		echo "Directory ../libopencm3 is missing or incomplete. Make sure that" ; \
		echo "the git submodule is checked out and a make has been performed" ; \
		echo "there." ; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		exit 2; \
	fi
	@if [ ! -f ../libopencm3/lib/libopencm3_stm32f1.a ] ; then \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		echo "Directory ../libopencm3 exists but the library file " ; \
		echo "../libopencm3/lib/libopencm3_stm32f1.a has not been created. " ; \
		echo "Please change to the ./libopencm3 and type make to build that " ; \
		echo "git submodule. " ; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" ; \
		exit 2; \
	else \
		mkdir -p "$(PROJRTOS)" ; \
	fi

group1:
	@for f in list.c queue.c tasks.c ; do \
		echo "cp '$(FREERTOSDIR)/FreeRTOS/Source/$$f' '$(PROJRTOS)/.'" ; \
		cp "$(FREERTOSDIR)/FreeRTOS/Source/$$f" "$(PROJRTOS)/." ; \
	done

group2:
	@for f in $(FREERTOSDIR)/FreeRTOS/Source/portable/MemMang/heap_*.c ; do \
		echo "cp '$$f' '$(PROJRTOS)/.'" ; \
		cp "$$f" "$(PROJRTOS)/." ; \
	done

group3:
	@for f in port.c portmacro.h ; do \
		echo "cp '$(FREERTOSDIR)/FreeRTOS/Source/portable/GCC/ARM_CM3/$$f' '$(PROJRTOS)/.'" ; \
		cp "$(FREERTOSDIR)/FreeRTOS/Source/portable/GCC/ARM_CM3/$$f" "$(PROJRTOS)/." ; \
	done

group4:
	@for f in FreeRTOS.h mpu_prototypes.h projdefs.h stdint.readme StackMacros.h event_groups.h mpu_wrappers.h queue.h task.h croutine.h list.h \
		portable.h semphr.h timers.h deprecated_definitions.h stack_macros.h ; do \
		cp $(FREERTOSDIR)/FreeRTOS/Source/include/$$f $(PROJRTOS)/. ; \
	done

license:
	cp $(FREERTOSDIR)/FreeRTOS/License/license.txt $(PROJRTOS)/LICENSE

# We don't use stm32f10x_lib.h because libopencm3 provides our device driver 
# facilities. So we comment that line out of FreeRTOSConfig.h
#
$(PROJECT)/FreeRTOSConfig.h:
	@echo "Editing $(PROJRTOS)/FreeRTOSConfig.h"
	sed <$(FREERTOSDIR)/FreeRTOS/Demo/CORTEX_STM32F103_Primer_GCC/FreeRTOSConfig.h \
	    >$(PROJECT)/FreeRTOSConfig.h -f $(RTOSDIR)/src/alters.sed

$(PROJECT)/stm32f103c8t6.ld:
	cp $(RTOSDIR)/src/stm32f103c8t6.ld $(PROJECT)/stm32f103c8t6.ld

# End
