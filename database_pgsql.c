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

#include "config.h"
#include "had.h"
#include "database.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "database_pgsql"

#ifdef HAVE_POSTGRESQL
static PGconn *pgconn = NULL;

static int initDatabase(void)
{
    const char timeout = 2;
    char conninfo[200];

    g_debug("init pg db");

    snprintf(conninfo,sizeof(conninfo),
        "dbname = %s host = %s user = %s sslmode = %s password = %s",
        config.database_pg_database,
        config.database_pg_server,
        config.database_pg_user,
        config.database_pg_sslmode,
        config.database_pg_password);

    pgconn = PQconnectdb(conninfo);

    g_debug(conninfo);

    if (PQstatus(pgconn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s",
                PQerrorMessage(pgconn));
        PQfinish(pgconn);
        pgconn = NULL;
        return -1;
    }
    return 0;
}

/*static int transformY(float temperature, int max, int min)
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
    char *c;
    PGresult   *res;

    *min = 0.0;
    *max = 0.0;

    if(!pgconn)
    {
        if(initDatabase())
            return;
    }
    sprintf(query,"SELECT MAX(value), MIN(value) FROM modul_%d%d WHERE DATE(FROM_UNIXTIME(date))=CURDATE() ORDER BY date asc",modul,sensor);
    res = PQexec(pgconn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "FETCH ALL failed: %s", PQerrorMessage(pgconn));
        PQclear(res);
    }

    if(PQntuples(res) != 1)
    {
        g_debug("Keine Daten fuer den Graphen vorhanden!");
        *max = -1000.0;
        *min = -1000.0;
        PQclear(res);
        return;
    }
    
    c = PQgetvalue(res,0,0);
    *max = atof(c);
    c = PQgetvalue(res,0,1);
    *min = atof(c);
    
    PQclear(res);
}*/


/*void getDailyGraph(int modul, int sensor, struct graphPacket *graph)
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
        
}*/

void databasePgInsertDigitalValue
(int modul, int sensor, int value, time_t timestamp)
{
    float fvalue = (float)value;
    if(!config.database_pg_activated)
        return;
    databasePgInsertTemperature(modul, sensor, &fvalue, timestamp);
}

void databasePgInsertTemperature(int modul, int sensor, float *temperature, time_t timestamp)
{
    static char query[DATABASE_FIFO_SIZE][128];
    static int fifo_low = 0, fifo_high = 0;
    char create_query[250];
    int status;
    PGresult *res;
    
    if(!config.database_pg_activated)
        return;

    if(!pgconn)
    {
        initDatabase();
    }
    if(!pgconn)
        return;

    sprintf(query[fifo_high],"INSERT INTO modul_%02d%02d (date,value) VALUES (to_timestamp(%ld),%4.4f)",modul, sensor, timestamp, *temperature);

    if(++fifo_high > (DATABASE_FIFO_SIZE -1)) fifo_high = 0;
    
    // error code for undefined table 42P01
    while( fifo_low != fifo_high )
    {
        res = PQexec(pgconn, query[fifo_low]);
        if(PQresultStatus(res) != PGRES_COMMAND_OK) // not successfull
        {
            // modul table does not exist yet
            if(strcmp(PQresultErrorField(res, PG_DIAG_SQLSTATE), "42P01") == 0)
            {
                PQclear(res);
                sprintf(create_query,"CREATE TABLE modul_%02d%02d (date timestamp without time zone, value real, PRIMARY KEY(date))",
                    modul,sensor);
                res = PQexec(pgconn,create_query);
                if(PQresultStatus(res) != PGRES_COMMAND_OK) // not successfull
                g_debug("no success %s",PQerrorMessage(pgconn));
            }
            // duplicate_object 42710 -> row with this date exists yet. throw data away
            else if(strcmp(PQresultErrorField(res, PG_DIAG_SQLSTATE), "23505") == 0)
            {
                g_debug("thrown away");
                if(++fifo_low > (DATABASE_FIFO_SIZE - 1)) fifo_low = 0;
            }
            else
            {
                g_debug("no success %s",PQerrorMessage(pgconn));
                g_debug(PQresultErrorField(res, PG_DIAG_SQLSTATE));
                PQfinish(pgconn);
                pgconn = NULL;
            }
            PQclear(res);
            break; // dont try further
        }
        else // query was successfull
        {
            PQclear(res);
            g_debug("success");
            if(++fifo_low > (DATABASE_FIFO_SIZE - 1)) fifo_low = 0;
        }

    }
}

void pgGetLastTemperature(int modul, int sensor, int16_t *temp)
{
    float temperature;
    char query[255];

    PGresult *pgres;
    
    if(!config.database_pg_activated)
        return;

    if(!pgconn)
    {
        initDatabase();
    }

    *temp = 0;
    if(!pgconn)
        return;

    sprintf(query,"SELECT value FROM modul_%02d%02d ORDER BY date DESC LIMIT 1",modul,sensor);

    g_debug(query);
    
    pgres = PQexec(pgconn, query);
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK)
    {
        g_warning("failed: %s", PQerrorMessage(pgconn));
        PQclear(pgres);
        PQfinish(pgconn);
        pgconn = NULL;
        return;
    }
    if(PQntuples(pgres) != 1)
        return;
    
    temperature = atof(PQgetvalue(pgres,0,0));

    g_debug("temperature = %f",temperature);

    *temp = (int16_t)(temperature*10.0);
    PQclear(pgres);
}

#endif // HAVE_POSTGRESQL
