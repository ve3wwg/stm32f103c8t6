USING THESE LIBRARY MODULES FROM YOUR PROJECT
---------------------------------------------

The files in this library subdirectory, must unfortunately be compiled
in your respective FreeRTOS project directories. This is due to having
potentially differently configured FreeRTOS builds.

Define your project in the usual way listing all of the input source
files for your project build. Also add the library modules to be
included (these will be copied). I'll use uartlib.c and .h for this
example:

    BINARY      = main
    SRCFILES    = main.c uartlib.c rtos/heap_4.c ...
    LDSCRIPT    = stm32f103c8t6.ld

Since the library modules must be copied to your project directory,
add them to the dependencies macro DEPS:

    DEPS        = uartlib.c uartlib.h

This will cause make to build (copy) these sources first from the
following rules that you'll add to your Makefile:

    uartlib.c: ../libwwg/uartlib.c
        cp ../libwwg/uartlib.c .

    uartlib.h: ../libwwg/uartlib.h
        cp ../libwwg/uartlib.h .

    uartlib.o: uartlib.h

Since these are copies of the original sources, you should also add
these file names to the macro CLOBBER. That way when you do a  "make
clobber", the copied files will be deleted.

    CLOBBER     += uartlib.h uartlib.c

IMPORTANT:
----------

Customizations should ONLY be made in this shared directory (libwwg), 
to prevent loss of changes (due to the files being _copied_).
