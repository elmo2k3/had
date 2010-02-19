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
//#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <glib.h>

#include "base_station.h"
#include "database.h"
#include "had.h"
#include "mpd.h"
#include "network.h"
#include "config.h"
#include "led_routines.h"
#include "sms.h"
#include "version.h"
#include "hr20.h"
#include "rfid_tag_reader.h"
#include "statefile.h"

/*! thread variable array for network, mpd and ledmatrix */
//pthread_t threads[5];
GMainLoop *had_main_loop;

/*! big array for the last measured temperatures */
int16_t lastTemperature[9][9][2];
/*! big array for the last measured voltages at the rf temperature modules */
int16_t lastVoltage[9];
static int database_status = 0;

static int killDaemon(int signal);
static void hadSignalHandler(int signal);
static void tag_read(struct RfidTagReader *tag_reader);
static void had_check_parameters(int argc, char **argv);
static void had_find_config(void);
static void had_check_daemonize(void);
static void had_print_version(void);
static void had_load_state(void);
static void had_init_hr20(void);
static void had_init_base_station(void);

int main(int argc, char* argv[])
{
	signal(SIGINT, (void*)hadSignalHandler);
	signal(SIGTERM, (void*)hadSignalHandler);
	struct RfidTagReader *tag_reader;
	struct NetworkServer *network_server; 

	g_thread_init(NULL);

	ledMatrixInitMutexes();
	had_find_config();
	had_check_parameters(argc,argv);
	had_check_daemonize();
	had_print_version();
	had_load_state();
	mpdInit();
	ledMatrixStart();
	had_init_hr20();
	had_init_base_station();

	lastTemperature[3][1][0] = -1;
	lastTemperature[3][0][0] = -1;

	tag_reader = rfid_tag_reader_new("/dev/ttyUSB1");
	rfid_tag_reader_set_callback(tag_reader, tag_read);
	network_server = network_server_new();

	had_main_loop = g_main_loop_new(NULL,FALSE);
	g_main_loop_run(had_main_loop);
	
	verbose_printf(0,"some cleanups missing\n");
	return 0;
}

static void hadSignalHandler(int signal)
{
	if(signal == SIGTERM || signal == SIGINT)
	{
		g_main_loop_quit(had_main_loop);
		if(config.daemonize)
			unlink(config.pid_file);
		//networkThreadStop();
		writeStateFile(config.statefile);
		verbose_printf(0,"Shutting down\n");
	}
	else if(signal == SIGHUP)
	{
		struct _config configTemp;
		memcpy(&configTemp, &config, sizeof(config));

		verbose_printf(0,"Config reloaded\n");
		loadConfig(HAD_CONFIG_FILE);

//		// check if ledmatrix should be turned off
//		if(configTemp.led_matrix_activated != config.led_matrix_activated)
//		{
//			if(config.led_matrix_activated)
//			{
//				if(!ledIsRunning() && (hadState.relais_state & 4))
//				{
//					////pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
//					//pthread_detach(threads[2]);
//				}
//				hadState.ledmatrix_user_activated = 1;
//			}
//			else
//				ledMatrixStop();
//		}

	}
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

static void tag_read(struct RfidTagReader *tag_reader)
{
	verbose_printf(0,"tag read: %s\n", rfid_tag_reader_last_tag(tag_reader));
}

static void had_check_parameters(int argc, char **argv)
{
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
}

static void had_find_config(void)
{
	if(loadConfig(HAD_CONFIG_FILE))
	{
		verbose_printf(0,"Using config %s\n",HAD_CONFIG_FILE);
	}
	else if(loadConfig("had.conf"))
	{
		verbose_printf(0,"Using config %s\n","had.conf");
	}
	else
		exit(EX_NOINPUT);
}

static void had_check_daemonize(void)
{
	pid_t pid;
	FILE *pid_file;
	
//	freopen("/dev/null", "a", stderr);

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
//		freopen("/dev/null", "a", stderr);

		/* write into file without buffer */
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);

	}
}

static void had_print_version(void)
{
#ifdef VERSION
	verbose_printf(0, "had %s started\n",VERSION);
#else
	verbose_printf(0, "had started\n");
#endif
}

static void had_load_state(void)
{
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
}

static void had_init_hr20(void)
{	
	if(config.hr20_database_activated)
	{
		g_timeout_add_seconds(300, hr20update, NULL);
	}
}

static void had_init_base_station(void)
{
	int celsius,decicelsius;
	if(config.serial_activated)
	{
		base_station_init(config.tty);
		
		glcdP.backlight = 1;

		getLastTemperature(3,1,&celsius,&decicelsius);
		lastTemperature[3][1][0] = (int16_t)celsius;
		lastTemperature[3][1][1] = (int16_t)decicelsius;

		getLastTemperature(3,0,&celsius,&decicelsius);
		lastTemperature[3][0][0] = (int16_t)celsius;
		lastTemperature[3][0][1] = (int16_t)decicelsius;

		sendBaseLcdText("had wurde gestartet ... ");

	} // config.serial_activated
	else
	{
		verbose_printf(9,"Serial port deactivated\n");
	}
}

