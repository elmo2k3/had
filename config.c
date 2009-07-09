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
* \file	config.c
* \brief	config file handling
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "had.h"

#define NUM_PARAMS 40
static char *config_params[NUM_PARAMS] = { "db_db", "db_server", "db_user", "db_pass",
	"db_port", "mpd_server", "mpd_pass", "mpd_port", "scrobbler_user", 
	"scrobbler_pass", "scrobbler_tmpfile", "logfile", "verbosity", "daemonize",
	"tty","led_matrix_ip","led_matrix_port","led_matrix_activated","scrobbler_activated",
	"pid_file","led_matrix_shift_speed","statefile","serial_activated",
	"sms_activated","sipgate_user","sipgate_pass","cellphone","hr20_activated","hr20_port",
	"mpd_activated","usbtemp_activated","usbtemp_device_id","usbtemp_device_module","usbtemp_device_sensor",
	"hr20_database_activated","hr20_database_number","door_sensor_id","window_sensor_id",
	"digital_input_module","password"};


int loadConfig(char *conf)
{
	FILE *config_file;
	char line[120];
	char value[100];
	char *lpos;
	int param;

	config_file = fopen(conf,"r");
	if(!config_file)
	{
		return 0;
	}

	/* set everything to zero */
	memset(&config, 0, sizeof(config));
	
	/* default values */
	config.database_port = MYSQL_PORT;
	config.mpd_port = MPD_PORT;
	strcpy(config.pid_file, PID_FILE);
	strcpy(config.logfile,LOG_FILE);
	config.led_shift_speed = 15000;
	config.serial_activated = 0;

	config.rkeys.backup = 50;
	config.rkeys.restore = 58;
	config.rkeys.light_on = 66;
	config.rkeys.light_off[0] = 74;
	config.rkeys.mpd_play_pause = 51;
	config.rkeys.mpd_random = 59;
	config.rkeys.mpd_prev = 67;
	config.rkeys.mpd_next = 75;
	config.rkeys.hifi_on_off = 52;
	config.rkeys.brightlight = 60;
	config.rkeys.red = 53;
	config.rkeys.green = 61;
	config.rkeys.blue = 69;
	config.rkeys.light_off[1] = 77;
	config.rkeys.light_single_off[0] = 78;
	config.rkeys.light_single_off[1] = 79;
	config.rkeys.light_single_off[2] = 80;
	config.rkeys.red_single[0] = 54;
	config.rkeys.red_single[1] = 55;
	config.rkeys.red_single[2] = 56;
	config.rkeys.green_single[0] = 62;
	config.rkeys.green_single[1] = 63;
	config.rkeys.green_single[2] = 64;
	config.rkeys.blue_single[0] = 70;
	config.rkeys.blue_single[1] = 71;
	config.rkeys.blue_single[2] = 72;
	config.rkeys.music_on_hifi_on = 100;
	config.rkeys.everything_off = 99;

	/* step through every line */
	while(fgets(line, sizeof(line), config_file) != NULL)
	{
		/* skip comments and empty lines */
		if(line[0] == '#' || line[0] == '\n')
			continue;
		for(param = 0; param < NUM_PARAMS; param++)
		{
			/* if param name not at the beginning of line */
			if(strstr(line,config_params[param]) != line)
				continue;
			/* go beyond the = */
			if(!(lpos =  strstr(line, "=")))
				continue;
			/* go to the beginning of value 
			 * only whitespaces are skipped, no tabs */
			do
				lpos++;
			while(*lpos == ' ');
			
			strcpy(value, lpos);

			/* throw away carriage return 
			 * might only work for *nix */
			lpos = strchr(value,'\n');
			*lpos = 0;

			/* put the value where it belongs */
			switch(param)
			{
				/* Mysql database */
				case 0: strcpy(config.database_database, value);
					break;
				/* Database server */
				case 1: strcpy(config.database_server, value);
					break;
				/* Database user */
				case 2: strcpy(config.database_user, value);
					break;
				/* Database password */
				case 3: strcpy(config.database_password, value);
					break;
				/* Database port */
				case 4: config.database_port = atoi(value);
					break;
				
				/* MPD Server */
				case 5: strcpy(config.mpd_server, value);
					break;
				/* MPD password */
				case 6: strcpy(config.mpd_password, value);
					break;
				/* MPD port */
				case 7: config.mpd_port = atoi(value);
					break;
				/* lastm user */
				case 8: strcpy(config.scrobbler_user, value);
					break;
				/* lastm hash */
				case 9: strcpy(config.scrobbler_pass, value);
					 break;
				/* lastfm tmpfile */
				case 10: strcpy(config.scrobbler_tmpfile, value);
					 break;
				/* logfile */
				case 11: strcpy(config.logfile, value);
					 break;
				/* verbosity */
				case 12: config.verbosity = atoi(value);
					 break;
				/* daemonize? */
				case 13: config.daemonize = atoi(value);
					 break;
				/* serial port */
				case 14: strcpy(config.tty, value);
					 break;
				/* led matrix ip */
				case 15: strcpy(config.led_matrix_ip, value);
					 break;
				/* led matrix port */
				case 16: config.led_matrix_port = atoi(value);
					 break;
				/* led matrix activated */
				case 17: config.led_matrix_activated = atoi(value);
					 break;
				/* scrobbler activated */
				case 18: config.scrobbler_activated = atoi(value);
					 break;
				/* pid file */
				case 19: strcpy(config.pid_file, value);
					 break;
				/* led shift speed */
				case 20: config.led_shift_speed = atoi(value);
					break;
				/* statefile */
				case 21: strcpy(config.statefile, value);
					break;
				/* serial activated */
				case 22: config.serial_activated = atoi(value);
					break;
				/* sms activated */
				case 23: config.sms_activated = atoi(value);
					break;
				/* sipgate user */
				case 24: strcpy(config.sipgate_user, value);
					 break;
				/* sipgate pass */
				case 25: strcpy(config.sipgate_pass, value);
					 break;
				/* cellphone number */
				case 26: strcpy(config.cellphone, value);
					 break;
				/* hr20 activated */
				case 27: config.hr20_activated = atoi(value);
					 break;
				/* hr20 port */
				case 28: strcpy(config.hr20_port, value);
					 break;
				/* mpd activated */
				case 29: config.mpd_activated = atoi(value);
					break;
				/* usbtemp activated */
				case 30: config.usbtemp_activated = atoi(value);
					break;
				/* usbtemp device id */
				case 31: strcpy(config.usbtemp_device_id[config.usbtemp_num_devices], value);
					break;
				/* usbtemp modul id for database */
				case 32: config.usbtemp_device_modul[config.usbtemp_num_devices] = atoi(value);
					break;
				/* usbtemp sensor id for database */
				case 33: config.usbtemp_device_sensor[config.usbtemp_num_devices] = atoi(value);
					config.usbtemp_num_devices++;
					break;
				/* hr20 insert values into database activated */
				case 34: config.hr20_database_activated = atoi(value);
					break;
				/* which module name will appear for hr20 data in database? */
				case 35: config.hr20_database_number = atoi(value);
					break;
				/* door sensor id for database */
				case 36: config.door_sensor_id = atoi(value);
					break;
				/* window sensor id for database */
				case 37: config.window_sensor_id = atoi(value);
					break;
				/* digital input module id for database */
				case 38: config.digital_input_module = atoi(value);
					break;
				/* had password */
				case 39: strcpy(config.password, value);
					 break;
			}
		}
	}

	fclose(config_file);
	return 1;
}

