#ifndef __DATABASE_H__
#define __DATABASE_H__

#include "main.h"
#include "mysql/mysql.h"
#include <time.h>


#define MYSQL_SERVER 	"192.168.2.1"
#define MYSQL_USER	"home_automation"
#define MYSQL_PASS	"XXX"
#define MYSQL_DB	"home_automation"

int initDatabase(void);
void getDailyGraph(int modul, int sensor, struct graphPacket *graph);
void getLastTemperature(int modul, int sensor, int *temp, int *temp_deci);
void databaseInsertTemperature(int modul, int sensor, int celsius, int decicelsius, struct tm *ptm);

#endif

