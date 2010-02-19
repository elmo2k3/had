/*
 * Copyright (C) 2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#include "base_station.h"
#include "had.h"
#include "mpd.h"
#include "led_routines.h"
#include "database.h"

static void incrementColor(uint8_t *color);

static struct BaseStation
{
	gchar cmd[1024];
	guint cmd_position;
	guint serial_port_watcher; /**< glib watcher */
	gchar error_string[1024]; /**< last error should be stored here (not in use yet) */
	GIOChannel *channel;
}base_station;

void process_glcd_remote(gchar **strings, int argc)
{
	int command;
	int gpcounter;
	
	verbose_printf(9,"Processing glcd_remote packet\n");

	if(strings[1])
	{
		command = atoi(strings[1]);
		if(command == 2)
		{
			updateGlcd();
			verbose_printf(9,"GraphLCD Info Paket gesendet\r\n");
		}
		else if(command == config.rkeys.mpd_random)
		{

			/* 50 - 82 reserved for remote control */
			mpdToggleRandom();
		}
		else if(command == config.rkeys.mpd_prev)
		{
				verbose_printf(9,"MPD prev\r\n");
				mpdPrev();
		}
		else if(command == config.rkeys.mpd_next)
		{
			verbose_printf(9,"MPD next song\r\n");
			mpdNext();
		}
		else if(command == config.rkeys.mpd_play_pause)
		{
			mpdTogglePlayPause();
		}
		else if(command == config.rkeys.music_on_hifi_on)
		{
			system(SYSTEM_MOUNT_MPD);
			relaisP.port |= 4;
			sendPacket(&relaisP, RELAIS_PACKET);
			if(relaisP.port & 4)
			{
				ledMatrixStart();
				hadState.ledmatrix_user_activated = 1;
			}
			hadState.relais_state = relaisP.port;
			mpdPlay();
		}
		else if(command == config.rkeys.everything_off)
		{
			relaisP.port = 0;
			sendPacket(&relaisP, RELAIS_PACKET);
			ledMatrixStop();
			hadState.relais_state = relaisP.port;
			mpdPause();
			for(gpcounter = 0; gpcounter < 3; gpcounter++)
			{
				hadState.rgbModuleValues[gpcounter].red = 0;
				hadState.rgbModuleValues[gpcounter].green = 0;
				hadState.rgbModuleValues[gpcounter].blue = 0;
			}
			setCurrentRgbValues();
			system(SYSTEM_KILL_MPD);
		}
		else if(command == config.rkeys.hifi_on_off)
		{
			relaisP.port ^= 4;
			sendPacket(&relaisP, RELAIS_PACKET);
			if(relaisP.port & 4)
			{
				ledMatrixStart();
				hadState.ledmatrix_user_activated = 1;
			}
			else
			{
				ledMatrixStop();
			}
			hadState.relais_state = relaisP.port;
		}
		else if(command == config.rkeys.brightlight)
		{
			relaisP.port ^= 32;
			sendPacket(&relaisP, RELAIS_PACKET);	
			hadState.relais_state = relaisP.port;
		}
		else if(command == config.rkeys.light_off[0] || command == config.rkeys.light_off[1])
		{
			for(gpcounter = 0; gpcounter < 3; gpcounter++)
			{
				hadState.rgbModuleValues[gpcounter].red = 0;
				hadState.rgbModuleValues[gpcounter].green = 0;
				hadState.rgbModuleValues[gpcounter].blue = 0;
			}
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.light_single_off[0])
		{
			hadState.rgbModuleValues[0].red = 0;
			hadState.rgbModuleValues[0].green = 0;
			hadState.rgbModuleValues[0].blue = 0;
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.light_single_off[1])
		{
			hadState.rgbModuleValues[1].red = 0;
			hadState.rgbModuleValues[1].green = 0;
			hadState.rgbModuleValues[1].blue = 0;
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.light_single_off[2])
		{
			hadState.rgbModuleValues[2].red = 0;
			hadState.rgbModuleValues[2].green = 0;
			hadState.rgbModuleValues[2].blue = 0;
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.light_on)
		{
			for(gpcounter = 0; gpcounter < 3; gpcounter++)
			{
				hadState.rgbModuleValues[gpcounter].red = 255;
				hadState.rgbModuleValues[gpcounter].green = 255 ;
				hadState.rgbModuleValues[gpcounter].blue = 0;
			}
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.red)
		{
			for(gpcounter = 0; gpcounter < 3; gpcounter++)
			{
				incrementColor(&hadState.rgbModuleValues[gpcounter].red);
			}
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.green)
		{
			for(gpcounter = 0; gpcounter < 3; gpcounter++)
			{
				incrementColor(&hadState.rgbModuleValues[gpcounter].green);
			}
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.blue)
		{
			for(gpcounter = 0; gpcounter < 3; gpcounter++)
			{
				incrementColor(&hadState.rgbModuleValues[gpcounter].blue);
			}
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.red_single[0])
		{
			incrementColor(&hadState.rgbModuleValues[0].red);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.red_single[1])
		{
			incrementColor(&hadState.rgbModuleValues[1].red);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.red_single[2])
		{
			incrementColor(&hadState.rgbModuleValues[2].red);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.green_single[0])
		{
			incrementColor(&hadState.rgbModuleValues[0].green);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.green_single[1])
		{
			incrementColor(&hadState.rgbModuleValues[1].green);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.green_single[2])
		{
			incrementColor(&hadState.rgbModuleValues[2].green);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.blue_single[0])
		{
			incrementColor(&hadState.rgbModuleValues[0].blue);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.blue_single[1])
		{
			incrementColor(&hadState.rgbModuleValues[1].blue);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.blue_single[2])
		{
			incrementColor(&hadState.rgbModuleValues[2].blue);
			setCurrentRgbValues();
		}
		else if(command == config.rkeys.ledmatrix_toggle)
		{
			ledMatrixToggle();
		}
		else if(command == config.rkeys.open_door)
		{
			open_door();
		}
	}
}

