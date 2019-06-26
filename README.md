Project stm32f103c8t6
---------------------

This is the source code for the book:

Beginning STM32 : Developing with FreeRTOS, libopencm3 and GCC

    ISBN: 978-1-4842-3623-9 Soft cover
          978-1-4842-3624-6 eBook

https://www.apress.com/us/book/9781484236239

This git repo contains libopencm3 and FreeRTOS+libopencm3 projects.
This provides a ready development environment for those who want to
apply the economical STM32F103 to using libopencm3 alone or in
concert with FreeRTOS.

THIS PROJECT ONLY USES OPEN SOURCE TOOLS
----------------------------------------

No Windows based IDE environments are used or implied! Cygwin, Linux,
MacOS and *BSD environments are suitable.

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
        |   |- *                miniblink files
        |- uart/                UART example (no flow control)
        |   |- *        
        |- uarthwfc/            UART example with hardware flow control
            |- *
        |- rtos/                FreeRTOS projects
            |- FreeRTOSv10.0.0  Unzipped FreeRTOS sources (you create this)
            |- Makefile         Used for creating new rtos projects
            |- Makefile.rtos    Rules for rtos project builds
            |- src/
            |   |- *            Files used for project creation
            |- libwwg
            |   |- Makefile
            |   |- include/*.h
            |   |- src/*.c
            |   |- libwwg.a
            |- blinky           Test FreeRTOS project
            |   |- *            Blinky project related files
            |   |- rtos
            |       |- *        RTOS Support files for blinky
            |- uart3            Another project..
            |   |- *
            |   |- rtos
            |       |- *
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
        there. It should create a subdirectory FreeRTOSv10.0.0 or 
        similar. If the release differs, you'll need to change the
        variable assignment in rtos/Project.mk FREERTOS ?= ....
        to match, or use the shell: export FREERTOS=whatever
    
    4.  Do NOT compile FreeRTOS, since portions of it will be copied
        to your project subdirectory for further customization. Each
        project is capable of running a different FreeRTOS configuration.

    5.  At the top level, do a "make" so that the libwwg and other
        projects get built.  Do a "make clobber" first, if you need
        to rebuild.

    All of this is explained in detail in the book.

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

All files covered by _this_ repo (_except_ those copied from libopencm3
and FreeRTOS, or otherwise marked), are covered by the GNU Lesser
General Public License Version 3. See the file named LICENSE.

Bill of Materials for Book:
---------------------------

1. ST-Link V2 Programmer
1. Breadboard
1. Dupont (Jumper) Wires
1. 0.1 uF Bypass caps (for use on breadboard)
1. USB TTL serial adapter
1. Power supply (on eBay MB102 Solderless Breadboard Power Supply Module 3.3 and 5V switchable)
1. Small assortment of LEDs
1. 220 ohm or SIP-9 resistor at 220 ohms for several LEDs
1. Get 3 x STM32 units if you want to do the full CAN project, but you can skimp with two if necessary.
1. Winbond W25Q32/W25Q64 flash chip (DIP)
1. PCF8574 GPIO extender (DIP)
1. OLED using SSD1306 controller (four wire SPI only, eBay)
1. RC servo(s) and/or scope (to control a servo)
1. RC servo controller(s) or 555 timer circuit to generate PWM to be read
1. Optional: Scope for examining clock signals for one chapter

Notes:
======

1. When compiling and you get the error message getline.c:5:20: fatal error: memory.h: No such file or directory, at the line where it is coded as "#include <memory.h>", you may have to change that to "#include <string.h>" instead. The compiler folks have sometimes moved the header file.

1. It has been reported that: Kubuntu 18.04 ships with arm-none-eabi-gcc (15:6.3.1+svn253039-1build1) 6.3.1 20170620, with this compiler the code does not work (creates problems for FreeRTOS). memcpy seems to be the problematic function call in the code, it is called by FreeRTOS when adding an element to the queue. (details in the FreeRTOS discussion on SourceForge)
