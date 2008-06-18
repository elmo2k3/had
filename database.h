#include "main.h"
#include "mysql/mysql.h"
#ifndef __DATABASE_H__
#define __DATABASE_H__

#define MYSQL_SERVER 	"192.168.2.1"
#define MYSQL_USER	"home_automation"
#define MYSQL_PASS	"rfm12"
#define MYSQL_DB	"home_automation"

void getDailyGraph(MYSQL *mysql_connection, int modul, int sensor, struct graphPacket graph);
//void getMinMaxTemp(MYSQL *mysql_connection, int modul, int sensor, float *max, float *min);

#endif
