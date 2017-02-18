Project stm32f103c8t6
---------------------

This git project contains libopencm3 (only) and FreeRTOS+libopencm3 projects.
This provides a ready development environment for those who want to apply the economical
STM32F103 to using libopencm3 alone or in concert with
FreeRTOS. Why struggle with  gleaning from forums etc.?

THIS PROJECT USES ONLY OPEN SOURCE TOOLS
----------------------------------------

No Windows based IDE environments are used or implied! Linux and MacOS
users rejoice (can you hear the cheers?) Vendors need to be reminded that
not everyone uses Windows. 

This project exists to bring several components together in a manner
that just works for the STM32F013 -- out of the box!

PROJECT STRUCTURE:
------------------

The top level defines the environment common to libopencm3 mainly, and
used by FreeRTOS builds in the ./rtos area.

At the top level directory, projects like ./miniblink use libopencm3
only (they do NOT use FreeRTOS).

Under the subdirectory ./rtos, projects in subdirectories there use
FreeRTOS (and libopencm3).

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
                |- *            Files used for project creastion
            |- blinky           Test FreeRTOS project
                |- *            Blinky project related files
                |- rtos
                    |- *        RTOS Support files for blinky
        
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
        sure that libopencm3 is fetched now. From the top level apply:
    
        git submodule update --recursive
    
    2.  Go into ./libopencm3 and type "make". This should build the 
        static libraries that will be needed. Any issues there should
        go to the libopencm3 community.
    
    3.  For FreeRTOS, cd into ./rtos and unzip your FreeRTOS download
        there. It should create a subdirectory FreeRTOSv9.0.0 or 
        similar. If the release differs, you'll need to change the
        variable assignment in rtos/Makefile FREERTOS ?= ....
        to match, or use the shell: export FREERTOS=whatever
    
    4.  Do NOT compile FreeRTOS, since portions of it will be copied
        to your project subdirectory for further optional
        customization.

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
    2. make PROJECT=name
    3. cd ./name
    4. Tweak and build

See ./rtos/README.md for more details.

LICENSE:
--------

See the file LICENSE, which applies to all files in this repo except for
FreeRTOS files that may be copied to some of the project subdirectories.
Licensing of libopencm3 and FreeRTOS are specified by those projects alone.