void process_temperature_module(gchar **strings, int argc)
{
	int modul_id, sensor_id;
	int voltage;
	float temperature;
	char temp_string[1023];
	char buf[256];
	
	verbose_printf(9,"Processing temperature_module packet\n");
	if(argc != 5)
	{
		verbose_printf(0,"Got wrong count of parameters from temperature-module\n");
		return;
	}

	sprintf(temp_string,"%s.%s",strings[2], strings[3]);
	temperature = atof(temp_string);
	modul_id = atoi(strings[0]);
	sensor_id = atoi(strings[1]);
	voltage = atoi(strings[2]);

	verbose_printf(9,"Modul ID: %d\t",modul_id);
	verbose_printf(9,"Sensor ID: %d\t",sensor_id);
	verbose_printf(9,"Temperatur: %2.2f\t\n",temperature);
	switch(modul_id)
	{
		case 1: verbose_printf(9,"Spannung: %2.2f\n",ADC_MODUL_1/voltage); break;
		case 3: verbose_printf(9,"Spannung: %2.2f\n",ADC_MODUL_3/voltage); break;
		default: verbose_printf(9,"Spannung: %2.2f\n",ADC_MODUL_DEFAULT/voltage);
	}		

	//rawtime -= 32; // Modul misst immer vor dem Schlafengehen
	lastTemperature[modul_id][sensor_id][0] = (int16_t)atoi(strings[2]);
	lastTemperature[modul_id][sensor_id][1] = (int16_t)atoi(strings[3]);
	lastVoltage[modul_id] = voltage;
	
	databaseInsertTemperature(modul_id,sensor_id,&temperature,time(NULL));

	sprintf(buf,"Aussen:  %2d.%2d CInnen:   %2d.%2d C",
			lastTemperature[3][1][0],
			lastTemperature[3][1][1]/100,
			lastTemperature[3][0][0],
			lastTemperature[3][0][1]/100);
	sendBaseLcdText(buf);

//	if(lastTemperature[3][0][0] < 15 && !belowMinTemp &&
//			config.sms_activated)
//	{
//		char stringToSend[100];
//		belowMinTemp = 1;
//		sprintf(stringToSend,"%s had: Temperature is now %2d.%2d",theTime(),
//				lastTemperature[3][0][0],
//			lastTemperature[3][0][1]);
//		sms(stringToSend);
//	}
//	else if(lastTemperature[3][0][0] > 16)
//		belowMinTemp = 0;
}

