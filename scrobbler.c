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
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "had.h"
#include "scrobbler.h"

static char *scrobblerGetAuthHash(time_t timestamp);

int scrobblerHandshake(char *session_id, char *now_playing, char *submission)
{
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
		fscanf(tmpFile,"%s\n%s\n%s\n%s",(char*)&status, session_id, now_playing, submission);
		fclose(tmpFile);

//		unlink(config.scrobbler_tmpfile);
	}
	else
		verbose_printf(0, "%s konnte nicht geoffnet werden\n",config.scrobbler_tmpfile);
	
	if(!strcmp(status,"OK"))
		return 1;
	else
	{
		verbose_printf(0,"Fehler: %s\n",status);
		return 0;
	}
}

static char *scrobblerGetAuthHash(time_t timestamp)
{
	/* Nur passend fuer TMP_DIR Laenge max 4 */
	char executeString[160];

	char *authHash = (char*)malloc(sizeof(char)*40);
	FILE *md5File;

	sprintf(executeString, SCROBBLER_MD5_EXECUTE, config.scrobbler_hash, (long long int)timestamp);
	system(executeString);
	
	md5File = fopen(config.scrobbler_tmpfile,"r");
	fscanf(md5File,"%s",authHash);
	if(md5File)
	{
		fclose(md5File);
		unlink(config.scrobbler_tmpfile);
	}

	return authHash;
}

int scrobblerNowPlaying(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track)
{
	char executeString[1024];
	char status[15];
	FILE *tmpFile;

	sprintf(executeString, SCROBBLER_NOW_PLAYING_EXECUTE,
			config.scrobbler_tmpfile, url, session_id, artist, title, album, length, track);
	system(executeString);
	
	tmpFile = fopen(config.scrobbler_tmpfile, "r");

	if(tmpFile)
	{
		fscanf(tmpFile,"%s",(char*)&status);
		fclose(tmpFile);
		unlink(config.scrobbler_tmpfile);
	}
	else
		return 0;

	if(!strcmp(status,"OK"))
		return 1;
	else
	{
		verbose_printf(0, "Fehler: %s\n",status);
		return 0;
	}
}

int scrobblerSubmitTrack(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track, time_t started_playing)
{
	char executeString[1024];
	char status[15];
	FILE *tmpFile;

	sprintf(executeString, SCROBBLER_SUBMISSION_EXECUTE,
			config.scrobbler_tmpfile, url, session_id, artist, title, album, length, track, (long long int)started_playing);
	system(executeString);
	
	tmpFile = fopen(config.scrobbler_tmpfile, "r");

	if(tmpFile)
	{
		fscanf(tmpFile,"%s",(char*)&status);
		fclose(tmpFile);
		unlink(config.scrobbler_tmpfile);
	}
	else
		return 0;

	if(!strcmp(status,"OK"))
		return 1;
	else
	{
		verbose_printf(0, "Fehler: %s\n",status);
		return 0;
	}
}

