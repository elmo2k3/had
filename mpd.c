#include <stdio.h>
#include <libmpd.h>
#include <stdlib.h>
#include <unistd.h>
#include "serial.h"
#include "mpd.h"
#include "main.h"

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

void mpdThread(MpdObj *mi)
{
	while(1)
	{
		mpd_status_update(mi);
		sleep(1);
	}
}
