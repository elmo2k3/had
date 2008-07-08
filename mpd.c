#include <stdio.h>
#include <libmpd.h>
#include <stdlib.h>
#include <unistd.h>
#include "serial.h"
#include "mpd.h"
#include "main.h"
	
static MpdObj *mpd;

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
	if(what & MPD_CST_SONGID)
	{
		mpd_Song *song = mpd_playlist_get_current_song(mi);
		if(song)
		{
			sprintf(mpdP.currentSong,"%s - %s",song->artist,song->title);
			sendPacket(&mpdP,MPD_PACKET);			
		}
	}
}

void mpdThread(void)
{
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
