Currently, usbcdc.c is hanging in the usbd_init() routine. In the
local copy of usb_f107.c, the hang happens AFTER the blink(2) call:

	OTG_FS_GUSBCFG |= OTG_GUSBCFG_PHYSEL;

If that is commented out, then the hang occurs at:

        /* Wait for AHB idle. */
        while (!(OTG_FS_GRSTCTL & OTG_GRSTCTL_AHBIDL));

This smells like a necessary clock has not started yet.
