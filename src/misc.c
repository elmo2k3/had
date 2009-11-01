#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#include "daemon.h"
#include "misc.h"

/*! convert the number of a month to its abbreviation */
char *monthToName[12] = {"Jan","Feb","Mar","Apr","May",
	"Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

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

int parseCmdLine(int argc, char **argv)
{
	if(argc > 2)
	{
		printUsage();
		return(EXIT_FAILURE);
	}
	
	if(argc >1)
	{
		if(!strcmp(argv[1],"--help"))
		{
			printUsage();
			return(EXIT_SUCCESS);
		}

		if(!strcmp(argv[1],"-k"))
		{
			return(killDaemon(SIGTERM));
		}

		/* reload config */
		if(!strcmp(argv[1],"-r"))
			return(killDaemon(SIGHUP));
	}
	return -1;
}

