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
#include <unistd.h>
#include <sys/signal.h>
#include <mysql/mysql.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <libmpd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "serial.h"
#include "database.h"
#include "had.h"
#include "mpd.h"
#include "network.h"
#include "config.h"
#include "led_routines.h"


pthread_t threads[3];

int16_t lastTemperature[9][9][2];
int16_t lastVoltage[9];

static char *monthToName[12] = {"Jan","Feb","Mar","Apr","May",
	"Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static int killDaemon(int signal);
static int fileExists(const char *filename);
static void printUsage(void);
static void hadSignalHandler(int signal);

static int fileExists(const char *filename)
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

int decodeStream(char *buf,int *modul_id, int *sensor_id, int *celsius, int *decicelsius, int *voltage)
{
	char *trenner;

	trenner = (char*)strtok(buf,";");
	if(trenner)
		*modul_id = atoi(trenner);
	else
		return 0;
	
	trenner = (char*)strtok(NULL,";");
	if(trenner)
		*sensor_id = atoi(trenner);
	else
		return 0;

	trenner = (char*)strtok(NULL,";");
	if(trenner)
		*celsius = atoi(trenner);
	else
		return 0;

	trenner = (char*)strtok(NULL,";");
	if(trenner)
		*decicelsius = atoi(trenner);
	else
		return 0;
					
	if(*modul_id == 10) // Base station
		return *sensor_id;
		
	trenner = (char*)strtok(NULL,";");
	if(trenner)
		*voltage = atoi(trenner);
	else
		return 0;

	if(*modul_id == 7) // GLCD
		return *sensor_id;

	return 1;
}

static void printUsage(void)
{
	printf("Usage :\n\n");
	printf("had --help  this text\n");
	printf("had -s      start (default)\n");
	printf("had -k      kill the daemon\n");
	printf("had -r      reload config (does currently not reconnect db and mpd\n\n");
}

static int killDaemon(int signal)
{
	FILE *pid_file = fopen(config.pid_file,"r");

	int pid;

	if(!pid_file)
	{
		printf("Could not open %s. Maybe had is not running?\n",config.pid_file);
		return(EXIT_FAILURE);
	}
	fscanf(pid_file,"%d",&pid);
	fclose(pid_file);

	kill(pid,signal);
	return EXIT_SUCCESS;
}


int main(int argc, char* argv[])
{
	signal(SIGINT, (void*)hadSignalHandler);
	signal(SIGTERM, (void*)hadSignalHandler);
	int res;
	char buf[255];

	int modul_id,sensor_id,celsius,decicelsius,voltage;
	time_t rawtime;
	struct tm *ptm;
	pid_t pid;
	FILE *pid_file;
	
	if(!loadConfig(HAD_CONFIG_FILE))
	{
		verbose_printf(0,"Could not load config ... aborting\n\n");
		exit(EX_NOINPUT);
	}


	if(argc > 2)
	{
		printUsage();
		exit(EXIT_FAILURE);
	}
	
	if(argc >1)
	{
		if(!strcmp(argv[1],"--help"))
		{
			printUsage();
			exit(EXIT_SUCCESS);
		}

		if(!strcmp(argv[1],"-k"))
		{
			exit(killDaemon(SIGTERM));
		}

		/* reload config */
		if(!strcmp(argv[1],"-r"))
			exit(killDaemon(SIGHUP));
	}

	if(config.daemonize)
	{
		if(fileExists(config.pid_file))
		{
			printf("%s exists. Maybe had is still running?\n",config.pid_file);
			exit(EXIT_FAILURE);
		}

		if(( pid = fork() ) != 0 )
			exit(EX_OSERR);

		if(setsid() < 0)
			exit(EX_OSERR);
		
		signal(SIGHUP, SIG_IGN);
		
		if(( pid = fork() ) != 0 )
			exit(EX_OSERR);

		umask(0);
		
		signal(SIGHUP, (void*)hadSignalHandler);

		pid_file = fopen(config.pid_file,"w");
		if(!pid_file)
		{
			printf("Could not write %s\n",config.pid_file);
			exit(EXIT_FAILURE);
		}
		fprintf(pid_file,"%d\n",(int)getpid());
		fclose(pid_file);

		if(config.verbosity >= 9)
			printf("My PID is %d\n",(int)getpid());


		
		freopen(config.logfile, "a", stdout);
		freopen(config.logfile, "a", stderr);

		/* write into file without buffer */
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);

	}

	verbose_printf(0, "had gestartet\n");
	/* Inhalt des Arrays komplett mit 0 initialisieren */
	memset(graphP.temperature_history,0,115);
	memset(&rgbP,0,sizeof(rgbP)); // rgpP mit 0 initialisieren

	lastTemperature[3][1][0] = -1;
	lastTemperature[3][0][0] = -1;
	
	pthread_create(&threads[0],NULL,(void*)&mpdThread,NULL);	
	pthread_create(&threads[1],NULL,(void*)&networkThread,NULL);

//	if(config.led_matrix_activated)
//		pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);

	if(!initSerial(config.tty)) // serielle Schnittstelle aktivieren
	{
		verbose_printf(0,"Serielle Schnittstelle konnte nicht geoeffnet werden!\n");
		exit(EX_NOINPUT);
	}

	/* Falls keine Verbindung zum Mysql-Server aufgebaut werden kann, einfach
	 * immer wieder versuchen
	 */
	while(initDatabase() == -1)
	{
		sleep(10);
	}

	getLastTemperature(3,1,&celsius,&decicelsius);
	lastTemperature[3][1][0] = (int16_t)celsius;
	lastTemperature[3][1][1] = (int16_t)decicelsius;

	getLastTemperature(3,0,&celsius,&decicelsius);
	lastTemperature[3][0][0] = (int16_t)celsius;
	lastTemperature[3][0][1] = (int16_t)decicelsius;

	/* main loop */
	while (1) {
		memset(buf,0,sizeof(buf));
		res = readSerial(buf); // blocking read
		if(res>1)
		{
			verbose_printf(9,"Res=%d\n",res);
			time(&rawtime);
			ptm = gmtime(&rawtime);
			switch(decodeStream(buf,&modul_id,&sensor_id,&celsius,&decicelsius,&voltage))
			{
				case 1:
					verbose_printf(9,"%02d:%02d:%02d\t",ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
					verbose_printf(9,"Modul ID: %d\t",modul_id);
					verbose_printf(9,"Sensor ID: %d\t",sensor_id);
					verbose_printf(9,"Temperatur: %d,%d\t",celsius,decicelsius);
					switch(modul_id)
					{
						case 1: verbose_printf(9,"Spannung: %2.2f\r\n",ADC_MODUL_1/voltage); break;
						case 3: verbose_printf(9,"Spannung: %2.2f\r\n",ADC_MODUL_3/voltage); break;
						default: verbose_printf(9,"Spannung: %2.2f\r\n",ADC_MODUL_DEFAULT/voltage);
					}		
				
					//rawtime -= 32; // Modul misst immer vor dem Schlafengehen
					lastTemperature[modul_id][sensor_id][0] = (int16_t)celsius;
					lastTemperature[modul_id][sensor_id][1] = (int16_t)decicelsius;
					lastVoltage[modul_id] = voltage;
					databaseInsertTemperature(modul_id,sensor_id,celsius,decicelsius,ptm);
					break;
				
				case 2:
					ptm = localtime(&rawtime);
					glcdP.hour = ptm->tm_hour;
					glcdP.minute = ptm->tm_min;
					glcdP.second = ptm->tm_sec;
					glcdP.day = ptm->tm_mday;
					glcdP.month = ptm->tm_mon+1;
					glcdP.year = ptm->tm_year;
					glcdP.weekday = 0;
					glcdP.temperature[0] = lastTemperature[3][1][0]; // draussen
					glcdP.temperature[1] = lastTemperature[3][1][1]; 
					glcdP.temperature[2] = lastTemperature[3][0][0]; // schlaf
					glcdP.temperature[3] = lastTemperature[3][0][1];
//					glcdP.temperature[4] = lastTemperature[1][1][0]; // kuehl
//					glcdP.temperature[5] = lastTemperature[1][1][1];
//					glcdP.temperature[6] = lastTemperature[1][2][0];// gefrier
//					glcdP.temperature[7] = lastTemperature[1][2][1];
					if(fileExists("/had/wakeme"))
					{
						verbose_printf(9,"... Wecker aktiviert ...\n");
						glcdP.wecker = 1;
					}
					else
						glcdP.wecker = 0;
					sendPacket(&glcdP,GP_PACKET);
					verbose_printf(9,"GraphLCD Info Paket gesendet\r\n");
					break;
				case 3:
					getDailyGraph(celsius,decicelsius, &graphP);
					//sendPacket(&graphP,GRAPH_PACKET);
					verbose_printf(9,"Graph gesendet\r\n");
					break;
				case 4:	// MPD Packet request
					verbose_printf(9,"MPD Packet request\r\n");
					sendPacket(&mpdP,MPD_PACKET);
					break;
				case 5: // MPD prev
					verbose_printf(9,"MPD prev\r\n");
					mpdPrev();
					break;
				case 6: // MPD next song
					verbose_printf(9,"MPD next song\r\n");
					mpdNext();
					break;
				case 7: // RGB Packet request
					rgbP.headP.address = GLCD_ADDRESS;
					rgbP.headP.command = RGB_PACKET;
					sendPacket(&rgbP,RGB_PACKET);
					rgbP.headP.command = 0;
					break;
				case 8: // RGB Packet set
					rgbP.red = celsius;
					rgbP.green = decicelsius;
					rgbP.blue = voltage;
					rgbP.headP.address = 0x01;
					sendPacket(&rgbP, RGB_PACKET);
					rgbP.headP.address = 0x03;
					sendPacket(&rgbP, RGB_PACKET);
					break;
				case 10: verbose_printf(0,"Serial Modul hard-reset\r\n");
					 break;
				case 11: verbose_printf(0,"Serial Modul Watchdog-reset\r\n");
					 break;
				case 12: verbose_printf(0,"Serial Modul uart timeout\r\n");
					 break;
				case 13: mpdTogglePlayPause();
					 break;
				case 14: relaisP.port ^= 4;
					 sendPacket(&relaisP, RELAIS_PACKET);
					 if(relaisP.port & 4)
					 {
						 if(config.led_matrix_activated && !ledIsRunning())
							 pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
					 }
					 else
					 {
						 if(ledIsRunning())
							 stopLedMatrixThread();
					 }
					 break;

				case 0: //decode Stream failed
					verbose_printf(0,"decodeStream failed! Read line was: %s\r\n",buf);
					break;
			}
		} // endif res>1
				
	}
	return 0;
}

static void hadSignalHandler(int signal)
{
	if(signal == SIGTERM || signal == SIGINT)
	{
		if(config.daemonize)
			unlink(config.pid_file);
		pthread_kill(threads[1],SIGQUIT);
		verbose_printf(0,"Shutting down\n");
		exit(EXIT_SUCCESS);
	}
	else if(signal == SIGHUP)
	{
		verbose_printf(0,"Config reloaded\n");
		loadConfig(HAD_CONFIG_FILE);
	}
}

