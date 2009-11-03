/*
 * Copyright (C) 2007-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file	had.c
 * \brief	main file for had
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/signal.h>
#include <mysql/mysql.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <libmpd/libmpd.h>
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
#include "sms.h"
#include "version.h"
#include "hr20.h"

/*! thread variable array for network, mpd and ledmatrix */
pthread_t threads[5];
pthread_mutex_t mutexLedmatrix;
pthread_mutex_t mutexLedmatrixToggle;

/*! big array for the last measured temperatures */
int16_t lastTemperature[9][9][2];

/*! big array for the last measured voltages at the rf temperature modules */
int16_t lastVoltage[9];

/*! convert the number of a month to its abbreviation */
static char *monthToName[12] = {"Jan","Feb","Mar","Apr","May",
	"Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static int killDaemon(int signal);
static int fileExists(const char *filename);
static void printUsage(void);
static void hadSignalHandler(int signal);
static int decodeStream(char *buf,int *modul_id, int *sensor_id, int *celsius, int *decicelsius, int *voltage);
static void incrementColor(uint8_t *color);

/*!
 *******************************************************************************
 * check if a file exists
 * 
 * \returns 1 if file exists, 0 if not
 *******************************************************************************/
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

static int decodeStream(char *buf,int *modul_id, int *sensor_id, int *celsius, int *decicelsius, int *voltage)
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
	int database_status = 0;
	int gpcounter;
	int belowMinTemp = 0;
	struct _hr20info hr20info;
	int result;
	
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
	
#ifdef VERSION
	verbose_printf(0, "had %s started\n",VERSION);
#else
	verbose_printf(0, "had started\n");
#endif

	/* init mutex for ledmatrix thread */
	pthread_mutex_init(&mutexLedmatrix, NULL);
	pthread_mutex_init(&mutexLedmatrixToggle, NULL);

	if(loadStateFile(config.statefile))
	{
		verbose_printf(9, "Statefile successfully read\n");
		relaisP.port = hadState.relais_state;
	}
	else
	{
		verbose_printf(9, "Statefile could not be read, using default values\n");
		memset(&hadState, 0, sizeof(hadState));
		memset(&relaisP, 0, sizeof(relaisP));
		hadState.scrobbler_user_activated = config.scrobbler_activated;
		hadState.ledmatrix_user_activated = config.led_matrix_activated;
		hadState.beep_on_door_opened = 1;
		hadState.beep_on_window_left_open = 1;
	}


	/* Inhalt des Arrays komplett mit 0 initialisieren */
	memset(graphP.temperature_history,0,115);


	lastTemperature[3][1][0] = -1;
	lastTemperature[3][0][0] = -1;

	if(config.hr20_database_activated || config.serial_activated || config.usbtemp_activated)
		database_status = initDatabase();

	if(database_status == -1 && !config.serial_activated)
	{
		while((database_status = initDatabase()) == -1);
	}
	
	if(config.mpd_activated)
		pthread_create(&threads[0],NULL,(void*)&mpdThread,NULL);	

	pthread_create(&threads[1],NULL,(void*)&networkThread,NULL);

	if(config.led_matrix_activated && (relaisP.port & 4) && hadState.ledmatrix_user_activated)
	{
		pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
		pthread_detach(threads[2]);
	}

//	if(config.usbtemp_activated)
//		pthread_create(&threads[3],NULL,(void*)&usbTempLoop,NULL);
	if(config.hr20_database_activated)
		pthread_create(&threads[4],NULL,(void*)&hr20thread,NULL);
	
	if(config.serial_activated)
	{
		if(!initSerial(config.tty)) // serielle Schnittstelle aktivieren
		{
			verbose_printf(0,"Serielle Schnittstelle konnte nicht geoeffnet werden!\n");
			exit(EX_NOINPUT);
		}

		/* Falls keine Verbindung zum Mysql-Server aufgebaut werden kann, einfach
		 * immer wieder versuchen
		 *
		 * Schlecht, ohne mysql server funktioniert das RF->Uart garnicht mehr
		 */
		/*while(initDatabase() == -1)
		{
			sleep(10);
		}*/

		glcdP.backlight = 1;

		char buffer[1024];

		if(database_status != -1)
		{
			getLastTemperature(3,1,&celsius,&decicelsius);
			lastTemperature[3][1][0] = (int16_t)celsius;
			lastTemperature[3][1][1] = (int16_t)decicelsius;

			getLastTemperature(3,0,&celsius,&decicelsius);
			lastTemperature[3][0][0] = (int16_t)celsius;
			lastTemperature[3][0][1] = (int16_t)decicelsius;
		}

		sendBaseLcdText("had wurde gestartet ... ");

		/* main loop */
		while (1)
		{
			memset(buf,0,sizeof(buf));
			res = readSerial(buf); // blocking read
			if(res>1)
			{
				verbose_printf(9,"Res=%d\n",res);
				time(&rawtime);
				result = decodeStream(buf,&modul_id,&sensor_id,&celsius,&decicelsius,&voltage);
				if( result == 1)
				{
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
					if(database_status == -1)
						database_status = initDatabase();
					if(database_status != -1)
						databaseInsertTemperature(modul_id,sensor_id,celsius,decicelsius,rawtime);

					sprintf(buf,"Aussen:  %2d.%2d CInnen:   %2d.%2d C",
							lastTemperature[3][1][0],
							lastTemperature[3][1][1]/100,
							lastTemperature[3][0][0],
							lastTemperature[3][0][1]/100);
					sendBaseLcdText(buf);

					if(lastTemperature[3][0][0] < 15 && !belowMinTemp &&
							config.sms_activated)
					{
						char stringToSend[100];
						belowMinTemp = 1;
						sprintf(stringToSend,"%s had: Temperature is now %2d.%2d",theTime(),
								lastTemperature[3][0][0],
							lastTemperature[3][0][1]);
						sms(stringToSend);
					}
					else if(lastTemperature[3][0][0] > 16)
						belowMinTemp = 0;
				}
				else if(result == 2)
				{
					updateGlcd();
					verbose_printf(9,"GraphLCD Info Paket gesendet\r\n");
				}
				else if(result == 3)
				{
					getDailyGraph(celsius,decicelsius, &graphP);
					//sendPacket(&graphP,GRAPH_PACKET);
					verbose_printf(9,"Graph gesendet\r\n");
				}
				else if(result == 4)
				{
					verbose_printf(9,"MPD Packet request\r\n");
					sendPacket(&mpdP,MPD_PACKET);
				}
				else if(result == 10)
				{
					verbose_printf(0,"Serial Modul hard-reset\r\n");
					sendBaseLcdText("Modul neu gestartet ....");
				}
				else if(result == 11)
				{
					verbose_printf(0,"Serial Modul Watchdog-reset\r\n");
				}
				else if(result == 12)
				{
					verbose_printf(0,"Serial Modul uart timeout\r\n");
				}
				else if(result == 30) // 1 open
				{
					verbose_printf(1,"Door opened\n");
					hadState.input_state |= 1;
					/* check for opened window */
					if(hadState.input_state & 8)
					{
						verbose_printf(0,"Window and door open at the same time! BEEEEP\n");
						if(hadState.beep_on_window_left_open)
						{
							setBeepOn();
							sleep(1);
							setBeepOff();
						}
					}
					if(config.door_sensor_id && config.digital_input_module)
						databaseInsertTemperature(config.digital_input_module,
							config.door_sensor_id, 1, 0, rawtime);
				}
				else if(result == 31) // 1 closed
				{
					verbose_printf(1,"Door closed\n");
					hadState.input_state &= ~1;
					setBeepOff();
					if(config.door_sensor_id && config.digital_input_module)
						databaseInsertTemperature(config.digital_input_module,
							config.door_sensor_id, 0, 0, rawtime);
				}
				else if(result == 32) // 2 open
				{
				}
				else if(result == 33) // 2 closed
				{
				}
				else if(result == 34) // 2 open
				{
				}
				else if(result == 35) // 2 closed
				{
				}
				else if(result == 36)
				{
						verbose_printf(1,"Window closed\n");
						hadState.input_state &= ~8;
						setBeepOff();
						if(config.window_sensor_id && config.digital_input_module)
							databaseInsertTemperature(config.digital_input_module,
								config.window_sensor_id, 0, 0, rawtime);
						if(config.hr20_activated && hr20info.tempset >= 50
							&& hr20info.tempset <= 300)
						{
							hr20SetTemperature(hr20info.tempset);
						}
				}
				else if(result == 37)
				{
					verbose_printf(1,"Window opened\n");
					hadState.input_state |= 8;
					if(config.window_sensor_id && config.digital_input_module)
						databaseInsertTemperature(config.digital_input_module,
							config.window_sensor_id, 1, 0, rawtime);
					if(config.hr20_activated)
					{
						hr20GetStatus(&hr20info);
						hr20SetTemperature(50);
					}
				}
				else if(result == config.rkeys.mpd_random)
				{

					/* 50 - 82 reserved for remote control */
					mpdToggleRandom();
				}
				else if(result == config.rkeys.mpd_prev)
				{
						verbose_printf(9,"MPD prev\r\n");
						mpdPrev();
				}
				else if(result == config.rkeys.mpd_next)
				{
					verbose_printf(9,"MPD next song\r\n");
					mpdNext();
				}
				else if(result == config.rkeys.mpd_play_pause)
				{
					mpdTogglePlayPause();
				}
				else if(result == config.rkeys.music_on_hifi_on)
				{
					system(SYSTEM_MOUNT_MPD);
					relaisP.port |= 4;
					sendPacket(&relaisP, RELAIS_PACKET);
					if(relaisP.port & 4)
					{
						if(config.led_matrix_activated && !ledIsRunning())
						{
							pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
							pthread_detach(threads[2]);
							hadState.ledmatrix_user_activated = 1;
						}
					}
					hadState.relais_state = relaisP.port;
					mpdPlay();
				}
				else if(result == config.rkeys.everything_off)
				{
					relaisP.port = 0;
					sendPacket(&relaisP, RELAIS_PACKET);
					if(ledIsRunning())
						stopLedMatrixThread();
					hadState.relais_state = relaisP.port;
					mpdPause();
					for(gpcounter = 0; gpcounter < 3; gpcounter++)
					{
						hadState.rgbModuleValues[gpcounter].red = 0;
						hadState.rgbModuleValues[gpcounter].green = 0;
						hadState.rgbModuleValues[gpcounter].blue = 0;
					}
					setCurrentRgbValues();
					system(SYSTEM_KILL_MPD);
				}
				else if(result == config.rkeys.hifi_on_off)
				{
					relaisP.port ^= 4;
					sendPacket(&relaisP, RELAIS_PACKET);
					if(relaisP.port & 4)
					{
						if(config.led_matrix_activated && !ledIsRunning())
						{
							pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
							pthread_detach(threads[2]);
							hadState.ledmatrix_user_activated = 1;
						}
					}
					else
					{
						if(ledIsRunning())
							stopLedMatrixThread();
					}
					hadState.relais_state = relaisP.port;
				}
				else if(result == config.rkeys.brightlight)
				{
					relaisP.port ^= 32;
					sendPacket(&relaisP, RELAIS_PACKET);	
					hadState.relais_state = relaisP.port;
				}
				else if(result == config.rkeys.light_off[0] || result == config.rkeys.light_off[1])
				{
					for(gpcounter = 0; gpcounter < 3; gpcounter++)
					{
						hadState.rgbModuleValues[gpcounter].red = 0;
						hadState.rgbModuleValues[gpcounter].green = 0;
						hadState.rgbModuleValues[gpcounter].blue = 0;
					}
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.light_single_off[0])
				{
					hadState.rgbModuleValues[0].red = 0;
					hadState.rgbModuleValues[0].green = 0;
					hadState.rgbModuleValues[0].blue = 0;
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.light_single_off[1])
				{
					hadState.rgbModuleValues[1].red = 0;
					hadState.rgbModuleValues[1].green = 0;
					hadState.rgbModuleValues[1].blue = 0;
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.light_single_off[2])
				{
					hadState.rgbModuleValues[2].red = 0;
					hadState.rgbModuleValues[2].green = 0;
					hadState.rgbModuleValues[2].blue = 0;
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.light_on)
				{
					for(gpcounter = 0; gpcounter < 3; gpcounter++)
					{
						hadState.rgbModuleValues[gpcounter].red = 255;
						hadState.rgbModuleValues[gpcounter].green = 255 ;
						hadState.rgbModuleValues[gpcounter].blue = 0;
					}
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.red)
				{
					for(gpcounter = 0; gpcounter < 3; gpcounter++)
					{
						incrementColor(&hadState.rgbModuleValues[gpcounter].red);
					}
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.green)
				{
					for(gpcounter = 0; gpcounter < 3; gpcounter++)
					{
						incrementColor(&hadState.rgbModuleValues[gpcounter].green);
					}
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.blue)
				{
					for(gpcounter = 0; gpcounter < 3; gpcounter++)
					{
						incrementColor(&hadState.rgbModuleValues[gpcounter].blue);
					}
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.red_single[0])
				{
					incrementColor(&hadState.rgbModuleValues[0].red);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.red_single[1])
				{
					incrementColor(&hadState.rgbModuleValues[1].red);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.red_single[2])
				{
					incrementColor(&hadState.rgbModuleValues[2].red);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.green_single[0])
				{
					incrementColor(&hadState.rgbModuleValues[0].green);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.green_single[1])
				{
					incrementColor(&hadState.rgbModuleValues[1].green);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.green_single[2])
				{
					incrementColor(&hadState.rgbModuleValues[2].green);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.blue_single[0])
				{
					incrementColor(&hadState.rgbModuleValues[0].blue);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.blue_single[1])
				{
					incrementColor(&hadState.rgbModuleValues[1].blue);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.blue_single[2])
				{
					incrementColor(&hadState.rgbModuleValues[2].blue);
					setCurrentRgbValues();
				}
				else if(result == config.rkeys.ledmatrix_toggle)
				{
					pthread_mutex_lock(&mutexLedmatrixToggle);
					ledDisplayToggle();
					pthread_mutex_unlock(&mutexLedmatrixToggle);
				}
				else if(result == config.rkeys.open_door)
				{
					open_door();
				}
				else if(result == 0)
				{
					verbose_printf(0,"decodeStream failed! Read line was: %s\r\n",buf);
				}
			} // endif res>1
					
		}
	} // config.serial_activated
	else
	{
		verbose_printf(9,"Serial port deactivated\n");
		while(1)
		{
			sleep(1);
		}
	}
	return 0;
}

