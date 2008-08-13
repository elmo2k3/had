#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "main.h"
#include "scrobbler.h"

static char *scrobblerGetAuthHash(time_t timestamp);

/* Gibt 1 bei Erfolg, 0 bei Misserfolg zurueck 
 * session_id: Session ID die fuer weitere Aktionen zu benutzen ist
 * now_playing: URL fuer now_playing
 * submission: URL fuer submissions
 */
int scrobblerHandshake(char *session_id, char *now_playing, char *submission)
{
	time_t rawtime;
	/* String nur genau passend fuer Benutzer mit maximal 7 Zeichen! */
	char wgetString[300];
	char status[20];
	char *authHash;
	FILE *tmpFile;

	time(&rawtime);
	authHash = scrobblerGetAuthHash(rawtime);
	sprintf(wgetString, SCROBBLER_HANDSHAKE_EXECUTE, SCROBBLER_TMP_FILE, SCROBBLER_USER, (long long int)rawtime, authHash);
	system(wgetString);
	free(authHash);
	
	tmpFile = fopen(SCROBBLER_TMP_FILE,"r");
	if(tmpFile)
	{
		fscanf(tmpFile,"%s\n%s\n%s\n%s",(char*)&status, session_id, now_playing, submission);
		fclose(tmpFile);

		unlink(SCROBBLER_TMP_FILE);
	}
	else
		printf("%s konnte nicht geoffnet werden\n",SCROBBLER_TMP_FILE);
	
	if(!strcmp(status,"OK"))
		return 1;
	else
	{
		printf("Fehler: %s\n",status);
		return 0;
	}
}

static char *scrobblerGetAuthHash(time_t timestamp)
{
	/* Nur passend fuer TMP_DIR Laenge max 4 */
	char executeString[80];

	char *authHash = (char*)malloc(sizeof(char)*40);
	FILE *md5File;

	sprintf(executeString, SCROBBLER_MD5_EXECUTE, SCROBBLER_PASS_HASH, (int64_t)timestamp);
	system(executeString);
	
	md5File = fopen(SCROBBLER_TMP_FILE,"r");
	fscanf(md5File,"%s",authHash);
	if(md5File)
	{
		fclose(md5File);
		unlink(SCROBBLER_TMP_FILE);
	}

	return authHash;
}

int scrobblerNowPlaying(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track)
{
	char executeString[1024];
	char status[15];
	FILE *tmpFile;

	sprintf(executeString, SCROBBLER_NOW_PLAYING_EXECUTE,
			SCROBBLER_TMP_FILE, url, session_id, artist, title, album, length, track);
	system(executeString);
	
	tmpFile = fopen(SCROBBLER_TMP_FILE, "r");

	if(tmpFile)
	{
		fscanf(tmpFile,"%s",(char*)&status);
		fclose(tmpFile);
		unlink(SCROBBLER_TMP_FILE);
	}
	else
		return 0;

	if(!strcmp(status,"OK"))
		return 1;
	else
	{
		printf("Fehler: %s\n",status);
		return 0;
	}
}

int scrobblerSubmitTrack(char *url, char *session_id, char *artist, char *title, char *album, int length, char *track, time_t started_playing)
{
	char executeString[1024];
	char status[15];
	FILE *tmpFile;

	sprintf(executeString, SCROBBLER_SUBMISSION_EXECUTE,
			SCROBBLER_TMP_FILE, url, session_id, artist, title, album, length, track, (long long int)started_playing);
	system(executeString);
	
	tmpFile = fopen(SCROBBLER_TMP_FILE, "r");

	if(tmpFile)
	{
		fscanf(tmpFile,"%s",(char*)&status);
		fclose(tmpFile);
		unlink(SCROBBLER_TMP_FILE);
	}
	else
		return 0;

	if(!strcmp(status,"OK"))
		return 1;
	else
	{
		printf("Fehler: %s\n",status);
		return 0;
	}
}

