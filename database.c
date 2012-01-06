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
 * \file    database.c
 * \brief   mysql database functions
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

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
    if(range != 0)
        return (int)(((temperature-(float)min)/range)*40.0);
    else
        return 40;
}

static void getMinMaxTemp(int modul, int sensor, float *max, float *min)
{
    char query[255];

    MYSQL *mysql_ws2000_connection;
    MYSQL *mysql_local_connection;
    MYSQL_RES *mysql_res;
    MYSQL_ROW mysql_row;

    *min = 0.0;
    *max = 0.0;

    if(!mysql_connection)
    {
        if(initDatabase())
            return;
    }
    if(modul == 4) //erkenschwick
    {
        mysql_ws2000_connection = mysql_init(NULL);
        if (!mysql_real_connect(mysql_ws2000_connection, 
                    config.database_server, 
                    config.database_user,
                    config.database_password,
                    config.database_database_ws2000, 0, NULL, 0))
        {
            fprintf(stderr, "%s\r\n", mysql_error(mysql_ws2000_connection));
            mysql_close(mysql_ws2000_connection);
            return;
        }
        if(sensor == 0)
            sprintf(query,"SELECT MAX(T_1), MIN(T_1) FROM sensor_1_8 WHERE date=CURDATE()");
        else if(sensor == 1)
            sprintf(query,"SELECT MAX(T_i), MIN(T_i) FROM inside WHERE date=CURDATE()");
        mysql_local_connection = mysql_ws2000_connection;
    }
    else
    {
        mysql_local_connection = mysql_connection;
        sprintf(query,"SELECT MAX(value), MIN(value) FROM modul_%d WHERE sensor='%d' AND DATE(FROM_UNIXTIME(date))=CURDATE() ORDER BY date asc",modul,sensor);
    }
    if(mysql_query(mysql_local_connection,query))
    {
        fprintf(stderr, "%s\r\n", mysql_error(mysql_local_connection));
        if(modul == 4)
            mysql_close(mysql_local_connection);
        return;
    }

    mysql_res = mysql_use_result(mysql_local_connection);
    mysql_row = mysql_fetch_row(mysql_res); // nur eine Zeile

    if(!mysql_row[0])
    {
        mysql_free_result(mysql_res);
        g_debug("Keine Daten fuer den Graphen vorhanden!");
        *max = -1000.0;
        *min = -1000.0;
        if(modul == 4)
            mysql_close(mysql_local_connection);
        return;
    }
    *max = atof(mysql_row[0]);
    *min = atof(mysql_row[1]);

    mysql_free_result(mysql_res);
    if(modul == 4)
        mysql_close(mysql_local_connection);
}