static void hadSignalHandler(int signal)
{
	if(signal == SIGTERM || signal == SIGINT)
	{
		if(config.daemonize)
			unlink(config.pid_file);
		networkThreadStop();
		writeStateFile(config.statefile);
		verbose_printf(0,"Shutting down\n");
		exit(EXIT_SUCCESS);
	}
	else if(signal == SIGHUP)
	{
		struct _config configTemp;
		memcpy(&configTemp, &config, sizeof(config));

		verbose_printf(0,"Config reloaded\n");
		loadConfig(HAD_CONFIG_FILE);

		// check if ledmatrix should be turned off
		if(configTemp.led_matrix_activated != config.led_matrix_activated)
		{
			if(config.led_matrix_activated)
			{
				if(!ledIsRunning() && (hadState.relais_state & 4))
				{
					pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
					pthread_detach(threads[2]);
				}
				hadState.ledmatrix_user_activated = 1;
			}
			else
				stopLedMatrixThread();
		}

	}
}

/*!
 *******************************************************************************
 * send data to the GLCD module connected to the base station
 *
 * the following data is transmitted: current date and time, last measured 
 * temperatures of outside and living room
 *******************************************************************************/
void updateGlcd()
{
	struct tm *ptm;
	time_t rawtime;
	
	time(&rawtime);
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
	
	glcdP.wecker = 0;
	sendPacket(&glcdP,GP_PACKET);
}

static void incrementColor(uint8_t *color)
{
	*color +=64;
	if(*color == 0)
		*color = 255;
	if(*color == 63)
		*color = 0;
}
