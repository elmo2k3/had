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
 * \file	database.c
 * \brief	mysql database functions
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/


#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "had.h"
#include "database.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "database"

static MYSQL *mysql_connection = NULL;

static int initDatabase(void)
{
	my_bool reconnect = 1;
	const char timeout = 2;

	mysql_connection = mysql_init(NULL);
	mysql_options(mysql_connection, MYSQL_OPT_RECONNECT, &reconnect);
	mysql_options(mysql_connection, MYSQL_OPT_WRITE_TIMEOUT, &timeout); // 2 sec
	mysql_options(mysql_connection, MYSQL_OPT_READ_TIMEOUT, &timeout); // 2 sec

	if (!mysql_real_connect(mysql_connection, 
				config.database_server, 
				config.database_user,
				config.database_password,
				config.database_database, 0, NULL, 0))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
		mysql_close(mysql_connection);
		mysql_connection = NULL;
		return -1;
	}
	return 0;
}

static int transformY(float temperature, int max, int min)
{
	const float range = max - min; // hier muss noch was getan werden!
	return ((temperature-min)/range)*40;
}

static void getMinMaxTemp(int modul, int sensor, float *max, float *min)
{
	char query[255];

	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;

	sprintf(query,"SELECT MAX(value), MIN(value) FROM modul_%d WHERE sensor='%d' AND DATE(FROM_UNIXTIME(date))=CURDATE() ORDER BY date asc",modul,sensor);
	if(mysql_query(mysql_connection,query))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
	}

	mysql_res = mysql_use_result(mysql_connection);
	mysql_row = mysql_fetch_row(mysql_res); // nur eine Zeile

	if(!mysql_row[0])
	{
		mysql_free_result(mysql_res);
		g_message("Keine Daten fuer den Graphen vorhanden!");
		*max = -1000.0;
		*min = -1000.0;
		return;
	}
	*max = atof(mysql_row[0]);
	*min = atof(mysql_row[1]);

	mysql_free_result(mysql_res);
}


void getDailyGraph(int modul, int sensor, struct graphPacket *graph)
{
	char query[255];
	float x_div=0.0;
	int y;
	int temp_max,temp_min;
	float sec;	
	float min,max;

	min = 0.0;
	max = 0.0;

	graph->numberOfPoints = 0;
	
	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;
	
	getMinMaxTemp(modul, sensor, &max, &min);

	graph->max[0] = (int)max;
	graph->max[1] = (max - (int)max)*10;
	graph->min[0] = (int)min;
	graph->min[1] = (min - (int)min)*10;

	temp_max = ((int)((float)graph->max[0]/10)+1)*10;
	temp_min = (int)((float)graph->min[0]/10)*10;
	
	sprintf(query,"SELECT TIME_TO_SEC(FROM_UNIXTIME(date)), value FROM modul_%d WHERE sensor='%d' AND DATE(FROM_UNIXTIME(date))=CURDATE() ORDER BY date asc",modul,sensor);
	if(mysql_query(mysql_connection,query))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
		return;
	}

	mysql_res = mysql_use_result(mysql_connection);
	while((mysql_row = mysql_fetch_row(mysql_res)))
	{
		sec = atoi(mysql_row[0]);
		x_div = ((float)sec/(60.0*60.0*24.0))*120.0;
		y = transformY(atof(mysql_row[1]),temp_max,temp_min);
		if(graph->temperature_history[(int)x_div] !=0)
			graph->temperature_history[(int)x_div] = (graph->temperature_history[(int)x_div] + y ) / 2;
		else
			graph->temperature_history[(int)x_div] = y;
		//printf("x_div = %d temp = %d\r\n",(int)x_div,temperature_history[(int)x_div]);
		//temperature_history[i] = i;
	}
	graph->numberOfPoints = x_div; // Letzter Wert
	
	
	g_debug("Max: %d,%d Min: %d,%d\t",graph->max[0],graph->max[1],graph->min[0],graph->min[1]);
	
	mysql_free_result(mysql_res);
}

void databaseInsertDigitalValue
(int modul, int sensor, int value, time_t timestamp)
{
	float fvalue = (float)value;
	databaseInsertTemperature(modul, sensor, &fvalue, timestamp);
}

void databaseInsertTemperature(int modul, int sensor, float *temperature, time_t timestamp)
{
	static char query[DATABASE_FIFO_SIZE][128];
	static int fifo_low = 0, fifo_high = 0;
	int status;
	if(!mysql_connection)
	{
		initDatabase();
	}

	g_debug("fifo_low = %d, fifo_high = %d",fifo_low, fifo_high);
	g_debug("temperature = %2.4f",*temperature);
	sprintf(query[fifo_high],"INSERT INTO modul_%d (date,sensor,value) VALUES ('%ld','%d','%4.4f')",modul, timestamp, sensor, *temperature);

	g_debug("query = %s",query[fifo_high]);

	if(++fifo_high > (DATABASE_FIFO_SIZE -1)) fifo_high = 0;

	while( fifo_low != fifo_high )
	{
		if((status = mysql_query(mysql_connection,query[fifo_low]))) // not successfull
		{
			if(status == 2006 ) { //CR_SERVER_GONE_ERROR
				mysql_close(mysql_connection);
				initDatabase();
			}
			break; // dont try further
		}
		else // query was successfull
		{
			if(++fifo_low > (DATABASE_FIFO_SIZE - 1)) fifo_low = 0;
		}

	}
}

void getLastTemperature(int modul, int sensor, int *temp, int *temp_deci)
{
	if(!mysql_connection)
	{
		initDatabase();
	}

	char query[255];

	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;

	sprintf(query,"SELECT value FROM modul_%d WHERE sensor=%d ORDER BY date DESC LIMIT 1",modul,sensor);
	if(mysql_query(mysql_connection,query))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
	}

	mysql_res = mysql_use_result(mysql_connection);
	mysql_row = mysql_fetch_row(mysql_res);
	if(mysql_row[0])
	{
		*temp = atoi(mysql_row[0]);
		*temp_deci = (atof(mysql_row[0]) - *temp)*10;
	}
	
	mysql_free_result(mysql_res);
}

