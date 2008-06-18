#include <stdio.h>
#include <unistd.h>
#include <sys/signal.h>
#include <mysql/mysql.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "serial.h"
#include "database.h"
#include "main.h"
#include "libmpdclient.h"

void initArray(signed char *temperature_history, int size);

int fileExists(const char *filename)
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

int decodeStream(char *buf,int *modul_id, int *sensor_id, int *celsius, int *decicelsius, int *voltage)
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
					
	if(*modul_id == 7) //GLCD Modul
		return *sensor_id;
		
	trenner = (char*)strtok(NULL,";");
	if(trenner)
		*voltage = atoi(trenner);
	else
		return 0;
	return 1;
}

struct graphPacket graphP;

int main(int argc, char* argv[])
{
	int fd, res;
	char buf[255];

	char query[255];
	int modul_id,sensor_id,celsius,decicelsius,voltage;
	signed char lastTemperature[9][9][2];
	time_t rawtime;
	struct tm *ptm;
	
	struct glcdMainPacket glcdP;
	struct rgbPacket rgbP;	

	struct mpdPacket mpdP;
	

	char *hostname = "router2.lan";

	mpd_Connection *mpdCon;
	mpd_Status *mpdStatus;


	//printf("song = %i\n", mpdStatus->song);

	mpd_InfoEntity *mpdEntity;
	mpd_Song *mpdSong;
	
	//printf("%s",currentSong);
	while(1)
	{
		mpdCon = mpd_newConnection(hostname,6600,1);
		if(mpdCon->error) {
			fprintf(stderr,"%s\n",mpdCon->errorStr);
			mpd_closeConnection(mpdCon);
			return -1;
		}
		mpd_sendCommandListOkBegin(mpdCon);
		mpd_sendStatusCommand(mpdCon);
		mpd_sendCurrentSongCommand(mpdCon);
		mpd_sendCommandListEnd(mpdCon);
		
		if((mpdStatus = mpd_getStatus(mpdCon))==NULL) {
			fprintf(stderr,"%s\n",mpdCon->errorStr);
			mpd_closeConnection(mpdCon);
			return -1;
		}
		mpd_nextListOkCommand(mpdCon);
		mpdEntity = mpd_getNextInfoEntity(mpdCon);
		mpdSong = mpdEntity->info.song;
		sprintf(mpdP.currentSong,"%s - %s",mpdSong->artist,mpdSong->title);
		printf("%s\n",mpdP.currentSong);
		mpd_freeInfoEntity(mpdEntity);
		sleep(1);
		mpd_closeConnection(mpdCon);
	}

	
	fd = initSerial(argv[1]); // serielle Schnittstelle aktivieren

	initArray(graphP.temperature_history,115);

	MYSQL *mysql_connection;
	MYSQL_RES *mysql_res;

	mysql_connection = mysql_init(NULL);
	

	if(argc > 2)
	{
		rgbP.address = atoi(argv[2]);
		//printf("%d\r\n",rgbP.address);
		rgbP.red = atoi(argv[3]);
		rgbP.green = atoi(argv[4]);
		rgbP.blue = atoi(argv[5]);
		rgbP.smoothness = atoi(argv[6]);
		rgbP.count = 4;
		write(fd,&rgbP,6);
		usleep(100000);
	}
	else
	{
		if (!mysql_real_connect(mysql_connection, MYSQL_SERVER, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0))
		{
			fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
			exit(0);
		}
		mysql_connection->reconnect=1;

		while (1) {
			res = read(fd,buf,255);
			buf[res]=0;
			if(res>1)
			{
				printf("Res=%d\t",res);
				time(&rawtime);
				ptm = gmtime(&rawtime);
				switch(decodeStream(buf,&modul_id,&sensor_id,&celsius,&decicelsius,&voltage))
				{
					case 1:
						printf("%02d:%02d:%02d\t",ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
						printf("Modul ID: %d\t",modul_id);
						printf("Sensor ID: %d\t",sensor_id);
						printf("Temperatur: %d,%d\t",celsius,decicelsius);
						switch(modul_id)
						{
							case 1: printf("Spannung: %2.2f\r\n",ADC_MODUL_1/voltage); break;
							case 3: printf("Spannung: %2.2f\r\n",ADC_MODUL_3/voltage); break;
							default: printf("Spannung: %2.2f\r\n",ADC_MODUL_DEFAULT/voltage);
						}		
					
						//rawtime -= 32; // Modul misst immer vor dem Schlafengehen
						lastTemperature[modul_id][sensor_id][0] = celsius;
						lastTemperature[modul_id][sensor_id][1] = decicelsius/625;
						sprintf(query,"INSERT INTO temperatures (date,modul_id,sensor_id,temperature) \
							VALUES (\"%d-%d-%d %d:%d:%d\",%d,%d,\"%d.%d\")",ptm->tm_year+1900,ptm->tm_mon+1,ptm->tm_mday,ptm->tm_hour,
							ptm->tm_min,ptm->tm_sec,modul_id,sensor_id,celsius,decicelsius);
						while(mysql_query(mysql_connection,query));
						break;
					
					case 2:
						printf("Paket vom GLCD-Modul empfangen!!....");
						ptm = localtime(&rawtime);
						glcdP.address = 7;
						glcdP.count = 17;
						glcdP.command = 0;
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
						glcdP.temperature[4] = lastTemperature[1][1][0]; // kuehl
						glcdP.temperature[5] = lastTemperature[1][1][1];
						glcdP.temperature[6] = lastTemperature[1][2][0];// gefrier
						glcdP.temperature[7] = lastTemperature[1][2][1];
						if(fileExists("/had/wakeme"))
						{
							printf("... Wecker aktiviert ...");
							glcdP.wecker = 1;
						}
						else
							glcdP.wecker = 0;
						write(fd,&glcdP,19);
						usleep(100000);
						printf("Antwort gesendet\r\n");
						break;
					case 3:
						getDailyGraph(mysql_connection,celsius,decicelsius, graphP);
						graphP.address = 7;
						graphP.count = 61;
						graphP.command = 1;
						write(fd,&graphP,63);
						usleep(1000000);
						
						graphP.command = 2;
						graphP.count = 61;
						char *ptr = (char*)&graphP;
						write(fd,ptr,3);
						write(fd,ptr+62,60);
//						write(fd,&graphP,63);
						usleep(100000);
						printf("Graph gesendet\r\n");
						break;
					case 4:	// MPD Packet request
						printf("MPD Packet request\r\n");
						mpdP.address = 7;
						mpdP.count = 21;
						mpdP.command = 3;
						write(fd,&mpdP,23);
						break;
					case 0: //decode Stream failed
						printf("decodeStream failed!\r\n");
						break;
				}
			} // endif res>1
					
		}
		mysql_free_result(mysql_res);
		mysql_close(mysql_connection);
	}
	return 0;
}
void initArray(signed char *temperature_history, int size)
{
	int counter;
	for(counter=0;counter<size;counter++)
		temperature_history[counter]=0;
}

