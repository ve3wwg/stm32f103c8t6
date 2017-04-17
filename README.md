Project stm32f103c8t6
---------------------

This git project contains libopencm3 (only) and FreeRTOS+libopencm3
projects. This provides a ready development environment for those who
want to apply the economical STM32F103 to using libopencm3 alone or in
concert with FreeRTOS.

THIS PROJECT ONLY USES OPEN SOURCE TOOLS
----------------------------------------

No Windows based IDE environments are used or implied! Cygwin, Linux,
MacOS and *BSD environments should be suitable.

This project exists to bring several components together in a manner
that just works for the STM32F103.

PROJECT STRUCTURE:
------------------

The top level defines the environment common to libopencm3 mainly, and
used by FreeRTOS builds in the ./rtos subdirectory.

At the top level directory, projects like ./miniblink use libopencm3
only (they do NOT use FreeRTOS).

The subdirectory ./rtos contains projects that do use FreeRTOS (and
libopencm3).

    stm32f103c8t6/
        |- README.md
        |- LICENSE
        |- libopencm3/          git submodule
        |- stm32f103c8t6.ld
        |- Makefile             makes all top-level projects
        |- Makefile.incl        rules for make file builds
        |- miniblink/           simple blink: libopencm3 only
            |- *                miniblink files
        |- uart/                UART example (no flow control)
            |- *        
        |- uarthwfc/            UART example with hardware flow control
            |- *
        |- rtos/                FreeRTOS projects
            |- FreeRTOSv9.0.0   Unzipped FreeRTOS sources (you create this)
            |- Makefile         Used for creating new rtos projects
            |- Makefile.rtos    Rules for rtos project builds
            |- src/
                |- *            Files used for project creation
            |- libwwg
                |- Makefile
                |- include/*.h
                |- src/*.c
                |- libwwg.a
            |- blinky           Test FreeRTOS project
                |- *            Blinky project related files
                |- rtos
                    |- *        RTOS Support files for blinky
            |- uart3            Another project..
                |- *
                |- rtos
                    |- *
            |- usbcdc           USB CDC Demo
                |- etc.
        
PREREQUISITES:
--------------

    0a. It will be assumed here that you have your cross compiler
        toolchain installed. In Makefile.incl you'll see references
        to a PREFIX variable. This defaults to arm-none-eabi so that
        gcc for example will be arm-none-eabi-gcc etc. If your 
        prefix differs, edit Makefile.incl at the top level to
        match, or use the shell: export PREFIX=whatever
    
    0b. It is also assumed that you have the st-link command installed
        on your system. You may need to download and install it. 
        Google is your friend.

    1.  If you didn't use a --recursive git clone, then you need to make
        sure that libopencm3 is fetched now. From the top level apply
        one of:
    
            $ git submodule update --init --recursive   # First time

            $ git submodule update --recursive          # Subsequent
    
    2.  Go into ./libopencm3 and type "make". This should build the 
        static libraries that will be needed. Any issues there should
        go to the libopencm3 community.
    
    3.  For FreeRTOS, cd into ./rtos and unzip your FreeRTOS download
        there. It should create a subdirectory FreeRTOSv9.0.0 or 
        similar. If the release differs, you'll need to change the
        variable assignment in rtos/Makefile FREERTOS ?= ....
        to match, or use the shell: export FREERTOS=whatever
    
    4.  Do NOT compile FreeRTOS, since portions of it will be copied
        to your project subdirectory for further customization. Each
        project is capable of running a different FreeRTOS configuration.

TEST EXAMPLES:
--------------

After the prerequisites have been met:

Going into ./miniblink and:

    $ make
    $ make flash

should build and flash your example program. Flashing assumes the
use of the st-link (v2) command.

Or go into ./rtos/blink:

    $ make
    $ make flash

will build your FreeRTOS blink example.

If you are encountering make errors, then try forcing the use
of Gnu make (it may be installed as "gmake").

CREATING NEW FreeRTOS PROJECTS:
-------------------------------

    1. cd ./rtos
    2. make -f Project.mk PROJECT=name
    3. cd ./name
    4. Tweak and build

See ./rtos/README.md for more details.

LICENSE:
--------

All files covered by _this_ repo (_except_ those files copied from
libopencm3 and FreeRTOS), are covered by the Gnu Lesser General Public
License Version 3. See the file named LICENSE.

