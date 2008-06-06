#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <mysql/mysql.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>

#define BAUDRATE B19200
#define FALSE 0
#define TRUE 1

#define MYSQL_SERVER 	"192.168.2.1"
#define MYSQL_USER	"home_automation"
#define MYSQL_PASS	"rfm12"
#define MYSQL_DB	"home_automation"

#define ADC_RES 1024

#define ADC_MODUL_1 ADC_RES*1.22

#define ADC_MODUL_3 ADC_RES*1.3

#define ADC_MODUL_DEFAULT ADC_RES*1.25

void getDailyGraph(MYSQL *mysql_connection, int modul, int sensor);
void initArray(signed char *temperature_history, int size);
int transformY(float temperature, int max, int min);

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

volatile int STOP=FALSE; 

struct graphPacket
{
	unsigned char address;
	unsigned char count;
	unsigned char command;
	unsigned char numberOfPoints;
	signed char max[2];
	signed char min[2];
	signed char temperature_history[115];
}sGraphPacket;


int main(int argc, char* argv[])
{
	int fd,c, res;
	struct termios newtio;
	struct sigaction saio;           /* definition of signal action */
	char buf[255];

	char query[255];
	int modul_id,sensor_id,celsius,decicelsius,voltage;
	signed char lastTemperature[9][9][2];
	time_t rawtime;
	struct tm *ptm;
	
	struct packet
	{
		unsigned char address;
		unsigned char count;
		unsigned char red;
		unsigned char green;
		unsigned char blue;
		unsigned char smoothness;
	}myp;
	
	struct packet2
	{
		unsigned char address;			// 1 Byte
		unsigned char count;			// 1 Byte
		unsigned char command;
		unsigned char hour;				// 1 Byte
		unsigned char minute;			// 1 Byte
		unsigned char second;			// 1 Byte
		unsigned char day;				// 1 Byte
		unsigned char month;			// 1 Byte
		unsigned char year;				// 1 Byte
		unsigned char weekday;			// 1 Byte
		unsigned char temperature[8];	// 8 Bytes
		unsigned char wecker;
	}glcdPacket;

	initArray(sGraphPacket.temperature_history,115);

	MYSQL *mysql_connection;
	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;

	mysql_connection = mysql_init(NULL);
	
	unsigned char wecker = 0;
	
	/* open the device */
	fd = open(argv[1], O_RDWR | O_NOCTTY );
	if (fd <0) {perror(argv[1]); exit(-1); }

	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */
	/* 
	BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
	CRTSCTS : output hardware flow control (only used if the cable has
		all necessary lines. See sect. 7 of Serial-HOWTO)
	CS8     : 8n1 (8bit,no parity,1 stopbit)
	CLOCAL  : local connection, no modem contol
	CREAD   : enable receiving characters
	*/
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	/*
	IGNPAR  : ignore bytes with parity errors
	ICRNL   : map CR to NL (otherwise a CR input on the other computer
		will not terminate input)
	otherwise make device raw (no other input processing)
	*/
	newtio.c_iflag = IGNPAR | ICRNL;
	/*
	Raw output.
	*/
	newtio.c_oflag = 0;
	/*
	ICANON  : enable canonical input
	disable all echo functionality, and don't send signals to calling program
	*/
	newtio.c_lflag = ICANON;
	/* 
	initialize all control characters 
	default values can be found in /usr/include/termios.h, and are given
	in the comments, but we don't need them here
	*/
	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtio.c_cc[VERASE]   = 0;     /* del */
	newtio.c_cc[VKILL]    = 0;     /* @ */
	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
	newtio.c_cc[VSWTC]    = 0;     /* '\0' */
	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtio.c_cc[VEOL]     = 0;     /* '\0' */
	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtio.c_cc[VEOL2]    = 0;     /* '\0' */

	/* 
	now clean the modem line and activate the settings for the port
	*/
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);

	if(argc > 2)
	{
		myp.address = atoi(argv[2]);
		//printf("%d\r\n",myp.address);
		myp.red = atoi(argv[3]);
		myp.green = atoi(argv[4]);
		myp.blue = atoi(argv[5]);
		myp.smoothness = atoi(argv[6]);
		myp.count = 4;
		write(fd,&myp,6);
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
						glcdPacket.address = 7;
						glcdPacket.count = 17;
						glcdPacket.command = 0;
						glcdPacket.hour = ptm->tm_hour;
						glcdPacket.minute = ptm->tm_min;
						glcdPacket.second = ptm->tm_sec;
						glcdPacket.day = ptm->tm_mday;
						glcdPacket.month = ptm->tm_mon+1;
						glcdPacket.year = ptm->tm_year;
						glcdPacket.weekday = 0;
						glcdPacket.temperature[0] = lastTemperature[3][1][0]; // draussen
						glcdPacket.temperature[1] = lastTemperature[3][1][1]; 
						glcdPacket.temperature[2] = lastTemperature[3][0][0]; // schlaf
						glcdPacket.temperature[3] = lastTemperature[3][0][1];
						glcdPacket.temperature[4] = lastTemperature[1][1][0]; // kuehl
						glcdPacket.temperature[5] = lastTemperature[1][1][1];
						glcdPacket.temperature[6] = lastTemperature[1][2][0];// gefrier
						glcdPacket.temperature[7] = lastTemperature[1][2][1];
						if(fileExists("/had/wakeme"))
						{
							printf("... Wecker aktiviert ...");
							glcdPacket.wecker = 1;
						}
						else
							glcdPacket.wecker = 0;
						write(fd,&glcdPacket,19);
						usleep(100000);
						printf("Antwort gesendet\r\n");
						break;
					case 3:
						getDailyGraph(mysql_connection,celsius,decicelsius);
						sGraphPacket.address = 7;
						sGraphPacket.count = 61;
						sGraphPacket.command = 1;
						write(fd,&sGraphPacket,63);
						usleep(1000000);
						
						sGraphPacket.command = 2;
						sGraphPacket.count = 61;
						char *ptr = (char*)&sGraphPacket;
						write(fd,ptr,3);
						write(fd,ptr+62,60);
//						write(fd,&sGraphPacket,63);
						usleep(100000);
						printf("Graph gesendet\r\n");
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

void getDailyGraph(MYSQL *mysql_connection, int modul, int sensor)
{
	char query[255];
	float x_div;
	int y;
	int temp_max,temp_min;

	
	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;
	MYSQL *mysql_helper_connection;

	float sec;
	
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
	sGraphPacket.max[0] = atoi(mysql_row[0]);
	sGraphPacket.max[1] = (atof(mysql_row[0]) - atoi(mysql_row[0]))*10;
	sGraphPacket.min[0] = atoi(mysql_row[1]);
	sGraphPacket.min[1] = (atof(mysql_row[1]) - atoi(mysql_row[1]))*10;

	temp_max = ceil((float)sGraphPacket.max[0]/10)*10;
	temp_min = floor((float)sGraphPacket.min[0]/10)*10;

	mysql_free_result(mysql_res);
	
	sprintf(query,"SELECT TIME_TO_SEC(date), temperature FROM temperatures WHERE modul_id='%d' AND sensor_id='%d' AND DATE(date)=CURDATE() ORDER BY date asc",modul,sensor);
	if(mysql_query(mysql_connection,query))
	{
		fprintf(stderr, "%s\r\n", mysql_error(mysql_connection));
		exit(0);
	}

	mysql_res = mysql_use_result(mysql_connection);
	while(mysql_row = mysql_fetch_row(mysql_res))
	{
		sec = atoi(mysql_row[0]);
		x_div = (sec/(60*60*24))*115;
		y = transformY(atof(mysql_row[1]),temp_max,temp_min);
		if(sGraphPacket.temperature_history[(int)x_div] !=0)
			sGraphPacket.temperature_history[(int)x_div] = (sGraphPacket.temperature_history[(int)x_div] + y ) / 2;
		else
			sGraphPacket.temperature_history[(int)x_div] = y;
		//printf("x_div = %d temp = %d\r\n",(int)x_div,temperature_history[(int)x_div]);
		//temperature_history[i] = i;
	}
	sGraphPacket.numberOfPoints = x_div; // Letzter Wert
	
	
	printf("Max: %d,%d Min: %d,%d\t",sGraphPacket.max[0],sGraphPacket.max[1],sGraphPacket.min[0],sGraphPacket.min[1]);
	
	mysql_free_result(mysql_res);
}
int transformY(float temperature, int max, int min)
{
	const float range = max - min; // hier muss noch was getan werden!
	return ((temperature-min)/range)*40;
}