void process_base_station(gchar **strings, int argc)
{
	int command;
	int rawtime;
	
	verbose_printf(9,"Processing base_station packet\n");
	
	rawtime = time(NULL);

	if(strings[1])
	{
		command = atoi(strings[1]);
		if(command == 10)
		{
			verbose_printf(0,"Serial Modul hard-reset\n");
			sendBaseLcdText("Modul neu gestartet ....");
		}
		else if(command == 11)
		{
			verbose_printf(0,"Serial Modul Watchdog-reset\n");
		}
		else if(command == 12)
		{
			verbose_printf(0,"Serial Modul uart timeout\n");
		}
		else if(command == 30)
		{
			verbose_printf(1,"Door opened\n");
			hadState.input_state |= 1;
			/* check for opened window */
			if(hadState.input_state & 8)
			{
				verbose_printf(0,"Window and door open at the same time! BEEEEP\n");
				if(hadState.beep_on_window_left_open)
				{
					setBeepOn();
					g_usleep(1000000);
					setBeepOff();
				}
			}
			if(config.door_sensor_id && config.digital_input_module)
				databaseInsertDigitalValue(config.digital_input_module,
					config.door_sensor_id, 1, rawtime);
		}
		else if(command == 31)
		{
			verbose_printf(1,"Door closed\n");
			hadState.input_state &= ~1;
			setBeepOff();
			if(config.door_sensor_id && config.digital_input_module)
				databaseInsertDigitalValue(config.digital_input_module,
					config.door_sensor_id, 0, rawtime);
		}
		else if(command == 32) {} // 2 open
		else if(command == 33) {} // 2 closed
		else if(command == 34) {}
		else if(command == 35) {}
		else if(command == 36)
		{
			verbose_printf(1,"Window closed\n");
			hadState.input_state &= ~8;
			setBeepOff();
			if(config.window_sensor_id && config.digital_input_module)
				databaseInsertDigitalValue(config.digital_input_module,
					config.window_sensor_id, 0, rawtime);
//				if(config.hr20_activated && hr20info.tempset >= 50
//					&& hr20info.tempset <= 300)
//				{
//					hr20SetTemperature(hr20info.tempset);
//				}
		}
		else if(command == 36)
		{
			verbose_printf(1,"Window opened\n");
			hadState.input_state |= 8;
			if(config.window_sensor_id && config.digital_input_module)
				databaseInsertDigitalValue(config.digital_input_module,
					config.window_sensor_id, 1, rawtime);
			if(config.hr20_activated)
			{
//							hr20GetStatus(&hr20info);
//							hr20SetTemperature(50);
			}
		}
	}
}

void process_command(struct BaseStation *base_station)
{
	gchar **strings;
	GError *error = NULL;
	int i=0;

	strings = g_strsplit( base_station->cmd, ";", 5);
	
	while(strings[i])
	{
		i++;
	}
	if(strings[0])
	{
		switch(strings[0][0])
		{
			case '3':	process_temperature_module(strings,i); break;
			case '7':	process_glcd_remote(strings,i); break;
			case '1':	if(strings[0][1] == '0') process_base_station(strings,i); break;
			default: 	
						g_debug("Unknown command: %s",strings[0]);
						break;
		}
	}
	g_io_channel_flush(base_station->channel, &error);
	if(error)
		g_error_free(error);
	g_strfreev(strings);
}

static gboolean serialReceive
(GIOChannel *channel, GIOCondition *condition, struct BaseStation *base_station)
{
    gchar buf[2048];
    gsize bytes_read;
	GError *error = NULL;
	GIOStatus status;
    gint i;
	
	if(condition != G_IO_IN)
	{
		g_debug("base_station: error in serialReceive");
	}
    status = g_io_channel_read_chars(channel, buf, sizeof(buf), &bytes_read, &error);
	if( status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN)
	{
		g_debug("status = %d",status);
		if(error)
		{
			g_debug("error = %s",error->message);
			g_error_free(error);
		}
		return FALSE;
	}
    buf[bytes_read] = '\0';

    for(i=0; i < bytes_read; i++)
    {
		if(buf[i] == '\r' || base_station->cmd_position == 1023)
		{
			process_command(base_station);
	//		g_debug("%lld %s",time(NULL), base_station->cmd);
			base_station->cmd[base_station->cmd_position] = '\0';
			base_station->cmd_position = 0;
		}
		else if(buf[i] == '\n')
		{
		}
		else if(buf[i])
        {
            base_station->cmd[base_station->cmd_position++] = buf[i];
        }
    }
    return TRUE;
}

