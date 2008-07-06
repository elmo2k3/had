#ifndef __MPD_H__
#define __MPD_H__

#include <libmpd.h>

extern void mpdStatusChanged(MpdObj *mi, ChangedStatusType what);
extern void mpdThread(MpdObj *mi);

#endif

