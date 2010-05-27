/*
 * Copyright (C) 2007-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*!
 * \file    mpd.h
 * \brief   header for mpd communication
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/


#ifndef __MPD_H__
#define __MPD_H__

#include <libmpd/libmpd.h>


extern struct _ledLine ledLineMpd;

/** 
 * MPD main thread
 * */
extern void mpdThread(void);
int mpdInit(void);

/** 
 * @return current mpd state. See libmpd for details
 */
extern int mpdGetState(void);

/** Toggle play/pause
 */
extern void mpdTogglePlayPause(void);

/** Next song
 */
extern void mpdNext(void);

/** Previous song */
extern void mpdPrev(void);

/** toggle random */
extern void mpdToggleRandom(void);

/** play specified song
 * \param number number of song in current playlist
 */
extern void mpdPlayNumber(int number);

extern void mpdPlay();
extern void mpdPause();


#endif

