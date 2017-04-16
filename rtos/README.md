CREATE A NEW FreeRTOS PROJECT:
------------------------------

There is a Makefile at this level designed to setup new
FreeRTOS projects in a subdirectory. To use it, simply
perform:

	make -f Project.mk PROJECT=myproj

This will create a subdirectory ./myproj containing the
necessary source files and Makefile for that project.


FreeRTOS SOURCE CODE:
---------------------

The Makefile depends upon having the FreeRTOS sources
unzipped here (for example ./FreeRTOSv9.0.0). Once
your project has been created (above), you can remove
these distributed sources if you need to save space.

libwwg:
-------

If you did a "make" at the top level directory, this
should already be done. But if you made changes to the
libwwg library, you can do:

    make -C ./libwwg

    NOTE:

    The modules compiled in the libwwg directory,
    compile with the FreeRTOSConfig.h file in
    libwwg/rtos. If the module(s) depend upon
    special customizations, then it is best to
    copy the affected modules to the specific project
    directory and build them there.