int base_station_init(char *serial_device)
{
    int fd;
	struct termios newtio;
	GError *error = NULL;
	/* open the device */
	fd = open(serial_device, O_RDWR | O_NOCTTY /*| O_NDELAY*/ | O_NONBLOCK );
	if (fd <0) 
    {
		return 1;
	}

	memset(&newtio, 0, sizeof(newtio)); /* clear struct for new port settings */
	newtio.c_cflag = B19200 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = (ICANON);
	newtio.c_oflag = 0;
	tcflush(fd, TCIFLUSH);
	if(tcsetattr(fd,TCSANOW,&newtio) < 0)
	{
		return 1;
	}
    
    GIOChannel *serial_device_chan = g_io_channel_unix_new(fd);
    guint serial_watch = g_io_add_watch(serial_device_chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
		(GIOFunc)serialReceive, &base_station);
//    g_io_add_watch(serial_device_chan, G_IO_OUT,
//		(GIOFunc)serialTransmit, base_station_to_return);
//	g_io_add_watch(serial_device_chan, G_IO_ERR, (GIOFunc)exit, NULL);
	g_io_channel_set_encoding(serial_device_chan, NULL, &error);
    g_io_channel_unref(serial_device_chan);
	base_station.channel = serial_device_chan;
	base_station.serial_port_watcher = serial_watch;

	return 0;
}

void sendPacket(void *packet, int type)
{
	struct headPacket *headP = (struct headPacket*)packet;
	gsize bytes_written;
	GError *error = NULL;

	if(config.serial_activated)
	{
		if(type == GP_PACKET)
		{
			headP->address = GLCD_ADDRESS;
			headP->command = GP_PACKET;
			headP->count = 18;
			g_io_channel_write_chars(base_station.channel, packet, sizeof(glcdP),
				&bytes_written, &error);
			//write(fd,packet,sizeof(glcdP));
		}
		else if(type == MPD_PACKET)
		{
			headP->address = GLCD_ADDRESS;
			headP->command = MPD_PACKET;
			headP->count = 32;
			g_io_channel_write_chars(base_station.channel, packet, sizeof(mpdP),
				&bytes_written, &error);
			//write(fd,packet,sizeof(mpdP));
		}
		else if(type == GRAPH_PACKET)
		{
			/* Very dirty! Zweistufiges Senden wegen Pufferueberlauf */
			headP->address = GLCD_ADDRESS;
			headP->count = 61;
			headP->command = GRAPH_PACKET;
			g_io_channel_write_chars(base_station.channel, packet, 63,
				&bytes_written, &error);

			headP->command = GRAPH_PACKET2;
			char *ptr = (char*)packet;
			g_io_channel_write_chars(base_station.channel, ptr, 3,
				&bytes_written, &error);
			g_io_channel_write_chars(base_station.channel, ptr+62, 60,
				&bytes_written, &error);
		}
		else if(type == RGB_PACKET)
		{
			struct _rgbPacket rgbP;
			headP->count = 5;
			g_io_channel_write_chars(base_station.channel, packet, sizeof(rgbP),
				&bytes_written, &error);
			//write(fd,packet,sizeof(rgbP));
		}		
		else if(type == RELAIS_PACKET)
		{
			headP->address = 0x02;
			headP->count = 2;
			headP->command = 0;
			g_io_channel_write_chars(base_station.channel, packet, sizeof(relaisP),
				&bytes_written, &error);
		}
		if(error)
			g_error_free(error);
	}
	g_io_channel_flush(base_station.channel, NULL);
}

void sendRgbPacket
(unsigned char address, unsigned char red, unsigned char green,
unsigned char blue, unsigned char smoothness)
{
	struct _rgbPacket rgbPacket;

	memset(&rgbPacket, 0, sizeof(rgbPacket));

	rgbPacket.headP.address = address;
	rgbPacket.headP.count = 5;
	rgbPacket.headP.command = RGB_PACKET;
	rgbPacket.red = red;
	rgbPacket.green = green;
	rgbPacket.blue = blue;
	rgbPacket.smoothness = smoothness;

	sendPacket(&rgbPacket,RGB_PACKET);
}

