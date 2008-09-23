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
#include "led_routines.h"
	
static MpdObj *mpd;

static char session_id[50],
	     now_playing_url[70],
	     submission_url[70];

struct _ledLine ledLineMpd;

static int isPlaying;

MpdObj *getMpdObject(void)
{
	return mpd;
}

int mpdGetState(void)
{
	return isPlaying;
}

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

	isPlaying = mpd_player_get_state(mpd);

	if(what & MPD_CST_SONGID)
	{
		time(&current_time);
		mpd_Song *song = mpd_playlist_get_current_song(mi);
		if(song)
		{
			clearScreen(&ledLineMpd);
			putString(song->artist,COLOR_RED,&ledLineMpd);
			putString(" - ",COLOR_AMBER,&ledLineMpd);
			putString(song->title,COLOR_GREEN,&ledLineMpd);

			/* Auf PIN4 liegt die Stereoanlage
			 * Nur wenn diese an ist zu last.fm submitten!
			 */
			if((relaisP.port & 4) && config.scrobbler_activated)
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

			if(song->artist && song->title)
			{
				strcpy(last_artist, song->artist);
				strcpy(last_title, song->title);
			}
			else
			{
				memset(last_artist,0,sizeof(last_artist));
				memset(last_title,0,sizeof(last_title));
			}
			if(song->album)
				strcpy(last_album, song->album);
			else
				memset(last_album,0,sizeof(last_album));
			if(song->track)
				strcpy(last_track, song->track);
			else
				memset(last_track,0,sizeof(last_track));

			last_time = song->time;
			/* First run! */
			if(!last_time_started)
				last_time_started = current_time - song->pos;
			else
				last_time_started = current_time;
			verbose_printf(9, "Song changed ...\n");
			
			sprintf(mpdP.currentSong,"%s - %s",song->artist,song->title);
			sendPacket(&mpdP,MPD_PACKET);			
		
		}
	}
}

void mpdTogglePlayStop(void)
{
	if(mpdGetState() & MPD_PLAYER_PLAY)
		mpd_player_stop(mpd);
	else
		mpd_player_play(mpd);
}

void mpdNext(void)
{
	mpd_player_next(mpd);
}

void mpdPrev(void)
{
	mpd_player_prev(mpd);
}

void mpdThread(void)
{
	int second_counter=0;

	/* Speicher reservieren */

	ledLineMpd.column_red = malloc(sizeof(uint16_t)*LINE_LENGTH);
	ledLineMpd.column_green = malloc(sizeof(uint16_t)*LINE_LENGTH);
	
	if(config.scrobbler_activated)
	{
		if(!scrobblerHandshake(session_id, now_playing_url, submission_url))
		{
			verbose_printf(0, "Scrobbler Handshake fehlgeschlagen\n");
		}
	}

	while(mpdInit() < 0)
	{
		sleep(10);
	}
	
	isPlaying = mpd_player_get_state(mpd);

	while(1)
	{
		/* Alle 10s checken ob die Verbindung zum MPD noch steht */
		if(second_counter++ == 90)
		{
			if(!mpd_check_connected(mpd))
				mpd_connect(mpd);
			second_counter = 0;
		}
		mpd_status_update(mpd);
		usleep(100000);
	}
	/* wird derzeit nie erreicht ... fuer spaeter */
	free(ledLineMpd.column_red);
	free(ledLineMpd.column_green);
}
