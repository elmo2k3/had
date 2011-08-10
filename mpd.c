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
#include "scrobbler.h"
#include "led_routines.h"
#include "base_station.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mpd"
    
static MpdObj *mpd;

static int isPlaying;
static guint submit_source = 0;
static guint now_playing_source = 0;

static void mpdStatusChanged(MpdObj *mi, ChangedStatusType what);
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
    g_warning("Error: %i : %s ", errorid, msg);
}

static gboolean mpdStatusUpdate(gpointer data)
{
    mpd_status_update(mpd);
    return TRUE;
}

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

    mpd_set_connection_timeout(mpd,10);
    g_timeout_add(500, mpdStatusUpdate, NULL);
    g_timeout_add_seconds(1, mpdCheckConnected, NULL);

    if(mpd_connect(mpd))
    {
        g_debug("Error connecting to mpd!");
        return 1;
    }
    isPlaying = mpd_player_get_state(mpd);
    return 0;
}

static void mpdStatusChanged(MpdObj *mi, ChangedStatusType what)
{
    guint shortest_time;
    static char led_matrix_text[255];
    
    mpd_Song *song = mpd_playlist_get_current_song(mi);
    isPlaying = mpd_player_get_state(mpd);

    if(what & MPD_CST_SONGID)
    {
        if(song)
        {
            snprintf(led_matrix_text, sizeof(led_matrix_text),
                "\r%s\a - \b%s", song->artist, song->title);
            ledMatrixSetMpdText(led_matrix_text);
            if(song->artist)
            {
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
                strncpy(current_track.last_title, song->title, sizeof(current_track.last_title));
                strncpy(mpdP.title, song->title, sizeof(mpdP.title));
            }
            if(song->track)
            {
                strncpy(current_track.current_track, song->track, sizeof(current_track));
            }
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
            if(base_station_hifi_is_on() && config.scrobbler_activated)
            {
                /* check if the track ran at least 2:40 or half of its runtime */
                if(song->artist && song->title)
                {
                    if(submit_source)
                    {
                        g_source_remove(submit_source);
                    }
                    submit_source = g_timeout_add_seconds(shortest_time,
                        submitTrack, NULL);

                    if(now_playing_source)
                    {
                        g_source_remove(now_playing_source);
                    }
                    now_playing_source = g_timeout_add_seconds(5,
                        submitNowPlaying, NULL);
                }
            }
            else
            {
                g_debug("Stereoanlage ist aus, kein Submit zu last.fm");
            }
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
            ledInsertFifo("Random on", 2, 1);
            mpdP.status |= MPD_RANDOM;
        }
        else
        {
            ledInsertFifo("Random off", 2, 1);
            mpdP.status &= ~MPD_RANDOM;
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

void mpdTogglePlayPause(void)
{
    if(mpdGetState() == MPD_PLAYER_PLAY)
        mpd_player_pause(mpd);
    else
        mpd_player_play(mpd);
}

void mpdPlay(void)
{
    mpd_player_play(mpd);
}

void mpdPause(void)
{
    mpd_player_pause(mpd);
}

void mpdNext(void)
{
    mpd_player_next(mpd);
}

void mpdPrev(void)
{
    mpd_player_prev(mpd);
}

void mpdPlayNumber(int number)
{
    mpd_player_play_id(mpd, number);
}

void mpdToggleRandom(void)
{
    if(mpd_player_get_random(mpd))
        mpd_player_set_random(mpd, 0);
    else
        mpd_player_set_random(mpd, 1);
}

static gboolean mpdCheckConnected(gpointer data)
{
    if(!mpd_check_connected(mpd))
    {
        if(!mpd_connect(mpd))
            g_debug("Connection to mpd successfully initiated!");
    }
    return TRUE;
}

static gboolean submitTrack(gpointer data)
{
    g_debug("Now submitting track %s", current_track.current_track);

    scrobblerSubmitTrack(current_track.last_artist, current_track.last_title,
        current_track.last_album, current_track.length, current_track.current_track,
        current_track.started_playing, 0);
    submit_source = 0;
    return FALSE;
}

static gboolean submitNowPlaying(gpointer data)
{
    g_debug("Now submitting now playing track %s", current_track.current_track);
    scrobblerSubmitTrack(current_track.last_artist, current_track.last_title,
        current_track.last_album, current_track.length, current_track.current_track,
        current_track.started_playing, 1);
    now_playing_source = 0;
    return FALSE;
}

