#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include "string.h"
#include "misc.h"
#include "had.h"

/*!
 *******************************************************************************
 * check if a file exists
 * 
 * \returns 1 if file exists, 0 if not
 *******************************************************************************/
int fileExists(const char *filename)
{
	FILE *fp = fopen(filename,"r");
	if(fp)
	{
		fclose(fp);
		return 1;
	}
	else
		return 0;
}

/*!
 *******************************************************************************
 * get the current time and date in a specific format
 * 
 * this function is _not_ thread safe! the returned value is an internally statically
 * allocated char array that will be overwritten by every call to this function
 * this function is to be used by the verbose_printf macro
 *
 * \returns the date and time in format Dec 1 12:31:19
 *******************************************************************************/
char *theTime(void)
{
	static char returnValue[9];
	time_t currentTime;
	struct tm *ptm;

    /*! convert the number of a month to its abbreviation */
    static char *monthToName[12] = {"Jan","Feb","Mar","Apr","May",
	    "Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

	time(&currentTime);

	ptm = localtime(&currentTime);

	sprintf(returnValue,"%s %2d %02d:%02d:%02d",monthToName[ptm->tm_mon], ptm->tm_mday, 
			ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	return returnValue;
}

void printUsage(void)
{
	printf("Usage :\n\n");
	printf("had --help  this text\n");
	printf("had -s      start (default)\n");
	printf("had -k      kill the daemon\n");
	printf("had -r      reload config (does currently not reconnect db and mpd\n\n");
}

/* the following function is a "copy&paste" from mpd */
int buffer2array(char *buffer, char *array[], const int max)
{
	int i = 0;
	char *c = buffer;

	while (*c != '\0' && i < max) {
		if (*c == '\"') {
			array[i++] = ++c;
			while (*c != '\0') {
				if (*c == '\"') {
					*(c++) = '\0';
					break;
				}
				else if (*(c++) == '\\' && *c != '\0') {
					memmove(c - 1, c, strlen(c) + 1);
				}
			}
		} else {
			c = g_strchug(c);
			if (*c == '\0')
				return i;

			array[i++] = c++;

			while (!g_ascii_isspace(*c) && *c != '\0')
				++c;
		}
		if (*c == '\0')
			return i;
		*(c++) = '\0';

		c = g_strchug(c);
	}
	return i;
}

void had_log_handler
(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
gpointer user_data)
{
	if (log_level & G_LOG_LEVEL_DEBUG) {
		verbose_printf(9,"%s-Message: %s\n",log_domain,message);
	} else {
		if (log_domain) {
			verbose_printf(0,"%s-Message: %s\n",log_domain,message);
		} else {
			verbose_printf(0,"%s\n",message);
		}
	}
}
