#ifndef __MAIN_H__
#define __MAIN_H__

#define ADC_RES 1024
#define ADC_MODUL_1 ADC_RES*1.22
#define ADC_MODUL_3 ADC_RES*1.3
#define ADC_MODUL_DEFAULT ADC_RES*1.25


/* 23 Bytes */
struct mpdPacket
{
	unsigned char address;
	unsigned char count;
	unsigned char command;
	char currentSong[20];
};

/* 123 Bytes */
struct graphPacket
{
	unsigned char address;
	unsigned char count;
	unsigned char command;
	unsigned char numberOfPoints;
	signed char max[2];
	signed char min[2];
	signed char temperature_history[115];
};

/* 19 Byte */
struct glcdMainPacket
{
	unsigned char address;
	unsigned char count;
	unsigned char command;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char day;
	unsigned char month;
	unsigned char year;
	unsigned char weekday;
	unsigned char temperature[8];
	unsigned char wecker;
};

/* 6 Byte */
struct rgbPacket
{
	unsigned char address;
	unsigned char count;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char smoothness;
};


#endif
