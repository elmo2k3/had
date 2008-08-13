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

#include "serial.h"
#include "database.h"
#include "main.h"
#include "mpd.h"
#include "network.h"

pthread_t threads[2];

volatile signed char lastTemperature[9][9][2];

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
					
	if(*modul_id == 7 || *modul_id == 10) //GLCD Modul
		return *sensor_id;
		
	trenner = (char*)strtok(NULL,";");
	if(trenner)
		*voltage = atoi(trenner);
	else
		return 0;
	return 1;
}

struct graphPacket graphP;

int main(int argc, char* argv[])
{
	signal(SIGINT, (void*)hadSIGINT);

	int res;
	char buf[255];

	int modul_id,sensor_id,celsius,decicelsius,voltage;
	time_t rawtime;
	struct tm *ptm;


	/* Inhalt des Arrays komplett mit 0 initialisieren */
	memset(graphP.temperature_history,0,115);
	memset(&rgbP,0,sizeof(rgbP)); // rgpP mit 0 initialisieren

	lastTemperature[3][1][0] = -1;
	lastTemperature[3][0][0] = -1;
	
	pthread_create(&threads[0],NULL,(void*)&mpdThread,NULL);	
	pthread_create(&threads[1],NULL,(void*)&networkThread,NULL);	

	if(initSerial(argv[1]) < 0) // serielle Schnittstelle aktivieren
	{
		printf("Serielle Schnittstelle konnte nicht geoeffnet werden!\n");
		exit(-1);
	}

	/* Falls keine Verbindung zum Mysql-Server aufgebaut werden kann, einfach
	 * immer wieder versuchen
	 */
	while(initDatabase() == -1)
	{
		sleep(10);
	}

	getLastTemperature(3,1,&celsius,&decicelsius);
	lastTemperature[3][1][0] = (char)celsius;
	lastTemperature[3][1][1] = (char)decicelsius;

	getLastTemperature(3,0,&celsius,&decicelsius);
	lastTemperature[3][0][0] = (char)celsius;
	lastTemperature[3][0][1] = (char)decicelsius;

	while (1) {
		res = readSerial(buf); // blocking read
		if(res>1)
		{
			printf("Res=%d\t",res);
			time(&rawtime);
			ptm = gmtime(&rawtime);
			switch(decodeStream(buf,&modul_id,&sensor_id,&celsius,&decicelsius,&voltage))
			{
				case 1:
					printf("%02d:%02d:%02d\t",ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
					printf("Modul ID: %d\t",modul_id);
					printf("Sensor ID: %d\t",sensor_id);
					printf("Temperatur: %d,%d\t",celsius,decicelsius);
					switch(modul_id)
					{
						case 1: printf("Spannung: %2.2f\r\n",ADC_MODUL_1/voltage); break;
						case 3: printf("Spannung: %2.2f\r\n",ADC_MODUL_3/voltage); break;
						default: printf("Spannung: %2.2f\r\n",ADC_MODUL_DEFAULT/voltage);
					}		
				
					//rawtime -= 32; // Modul misst immer vor dem Schlafengehen
					lastTemperature[modul_id][sensor_id][0] = celsius;
					lastTemperature[modul_id][sensor_id][1] = decicelsius/625;
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
					glcdP.temperature[4] = lastTemperature[1][1][0]; // kuehl
					glcdP.temperature[5] = lastTemperature[1][1][1];
					glcdP.temperature[6] = lastTemperature[1][2][0];// gefrier
					glcdP.temperature[7] = lastTemperature[1][2][1];
					if(fileExists("/had/wakeme"))
					{
						printf("... Wecker aktiviert ...");
						glcdP.wecker = 1;
					}
					else
						glcdP.wecker = 0;
					sendPacket(&glcdP,GP_PACKET);
					printf("GraphLCD Info Paket gesendet\r\n");
					break;
				case 3:
					getDailyGraph(celsius,decicelsius, &graphP);
					//sendPacket(&graphP,GRAPH_PACKET);
					printf("Graph gesendet\r\n");
					break;
				case 4:	// MPD Packet request
					printf("MPD Packet request\r\n");
					sendPacket(&mpdP,MPD_PACKET);
					break;
				case 5: // MPD prev song
					printf("MPD prev song\r\n");
					//mpd_player_prev(mpd);
					break;
				case 6: // MPD next song
					printf("MPD next song\r\n");
					//mpd_player_next(mpd);
					break;
				case 10: printf("Serial Modul hard-reset\r\n");
					 break;
				case 11: printf("Serial Modul Watchdog-reset\r\n");
					 break;
				case 12: printf("Serial Modul uart timeout\r\n");
					 break;
				case 0: //decode Stream failed
					printf("decodeStream failed!\r\n");
					break;
			}
		} // endif res>1
				
	}
	return 0;
}

void hadSIGINT(void)
{
	pthread_kill(threads[1],SIGQUIT);
	printf("Shutting down\n");
	exit(0);
}