void getDailyGraph(int modul, int sensor, struct graphPacket *graph)
{
    MYSQL *mysql_local_connection;
    MYSQL *mysql_ws2000_connection;
    char query[255];
    float x_div=0.0;
    int temp_max,temp_min;
    float sec;  
    float min,max;
    int i;
    float temperature[120];
    int num_values[120];

    min = 0.0;
    max = 0.0;

    memset(graph, 0, sizeof(struct graphPacket));

    if(!mysql_connection)
    {
        if(initDatabase())
            return;
    }
    
    MYSQL_RES *mysql_res;
    MYSQL_ROW mysql_row;
    
    getMinMaxTemp(modul, sensor, &max, &min);

    graph->max[0] = (int)max;
    graph->max[1] = (max - (int)max)*10;
    if(graph->max[1] < 0) graph->max[1] = -graph->max[1];
    graph->min[0] = (int)min;
    graph->min[1] = (min - (int)min)*10;
    if(graph->min[1] < 0) graph->min[1] = -graph->min[1];

    temp_max = ((int)(ceilf((float)graph->max[0]/10.0)))*10;
    temp_min = ((int)(floorf((float)graph->min[0]/10.0)))*10;
    g_debug("Max: %d,%d Min: %d,%d\t",graph->max[0],graph->max[1],graph->min[0],graph->min[1]);
    g_debug("Max: %d Min: %d\t",temp_max,temp_min);
    
    if(modul == 4) //erkenschwick
    {
        mysql_ws2000_connection = mysql_init(NULL);
        if (!mysql_real_connect(mysql_ws2000_connection, 
                    config.database_server, 
                    config.database_user,
                    config.database_password,
                    config.database_database_ws2000, 0, NULL, 0))
        {
            fprintf(stderr, "%s\r\n", mysql_error(mysql_ws2000_connection));
            mysql_close(mysql_ws2000_connection);
            return;
        }
        if(sensor == 0)
            sprintf(query,"SELECT TIME_TO_SEC(CONCAT(date,\" \",time)), T_1 FROM sensor_1_8 WHERE date=CURDATE()");
        else if(sensor == 1)
            sprintf(query,"SELECT TIME_TO_SEC(CONCAT(date,\" \",time)), T_i FROM inside WHERE date=CURDATE()");
        mysql_local_connection = mysql_ws2000_connection;
    }
    else
    {
        sprintf(query,"SELECT TIME_TO_SEC(FROM_UNIXTIME(date)), value FROM modul_%d WHERE sensor='%d' AND DATE(FROM_UNIXTIME(date))=CURDATE() ORDER BY date asc",modul,sensor);
        mysql_local_connection = mysql_connection;
    }
    if(mysql_query(mysql_local_connection,query))
    {
        fprintf(stderr, "%s\r\n", mysql_error(mysql_local_connection));
        if(modul == 4)
            mysql_close(mysql_local_connection);
        return;
    }

    mysql_res = mysql_use_result(mysql_local_connection);

    for(i=0;i<120;i++)
    {
        num_values[i] = 0;
        temperature[i] = 0.0;
    }
    while((mysql_row = mysql_fetch_row(mysql_res)))
    {
        sec = atoi(mysql_row[0]);
        x_div = (float)sec/(60.0*60.0*24.0)*120.0;
        num_values[(int)x_div]++;
        temperature[(int)x_div] += atof(mysql_row[1]);
    }
    graph->numberOfPoints = x_div; // Letzter Wert
    for(i=0;i<(int)x_div;i++)
    {
        if(num_values[i] > 0)
        {
            graph->temperature_history[i] = transformY(
                temperature[i]/((float)num_values[i]), temp_max, temp_min);
        }
        else if(i > 0)
        {
            graph->temperature_history[i] = graph->temperature_history[i-1];
        }
    }
    
    mysql_free_result(mysql_res);
    if(modul == 4)
        mysql_close(mysql_local_connection);
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
    
    if(!mysql_connection)
        return;

    while( fifo_low != fifo_high )
    {
        if((status = mysql_query(mysql_connection,query[fifo_low]))) // not successfull
        {
            mysql_close(mysql_connection);
            mysql_connection = NULL;
            break; // dont try further
        }
        else // query was successfull
        {
            if(++fifo_low > (DATABASE_FIFO_SIZE - 1)) fifo_low = 0;
        }

    }
}

void getLastTemperature(int modul, int sensor, int16_t *temp)
{
    char query[255];

    MYSQL *mysql_ws2000_connection;
    MYSQL *mysql_local_connection;
    MYSQL_RES *mysql_res;
    MYSQL_ROW mysql_row;

    float temperature;

    if(!mysql_connection)
    {
        if(initDatabase())
            return;
    }
    if(modul == 4) //erkenschwick
    {
        mysql_ws2000_connection = mysql_init(NULL);
        if (!mysql_real_connect(mysql_ws2000_connection, 
                    config.database_server, 
                    config.database_user,
                    config.database_password,
                    config.database_database_ws2000, 0, NULL, 0))
        {
            fprintf(stderr, "%s\r\n", mysql_error(mysql_ws2000_connection));
            mysql_close(mysql_ws2000_connection);
            return;
        }
        if(sensor == 0)
            sprintf(query,"SELECT T_1 FROM sensor_1_8 ORDER BY date DESC LIMIT 1");
        else if(sensor == 1)
            sprintf(query,"SELECT T_i FROM inside ORDER BY date DESC LIMIT 1");
        mysql_local_connection = mysql_ws2000_connection;
    }
    else
    {
        mysql_local_connection = mysql_connection;
        sprintf(query,"SELECT value FROM modul_%d WHERE sensor='%d' ORDER BY date DESC LIMIT 1",modul,sensor);
    }
    if(mysql_query(mysql_local_connection,query))
    {
        fprintf(stderr, "%s\r\n", mysql_error(mysql_local_connection));
        if(modul == 4)
            mysql_close(mysql_local_connection);
        return;
    }

    mysql_res = mysql_use_result(mysql_local_connection);
    mysql_row = mysql_fetch_row(mysql_res); // nur eine Zeile

    if(!mysql_row[0])
    {
        mysql_free_result(mysql_res);
        *temp= 0;
        if(modul == 4)
            mysql_close(mysql_local_connection);
        return;
    }
    temperature = atof(mysql_row[0]);
    // quite dirty hack: as there is no int with value -0 we have to put the
    // information about negative value into the decimal
    *temp = (int16_t)(temperature*10.0);

    mysql_free_result(mysql_res);
    if(modul == 4)
        mysql_close(mysql_local_connection);
}

