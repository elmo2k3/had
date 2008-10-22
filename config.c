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
#include <string.h>

#include "config.h"
#include "had.h"

#define NUM_PARAMS 21
static char *config_params[NUM_PARAMS] = { "db_db", "db_server", "db_user", "db_pass",
	"db_port", "mpd_server", "mpd_pass", "mpd_port", "scrobbler_user", 
	"scrobbler_hash", "scrobbler_tmpfile", "logfile", "verbosity", "daemonize",
	"tty","led_matrix_ip","led_matrix_port","led_matrix_activated","scrobbler_activated",
	"pid_file","led_matrix_shift_speed"};


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
				case 9: strcpy(config.scrobbler_hash, value);
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
			}
		}
	}

	fclose(config_file);
	return 1;
}

