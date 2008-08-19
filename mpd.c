/*
 * Copyright (C) 2007-2008 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

#include <stdio.h>
#include <libmpd.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "serial.h"
#include "mpd.h"
#include "had.h"
#include "scrobbler.h"
	
static MpdObj *mpd;

static char session_id[50],
	     now_playing_url[70],
	     submission_url[70];

void mpdErrorCallback(MpdObj *mi, int errorid, char *msg, void *userdata)
{
	verbose_printf(0,"Error: %i : %s \n", errorid, msg);
}

int mpdInit(void)
{
	mpd = mpd_new(config.mpd_server,
			config.mpd_port,
			config.mpd_password);

	mpd_signal_connect_status_changed(mpd,(StatusChangedCallback)mpdStatusChanged, NULL);
	mpd_signal_connect_error(mpd,(ErrorCallback)mpdErrorCallback, NULL);

	mpd_set_connection_timeout(mpd,10);

	if(mpd_connect(mpd))
	{
		verbose_printf(0,"Error connecting to mpd!\n");
		return -1;
	}
	else
		return 1;
}

void mpdStatusChanged(MpdObj *mi, ChangedStatusType what)
{
	static char last_artist[30],
		    last_title[30],
		    last_album[30],
		    last_track[5];

	/* last_time = duration */
	static int last_time = 0;
	static time_t last_time_started = 0;

	time_t current_time;

	if(what & MPD_CST_SONGID)
	{
		time(&current_time);
		mpd_Song *song = mpd_playlist_get_current_song(mi);
		if(song)
		{
			/* Auf PIN4 liegt die Stereoanlage
			 * Nur wenn diese an ist zu last.fm submitten!
			 */
			if(relaisP.port & 4)
			{
				/* check if the track ran at least 2:40 or half of its runtime */
				if(last_time && last_time_started && 
						((last_time_started + 160) <= current_time ||
						 (last_time_started + (last_time/2)) <= current_time  )
						&& song->artist && song->title)
				{
					if(scrobblerSubmitTrack(submission_url, session_id, 
							last_artist, last_title, last_album,
							last_time, last_track, last_time_started))
					{
						verbose_printf(1,"%s - %s submitted to lastfm!\n", last_artist, last_title);
					}
					else
					{
						verbose_printf(0,"Submit fehlgeschlagen!\n");
						if(!scrobblerHandshake(session_id, now_playing_url, submission_url))
						{
							verbose_printf(0,"Scrobbler Handshake fehlgeschlagen\n");
						}
					}

				}

				if(scrobblerNowPlaying(now_playing_url, session_id,
						song->artist, song->title, song->album,
						song->time, song->track))
				{
					verbose_printf(9, "%s - %s now-playing submitted\n", song->artist, song->title);
				}
				else
				{
					verbose_printf(9, "Now-Playing fehlgeschlagen\n");
				}
			}
			else
			{
				verbose_printf(9, "Stereoanlage ist aus, kein Submit zu last.fm\n");
			}

			sprintf(mpdP.currentSong,"%s - %s",song->artist,song->title);
			sendPacket(&mpdP,MPD_PACKET);			

			if(song->artist)
				strcpy(last_artist, song->artist);
			if(song->title)
				strcpy(last_title, song->title);
			if(song->album)
				strcpy(last_album, song->album);
			if(song->track)
				strcpy(last_track, song->track);

			last_time = song->time;
			/* First run! */
			if(!last_time_started)
				last_time_started = current_time - song->pos;
			else
				last_time_started = current_time;
			verbose_printf(9, "Song changed ...\n");
		}
	}
}

void mpdThread(void)
{
	int second_counter=0;

	if(!scrobblerHandshake(session_id, now_playing_url, submission_url))
	{
		verbose_printf(0, "Scrobbler Handshake fehlgeschlagen\n");
	}
	
	while(mpdInit() < 0)
	{
		sleep(10);
	}
	while(1)
	{
		/* Alle 10s checken ob die Verbindung zum MPD noch steht */
		if(second_counter++ == 9)
		{
			if(!mpd_check_connected(mpd))
				mpd_connect(mpd);
			second_counter = 0;
		}
		mpd_status_update(mpd);
		sleep(1);
	}
}
