#ifndef __MPD_H__
#define __MPD_H__

#include <libmpd.h>

extern int mpdInit(void);
extern void mpdStatusChanged(MpdObj *mi, ChangedStatusType what);
extern void mpdThread(void);

#endif

