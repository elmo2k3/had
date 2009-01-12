/*
 * Copyright (C) 2008-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

#ifndef __SCROBBLER_H__
#define __SCROBBLER_H__

/** @file scrobbler.h
 * Audioscrobbler routines
 */

/* For info: http://www.audioscrobbler.net/development/protocol/ */

#define SCROBBLER_HANDSHAKE_EXECUTE "wget -O %s \"http://post.audioscrobbler.com/?hs=true&p=1.2&c=tst&v=1.0&u=%s&t=%qd&a=%s\" >> /dev/null 2>&1"

#define SCROBBLER_NOW_PLAYING_EXECUTE "wget -O %s %s --post-data \"s=%s&a=%s&t=%s&b=%s&l=%d&n=%s\" >> /dev/null 2>&1" 
#define SCROBBLER_SUBMISSION_EXECUTE "wget -O %s %s --post-data \"s=%s&a[0]=%s&t[0]=%s&b[0]=%s&l[0]=%d&n[0]=%s&i[0]=%qd&o[0]=P&r[0]=L&m[0]=\" >> /dev/null 2>&1" 

/** Initiate audioscrobbler connection
 *
 * Has to be called first
 *
 * @param *session_id session_id, which has to be used for all actions, will be put here
 * @param *now_playing URL for now-playing will be put here
 * @param *submission URL for submissions will be put here
 * 
 * @return 0 on failure, 1 on success
 */
extern int scrobblerHandshake(char *session_id, char *now_playing, char *submission);

/** Submit now-playing track
 *
 * Will be shown as now-playing. No database insert
 */
extern int scrobblerNowPlaying(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track);

/** Submit tack
 *
 * Track will be inserted into database
 */
extern int scrobblerSubmitTrack(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track, time_t started_playing);

#endif

