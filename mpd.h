#ifndef __MPD_H__
#define __MPD_H__

#include <libmpd.h>


void mpdStatusChanged(MpdObj *mi, ChangedStatusType what);
void mpdThread(MpdObj *mi);

#endif

