#ifndef __USBTEMP_H__
#define __USBTEMP_H__
#include <usb.h>

#define USBDEV_SHARED_VENDOR    0x16C0  /* VOTI */
#define USBDEV_SHARED_PRODUCT   0x05DC  /* Obdev's free shared PID */
extern void usbTempLoop();
#endif