void setCurrentRgbValues()
{
	int i;
	for(i=0;i<3;i++)
	{
		sendRgbPacket(0x10+i, hadState.rgbModuleValues[i].red,
			hadState.rgbModuleValues[i].green,
			hadState.rgbModuleValues[i].blue, 0);
	}
}

void setBeepOn()
{
	gsize bytes_written;
	GError *error = NULL;
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 3;
	if(config.serial_activated)
		g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
			&bytes_written, &error);
	if(error)
		g_error_free(error);
	g_io_channel_flush(base_station.channel, NULL);
}

void setBeepOff()
{
	gsize bytes_written;
	GError *error = NULL;
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 4;
	if(config.serial_activated)
		g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
			&bytes_written, &error);
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
	if(error)
		g_error_free(error);
	g_io_channel_flush(base_station.channel, NULL);
}

void setBaseLcdOn()
{
	gsize bytes_written;
	GError *error = NULL;
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 1;
	if(config.serial_activated)
		g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
			&bytes_written, &error);
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
	if(error)
		g_error_free(error);
	g_io_channel_flush(base_station.channel, NULL);
}

void setBaseLcdOff()
{
	gsize bytes_written;
	GError *error = NULL;
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 2;
	if(config.serial_activated)
		g_io_channel_write_chars(base_station.channel, (char*)&headP, sizeof(headP),
			&bytes_written, &error);
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
	if(error)
		g_error_free(error);
	g_io_channel_flush(base_station.channel, NULL);
}

void open_door()
{
	verbose_printf(0,"Opening Door\n");
	relaisP.port |= 16;
	sendPacket(&relaisP, RELAIS_PACKET);
	if(hadState.beep_on_door_opened)
	{
		setBeepOn();
		g_usleep(1000000);
		setBeepOff();
	}
	else
		g_usleep(1000000);
	g_usleep(9000000);
	relaisP.port &= ~(16);
	sendPacket(&relaisP, RELAIS_PACKET);
}

void sendBaseLcdText(char *text)
{
	gsize bytes_written;
	GError *error = NULL;
	struct _lcd_text
	{
		struct headPacket headP;
		char text[33];
	}lcd_text;

	lcd_text.headP.address = 0x02;
	lcd_text.headP.count = 34;
	lcd_text.headP.command = 5;
	strncpy(lcd_text.text,text,32);
	lcd_text.text[32] = '\0';
	if(config.serial_activated)
		g_io_channel_write_chars(base_station.channel, (char*)&lcd_text, sizeof(lcd_text),
			&bytes_written, &error);
	//if(config.serial_activated)
	//	write(fd,&lcd_text,sizeof(lcd_text));
	if(error)
		g_error_free(error);
	g_io_channel_flush(base_station.channel, NULL);
}

static void incrementColor(uint8_t *color)
{
	*color +=64;
	if(*color == 0)
		*color = 255;
	if(*color == 63)
		*color = 0;
}

/*!
 *******************************************************************************
 * send data to the GLCD module connected to the base station
 *
 * the following data is transmitted: current date and time, last measured 
 * temperatures of outside and living room
 *******************************************************************************/
void updateGlcd()
{
	struct tm *ptm;
	time_t rawtime;
	
	time(&rawtime);
	ptm = localtime(&rawtime);

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
	
	glcdP.wecker = 0;
	sendPacket(&glcdP,GP_PACKET);
}

void base_station_rgb_blink_all(int num)
{
	int i, gpcounter;
	for(i=0;i<2;i++)
	{
		/* alle Module rot */
		sendRgbPacket(0x10,255,0,0,0);
		sendRgbPacket(0x11,255,0,0,0);
		sendRgbPacket(0x12,255,0,0,0);
		
		g_usleep(100000);
		
		/* vorherige Farben zurueckschreiben */
		for(gpcounter = 0; gpcounter < 3; gpcounter++)
		{
			sendRgbPacket(gpcounter+0x10, hadState.rgbModuleValues[gpcounter].red, 
				hadState.rgbModuleValues[gpcounter].green,
				hadState.rgbModuleValues[gpcounter].blue,
				0);
		}
		g_usleep(100000);
	}
}

