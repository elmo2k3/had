#include <stdio.h>
#include <libmpd.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "serial.h"
#include "mpd.h"
#include "main.h"
#include "scrobbler.h"
	
static MpdObj *mpd;

static char session_id[50],
	     now_playing_url[70],
	     submission_url[70];

void mpdErrorCallback(MpdObj *mi, int errorid, char *msg, void *userdata)
{
	printf("Error: %i : %s ", errorid, msg);
}

int mpdInit(void)
{
	int iport = 6600;
	char *hostname = getenv("MPD_HOST");
	char *port = getenv("MPD_PORT");
	char *password = getenv("MPD_PASSWORD");
	
	if(!hostname)
		hostname = "localhost";
	if(port)
		iport = atoi(port);

	mpd = mpd_new(hostname,iport,password);

	mpd_signal_connect_status_changed(mpd,(StatusChangedCallback)mpdStatusChanged, NULL);
	mpd_signal_connect_error(mpd,(ErrorCallback)mpdErrorCallback, NULL);

	mpd_set_connection_timeout(mpd,60);

	if(mpd_connect(mpd))
	{
		printf("Error connecting to mpd!\n");
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
			if(last_time && last_time_started && 
					((last_time_started + 240) <= current_time ||
					 (last_time_started + (last_time/2)) <= current_time  ))
			{
				if(scrobblerSubmitTrack(submission_url, session_id, 
						last_artist, last_title, last_album,
						last_time, last_track, last_time_started))
					printf("%s - %s submitted to lastfm!\n", last_artist, last_title);
				else
					printf("Submit fehlgeschlagen!\n");

			}

			if(scrobblerNowPlaying(now_playing_url, session_id,
					song->artist, song->title, song->album,
					song->time, song->track))
				printf("%s - %s now-playing submitted\n", song->artist, song->title);
			else
				printf("Now-Playing fehlgeschlagen\n");

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
			last_time_started = current_time - song->pos;
			printf("Song changed ...\n");
		}
	}
}

void mpdThread(void)
{

	if(!scrobblerHandshake(session_id, now_playing_url, submission_url))
		printf("Scrobbler Handshake fehlgeschlagen\n");
	
	while(mpdInit() < 0)
	{
		sleep(10);
	}
	while(1)
	{
		mpd_status_update(mpd);
		sleep(1);
	}
}
