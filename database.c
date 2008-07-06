#include "main.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


MYSQL *mysql_connection;

int initDatabase(void)
{
	mysql_connection = mysql_init(NULL);
	
	if (!mysql_real_connect(mysql_connection, MYSQL_SERVER, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
		return -1;
	}
	mysql_connection->reconnect=1;
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

	sprintf(query,"SELECT MAX(temperature),MIN(temperature) FROM temperatures WHERE modul_id='%d' AND sensor_id='%d' AND DATE(date)=CURDATE() ORDER BY date asc",modul,sensor);
	if(mysql_query(mysql_connection,query))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
		exit(0);
	}

	mysql_res = mysql_use_result(mysql_connection);
	mysql_row = mysql_fetch_row(mysql_res); // nur eine Zeile

	if(!mysql_row[0])
	{
		mysql_free_result(mysql_res);
		printf("Keine Daten fuer den Graphen vorhanden!\n");
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

	
	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;
	
	getMinMaxTemp(modul, sensor, &max, &min);

	graph->max[0] = (int)max;
	graph->max[1] = (max - (int)max)*10;
	graph->min[0] = (int)min;
	graph->min[1] = (min - (int)min)*10;

	temp_max = ((int)((float)graph->max[0]/10)+1)*10;
	temp_min = (int)((float)graph->min[0]/10)*10;
	//temp_max = (float)graphP.max[0]/10*10;
	//temp_min = (float)graphP.min[0]/10*10;

	
	sprintf(query,"SELECT TIME_TO_SEC(date), temperature FROM temperatures WHERE modul_id='%d' AND sensor_id='%d' AND DATE(date)=CURDATE() ORDER BY date asc",modul,sensor);
	if(mysql_query(mysql_connection,query))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
		exit(0);
	}

	mysql_res = mysql_use_result(mysql_connection);
	while((mysql_row = mysql_fetch_row(mysql_res)))
	{
		sec = atoi(mysql_row[0]);
		x_div = (sec/(60*60*24))*115;
		y = transformY(atof(mysql_row[1]),temp_max,temp_min);
		if(graph->temperature_history[(int)x_div] !=0)
			graph->temperature_history[(int)x_div] = (graph->temperature_history[(int)x_div] + y ) / 2;
		else
			graph->temperature_history[(int)x_div] = y;
		//printf("x_div = %d temp = %d\r\n",(int)x_div,temperature_history[(int)x_div]);
		//temperature_history[i] = i;
	}
	graph->numberOfPoints = x_div; // Letzter Wert
	
	
	printf("Max: %d,%d Min: %d,%d\t",graph->max[0],graph->max[1],graph->min[0],graph->min[1]);
	
	mysql_free_result(mysql_res);
}

void databaseInsertTemperature(int modul, int sensor, int celsius, int decicelsius, struct tm *ptm)
{
	char query[255];

	sprintf(query,"INSERT INTO temperatures (date,modul_id,sensor_id,temperature) \
		VALUES (\"%d-%d-%d %d:%d:%d\",%d,%d,\"%d.%d\")",ptm->tm_year+1900,ptm->tm_mon+1,ptm->tm_mday,ptm->tm_hour,
		ptm->tm_min,ptm->tm_sec,modul,sensor,celsius,decicelsius);
	while(mysql_query(mysql_connection,query));
}

void getLastTemperature(int modul, int sensor, int *temp, int *temp_deci)
{
	char query[255];

	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;

	sprintf(query,"SELECT temperature FROM temperatures WHERE modul_id=%d AND sensor_id=%d ORDER BY id DESC LIMIT 1",modul,sensor);
	if(mysql_query(mysql_connection,query))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
		exit(0);
	}

	mysql_res = mysql_use_result(mysql_connection);
	mysql_row = mysql_fetch_row(mysql_res);
	*temp = atoi(mysql_row[0]);
	*temp_deci = (atof(mysql_row[0]) - *temp)*10;
	
	mysql_free_result(mysql_res);
}

