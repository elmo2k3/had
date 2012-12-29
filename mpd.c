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
 * \file    mpd.c
 * \brief   mpd communication functions
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "mpd.h"
#include "had.h"
#include "led_routines.h"
#include "base_station.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mpd"

#ifdef ENABLE_LIBMPD
static MpdObj *mpd;
static void mpdStatusChanged(MpdObj *mi, ChangedStatusType what);
#endif

static int isPlaying;
static guint submit_source = 0;
static guint now_playing_source = 0;
static gboolean israndom = 0;

static gboolean mpdCheckConnected(gpointer data);
static gboolean submitTrack(gpointer data);
static gboolean submitNowPlaying(gpointer data);

struct _track_information
{
    char last_artist[30];
    char last_title[30];
    char last_album[30];
    char current_track[5];

    time_t started_playing;
    int length;
}current_track;

int mpdGetState(void)
{
    return isPlaying;
}

int mpdGetRandom(void)
{
    return israndom;
}

#ifdef ENABLE_LIBMPD
static gboolean mpdStatusUpdate(gpointer data)
{
    mpd_status_update(mpd);
    return TRUE;
}

static void mpdStatusChanged(MpdObj *mi, ChangedStatusType what)
{
    guint shortest_time;
    static struct _artist_title artist_title;
    
    mpd_Song *song = mpd_playlist_get_current_song(mi);
    isPlaying = mpd_player_get_state(mpd);

    if(what & MPD_CST_SONGID)
    {
        if(song)
        {
            strcpy(artist_title.artist, "No Artist");
            strcpy(artist_title.title, "No Title");
            if(song->artist)
            {
                strcpy(artist_title.artist, song->artist);
                strncpy(current_track.last_artist, song->artist, sizeof(current_track.last_artist));
                strncpy(mpdP.artist, song->artist, sizeof(mpdP.artist));
            }
            if(song->album)
            {
                strncpy(current_track.last_album, song->album, sizeof(current_track.last_album));
                strncpy(mpdP.album, song->album, sizeof(mpdP.album));
            }
            if(song->title)
            {
                strcpy(artist_title.title, song->title);
                strncpy(current_track.last_title, song->title, sizeof(current_track.last_title));
                strncpy(mpdP.title, song->title, sizeof(mpdP.title));
            }
            if(song->track)
            {
                strncpy(current_track.current_track, song->track, sizeof(current_track));
            }
            ledMatrixSetMpdText(&artist_title);
            current_track.length = song->time;
            current_track.started_playing = time(NULL);
            mpdP.length = song->time;
            mpdP.pos = mpd_status_get_elapsed_song_time(mpd);

            if(song->time / 2 < 240)
                shortest_time = song->time / 2 + 5;
            else
                shortest_time = 240;

            /* Auf PIN4 liegt die Stereoanlage
             * Nur wenn diese an ist zu last.fm submitten!
             */
            sendPacket(&mpdP, MPD_PACKET);
        }
    }
    if(what & MPD_CST_ELAPSED_TIME)
    {
        mpdP.pos = mpd_status_get_elapsed_song_time(mpd);
    }
    if(what & MPD_CST_RANDOM)
    {
        if(mpd_player_get_random(mpd))
        {
            ledFifoInsert("Random on", 2, 1);
            mpdP.status |= MPD_RANDOM;
            israndom = 1;
        }
        else
        {
            ledFifoInsert("Random off", 2, 1);
            mpdP.status &= ~MPD_RANDOM;
            israndom = 0;
        }
        sendPacket(&mpdP, MPD_PACKET);
    }
    if(what & MPD_CST_STATE)
    {
        if(isPlaying == MPD_PLAYER_PLAY)
            mpdP.status |= MPD_PLAYING;
        else
            mpdP.status &= ~MPD_PLAYING;    
        sendPacket(&mpdP, MPD_PACKET);
    }
}

static gboolean mpdCheckConnected(gpointer data)
{
    if(!mpd_check_connected(mpd))
    {
        if(!mpd_connect(mpd))
        {
            g_debug("Connection to mpd successfully initiated!");
            mpd_set_connection_timeout(mpd,5); // higher timeout for not losing connection
        }
        else
        {
            mpd_set_connection_timeout(mpd,0.5); // lower timeout for less blocking
        }
    }
    return TRUE;
}

#endif // ENABLE_LIBMPD

#ifdef ENABLE_LIBMPD
int mpdInit(void)
{
    if(!config.mpd_activated)
    {
        return 1;
    }


    mpd = mpd_new(config.mpd_server,
            config.mpd_port,
            config.mpd_password);

    mpd_signal_connect_status_changed(mpd,(StatusChangedCallback)mpdStatusChanged, NULL);

    mpd_set_connection_timeout(mpd,5);
    g_timeout_add(2000, mpdStatusUpdate, NULL);
    g_timeout_add_seconds(20, mpdCheckConnected, NULL);

    if(mpd_connect(mpd))
    {
        g_debug("Error connecting to mpd!");
        return 1;
    }
    isPlaying = mpd_player_get_state(mpd);
    return 0;
}
#else
int mpdInit(void)
{
    return 0;
}
#endif



void mpdTogglePlayPause(void)
{
#ifdef ENABLE_LIBMPD
    if(mpdGetState() == MPD_PLAYER_PLAY)
        mpd_player_pause(mpd);
    else
        mpd_player_play(mpd);
#endif
}

void mpdPlay(void)
{
#ifdef ENABLE_LIBMPD
    mpd_player_play(mpd);
#endif
}

void mpdPause(void)
{
#ifdef ENABLE_LIBMPD
    mpd_player_pause(mpd);
#endif
}

void mpdNext(void)
{
#ifdef ENABLE_LIBMPD
    mpd_player_next(mpd);
#endif
}

void mpdPrev(void)
{
#ifdef ENABLE_LIBMPD
    mpd_player_prev(mpd);
#endif
}

void mpdPlayNumber(int number)
{
#ifdef ENABLE_LIBMPD
    mpd_player_play_id(mpd, number);
#endif
}

void mpdToggleRandom(void)
{
#ifdef ENABLE_LIBMPD
    if(mpd_player_get_random(mpd))
        mpd_player_set_random(mpd, 0);
    else
        mpd_player_set_random(mpd, 1);
#endif
}


