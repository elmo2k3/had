/*
 * Copyright (C) 2008-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file    scrobbler.c
 * \brief   audioscrobbler plugin
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
#ifdef ENABLE_LIBCRYPTO
#include <openssl/md5.h>
#endif

#include "had.h"
#include "scrobbler.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "scrobbler"


static char *scrobblerGetAuthHash(time_t timestamp);
static gboolean handshake_successfull = FALSE;

struct _scrobbler_data
{
    char session_id[50];
    char now_playing_url[70];
    char submission_url[70];
}scrobbler_data;

static int scrobblerHandshake(void)
{
#ifdef ENABLE_LIBCRYPTO
    time_t rawtime;
    /* String nur genau passend fuer Benutzer mit maximal 7 Zeichen! */
    char wgetString[300];
    char status[20];
    char *authHash;
    FILE *tmpFile;

    /* dirty hack. untraceable problems on mips otherwise */
    long long int copy_of_time;

    time(&rawtime);

    copy_of_time = (long long int)rawtime;

    authHash = scrobblerGetAuthHash((time_t)copy_of_time);
    sprintf(wgetString, SCROBBLER_HANDSHAKE_EXECUTE, config.scrobbler_tmpfile, config.scrobbler_user, copy_of_time, authHash);
    system(wgetString);
    free(authHash);

    tmpFile = fopen(config.scrobbler_tmpfile,"r");
    if(tmpFile)
    {
        fscanf(tmpFile,"%s\n%s\n%s\n%s",(char*)&status, 
            scrobbler_data.session_id, scrobbler_data.now_playing_url,
            scrobbler_data.submission_url);
        fclose(tmpFile);

//      unlink(config.scrobbler_tmpfile);
    }
    else
        g_warning("%s konnte nicht geoffnet werden",config.scrobbler_tmpfile);
    
    if(!strcmp(status,"OK"))
        return 1;
    else
    {
        g_warning("Fehler: %s",status);
        return 0;
    }
#else
    return 1;
#endif
}

static char *scrobblerGetAuthHash(time_t timestamp)
{
#ifdef ENABLE_LIBCRYPTO
    char executeString[160];
    unsigned char digest[MD5_DIGEST_LENGTH];
    int i;

    char *authHash = (char*)malloc(sizeof(char)*MD5_DIGEST_LENGTH*2+1);
    
    MD5((unsigned char*)config.scrobbler_pass, strlen(config.scrobbler_pass), digest);
    
    for(i=0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(&authHash[i*2],"%02x", digest[i]);
    }

    sprintf(executeString, "%s%qd", authHash, (long long int)timestamp);
    MD5((unsigned char*)executeString, strlen(executeString), digest);
    
    for(i=0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(&authHash[i*2],"%02x", digest[i]);
    }
    
    return authHash;
#else
    return NULL;
#endif
}

void scrobblerSubmitTrack
(char *artist, char *title, char *album,
int length, char *track, time_t started_playing, int isNowPlaying)
{
    static int fifo_low = 0, fifo_high = 0;
    char executeString[10][1024];
    char status[15];
    FILE *tmpFile;
    int chars;

    if(!handshake_successfull)
    {
        g_debug("handshaking");
        handshake_successfull = scrobblerHandshake();
    }
    
    if(isNowPlaying)
    {
        g_debug("submitting now playing");
        chars = snprintf(executeString[fifo_high],1023, SCROBBLER_NOW_PLAYING_EXECUTE,
            config.scrobbler_tmpfile, scrobbler_data.now_playing_url, scrobbler_data.session_id,
            artist, title, album, length, track);
    }
    else
    {
        g_debug("submitting track");
        chars = snprintf(executeString[fifo_high],1023,SCROBBLER_SUBMISSION_EXECUTE,
            config.scrobbler_tmpfile, scrobbler_data.submission_url, scrobbler_data.session_id,
            artist, title, album, length, track, (long long int)started_playing);
    }
    executeString[fifo_high][chars] = '\0';
    
    if(++fifo_high > (SCROBBLER_FIFO_SIZE -1)) fifo_high = 0;

    while( fifo_low != fifo_high )
    {
        system(executeString[fifo_low]);
        tmpFile = fopen(config.scrobbler_tmpfile, "r");
        if(tmpFile)
        {
            fscanf(tmpFile,"%s",(char*)&status);
            fclose(tmpFile);
//          unlink(config.scrobbler_tmpfile);
        }
        else
        {
            handshake_successfull = 0;
            break;
        }

        if(!strcmp(status,"OK"))
        {
            if(++fifo_low > (SCROBBLER_FIFO_SIZE - 1)) fifo_low = 0;
        }
        else
        {
            handshake_successfull = 0;
            g_warning("Fehler: %s",status);
            break; // dont try further
        }
    }
}

