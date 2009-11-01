/*
 * Copyright (C) 2007-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file	had.c
 * \brief	main file for had
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include <sys/signal.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <sysexits.h>
#include <gmodule.h>

#include "had.h"
#include "daemon.h"
#include "misc.h"
#include "config.h"
#include "version.h"

GMainLoop *had_mainloop;

int main(int argc, char* argv[])
{
	GModule *mod_base_station;
	GModule *mod_rfid_tag_reader;
	int returnValue;
	
//	signal(SIGINT, (void*)hadSignalHandler);
//	signal(SIGTERM, (void*)hadSignalHandler);
	if(!loadConfig(HAD_CONFIG_FILE))
	{
		verbose_printf(0,"Could not load config ... aborting\n\n");
		exit(EX_NOINPUT);
	}

	if((returnValue = parseCmdLine(argc, argv)) != -1)
		return returnValue;

	if(config.daemonize)
	{
		daemonize();
	}
	
//#ifdef VERSION
//	verbose_printf(0, "had %s started\n",VERSION);
//#else
//	verbose_printf(0, "had started\n");
//#endif

	if(loadStateFile(config.statefile))
	{
		verbose_printf(9, "Statefile successfully read\n");
		relaisP.port = hadState.relais_state;
	}
	else
	{
		verbose_printf(9, "Statefile could not be read, using default values\n");
		memset(&hadState, 0, sizeof(hadState));
		hadState.scrobbler_user_activated = config.scrobbler_activated;
		hadState.ledmatrix_user_activated = config.led_matrix_activated;
		hadState.beep_on_door_opened = 1;
		hadState.beep_on_window_left_open = 1;
	}

	mod_base_station = g_module_open("./plugins/base_station/libbase_station.la",G_MODULE_BIND_LAZY);
	mod_rfid_tag_reader = g_module_open("./plugins/rfid_tag_reader/librfid_tag_reader.la",G_MODULE_BIND_LAZY);
	if(!mod_base_station)
	{
		g_warning("could not load mod_base_station");
	}
	had_mainloop = g_main_loop_new(NULL,FALSE);
//    g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG, logfunc, NULL);
    g_main_loop_run(had_mainloop);

//	if(config.hr20_database_activated || config.serial_activated || config.usbtemp_activated)
//		database_status = initDatabase();
//
//	if(database_status == -1 && !config.serial_activated)
//	{
//		while((database_status = initDatabase()) == -1);
//	}
//	
//	if(config.mpd_activated)
//		pthread_create(&threads[0],NULL,(void*)&mpdThread,NULL);	
//
//	pthread_create(&threads[1],NULL,(void*)&networkThread,NULL);
//
//	if(config.led_matrix_activated && (relaisP.port & 4) && hadState.ledmatrix_user_activated)
//	{
//		pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
//		pthread_detach(threads[2]);
//	}
//
////	if(config.usbtemp_activated)
////		pthread_create(&threads[3],NULL,(void*)&usbTempLoop,NULL);
//	if(config.hr20_database_activated)
//		pthread_create(&threads[4],NULL,(void*)&hr20thread,NULL);
//	
//	if(config.serial_activated)
//	{
//		if(!initSerial(config.tty)) // serielle Schnittstelle aktivieren
//		{
//			verbose_printf(0,"Serielle Schnittstelle konnte nicht geoeffnet werden!\n");
//			exit(EX_NOINPUT);
//		}
//
//		/* Falls keine Verbindung zum Mysql-Server aufgebaut werden kann, einfach
//		 * immer wieder versuchen
//		 *
//		 * Schlecht, ohne mysql server funktioniert das RF->Uart garnicht mehr
//		 */
//		/*while(initDatabase() == -1)
//		{
//			sleep(10);
//		}*/
//
//		glcdP.backlight = 1;
//
//		char buffer[1024];
//
//		if(database_status != -1)
//		{
//			getLastTemperature(3,1,&celsius,&decicelsius);
//			lastTemperature[3][1][0] = (int16_t)celsius;
//			lastTemperature[3][1][1] = (int16_t)decicelsius;
//
//			getLastTemperature(3,0,&celsius,&decicelsius);
//			lastTemperature[3][0][0] = (int16_t)celsius;
//			lastTemperature[3][0][1] = (int16_t)decicelsius;
//		}
//
//		sendBaseLcdText("had wurde gestartet ... ");
//
//		/* main loop */
//		while (1)
//		{
//			memset(buf,0,sizeof(buf));
//			res = readSerial(buf); // blocking read
//			if(res>1)
//			{
//				verbose_printf(9,"Res=%d\n",res);
//				time(&rawtime);
//				result = decodeStream(buf,&modul_id,&sensor_id,&celsius,&decicelsius,&voltage);
//				if( result == 1)
//				{
//					verbose_printf(9,"Modul ID: %d\t",modul_id);
//					verbose_printf(9,"Sensor ID: %d\t",sensor_id);
//					verbose_printf(9,"Temperatur: %d,%d\t",celsius,decicelsius);
//					switch(modul_id)
//					{
//						case 1: verbose_printf(9,"Spannung: %2.2f\r\n",ADC_MODUL_1/voltage); break;
//						case 3: verbose_printf(9,"Spannung: %2.2f\r\n",ADC_MODUL_3/voltage); break;
//						default: verbose_printf(9,"Spannung: %2.2f\r\n",ADC_MODUL_DEFAULT/voltage);
//					}		
//				
//					//rawtime -= 32; // Modul misst immer vor dem Schlafengehen
//					lastTemperature[modul_id][sensor_id][0] = (int16_t)celsius;
//					lastTemperature[modul_id][sensor_id][1] = (int16_t)decicelsius;
//					lastVoltage[modul_id] = voltage;
//					if(database_status == -1)
//						database_status = initDatabase();
//					if(database_status != -1)
//						databaseInsertTemperature(modul_id,sensor_id,celsius,decicelsius,rawtime);
//
//					sprintf(buf,"Aussen:  %2d.%2d CInnen:   %2d.%2d C",
//							lastTemperature[3][1][0],
//							lastTemperature[3][1][1]/100,
//							lastTemperature[3][0][0],
//							lastTemperature[3][0][1]/100);
//					sendBaseLcdText(buf);
//
//					if(lastTemperature[3][0][0] < 15 && !belowMinTemp &&
//							config.sms_activated)
//					{
//						char stringToSend[100];
//						belowMinTemp = 1;
//						sprintf(stringToSend,"%s had: Temperature is now %2d.%2d",theTime(),
//								lastTemperature[3][0][0],
//							lastTemperature[3][0][1]);
//						sms(stringToSend);
//					}
//					else if(lastTemperature[3][0][0] > 16)
//						belowMinTemp = 0;
//				}
//				else if(result == 2)
//				{
//					updateGlcd();
//					verbose_printf(9,"GraphLCD Info Paket gesendet\r\n");
//				}
//				else if(result == 3)
//				{
//					getDailyGraph(celsius,decicelsius, &graphP);
//					//sendPacket(&graphP,GRAPH_PACKET);
//					verbose_printf(9,"Graph gesendet\r\n");
//				}
//				else if(result == 4)
//				{
//					verbose_printf(9,"MPD Packet request\r\n");
//					sendPacket(&mpdP,MPD_PACKET);
//				}
//				else if(result == 10)
//				{
//					verbose_printf(0,"Serial Modul hard-reset\r\n");
//					sendBaseLcdText("Modul neu gestartet ....");
//				}
//				else if(result == 11)
//				{
//					verbose_printf(0,"Serial Modul Watchdog-reset\r\n");
//				}
//				else if(result == 12)
//				{
//					verbose_printf(0,"Serial Modul uart timeout\r\n");
//				}
//				else if(result == 30) // 1 open
//				{
//					verbose_printf(1,"Door opened\n");
//					hadState.input_state |= 1;
//					/* check for opened window */
//					if(hadState.input_state & 8)
//					{
//						verbose_printf(0,"Window and door open at the same time! BEEEEP\n");
//						if(hadState.beep_on_window_left_open)
//						{
//							setBeepOn();
//							sleep(1);
//							setBeepOff();
//						}
//					}
//					if(config.door_sensor_id && config.digital_input_module)
//						databaseInsertTemperature(config.digital_input_module,
//							config.door_sensor_id, 1, 0, rawtime);
//				}
//				else if(result == 31) // 1 closed
//				{
//					verbose_printf(1,"Door closed\n");
//					hadState.input_state &= ~1;
//					setBeepOff();
//					if(config.door_sensor_id && config.digital_input_module)
//						databaseInsertTemperature(config.digital_input_module,
//							config.door_sensor_id, 0, 0, rawtime);
//				}
//				else if(result == 32) // 2 open
//				{
//				}
//				else if(result == 33) // 2 closed
//				{
//				}
//				else if(result == 34) // 2 open
//				{
//				}
//				else if(result == 35) // 2 closed
//				{
//				}
//				else if(result == 36)
//				{
//						verbose_printf(1,"Window closed\n");
//						hadState.input_state &= ~8;
//						setBeepOff();
//						if(config.window_sensor_id && config.digital_input_module)
//							databaseInsertTemperature(config.digital_input_module,
//								config.window_sensor_id, 0, 0, rawtime);
//						if(config.hr20_activated && hr20info.tempset >= 50
//							&& hr20info.tempset <= 300)
//						{
//							hr20SetTemperature(hr20info.tempset);
//						}
//				}
//				else if(result == 37)
//				{
//					verbose_printf(1,"Window opened\n");
//					hadState.input_state |= 8;
//					if(config.window_sensor_id && config.digital_input_module)
//						databaseInsertTemperature(config.digital_input_module,
//							config.window_sensor_id, 1, 0, rawtime);
//					if(config.hr20_activated)
//					{
//						hr20GetStatus(&hr20info);
//						hr20SetTemperature(50);
//					}
//				}
//				else if(result == config.rkeys.mpd_random)
//				{
//
//					/* 50 - 82 reserved for remote control */
//					mpdToggleRandom();
//				}
//				else if(result == config.rkeys.mpd_prev)
//				{
//						verbose_printf(9,"MPD prev\r\n");
//						mpdPrev();
//				}
//				else if(result == config.rkeys.mpd_next)
//				{
//					verbose_printf(9,"MPD next song\r\n");
//					mpdNext();
//				}
//				else if(result == config.rkeys.mpd_play_pause)
//				{
//					mpdTogglePlayPause();
//				}
//				else if(result == config.rkeys.music_on_hifi_on)
//				{
//					system(SYSTEM_MOUNT_MPD);
//					relaisP.port |= 4;
//					sendPacket(&relaisP, RELAIS_PACKET);
//					if(relaisP.port & 4)
//					{
//						if(config.led_matrix_activated && !ledIsRunning())
//						{
//							pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
//							pthread_detach(threads[2]);
//							hadState.ledmatrix_user_activated = 1;
//						}
//					}
//					hadState.relais_state = relaisP.port;
//					mpdPlay();
//				}
//				else if(result == config.rkeys.everything_off)
//				{
//					relaisP.port = 0;
//					sendPacket(&relaisP, RELAIS_PACKET);
//					if(ledIsRunning())
//						stopLedMatrixThread();
//					hadState.relais_state = relaisP.port;
//					mpdPause();
//					for(gpcounter = 0; gpcounter < 3; gpcounter++)
//					{
//						hadState.rgbModuleValues[gpcounter].red = 0;
//						hadState.rgbModuleValues[gpcounter].green = 0;
//						hadState.rgbModuleValues[gpcounter].blue = 0;
//					}
//					setCurrentRgbValues();
//					system(SYSTEM_KILL_MPD);
//				}
//				else if(result == config.rkeys.hifi_on_off)
//				{
//					relaisP.port ^= 4;
//					sendPacket(&relaisP, RELAIS_PACKET);
//					if(relaisP.port & 4)
//					{
//						if(config.led_matrix_activated && !ledIsRunning())
//						{
//							pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
//							pthread_detach(threads[2]);
//							hadState.ledmatrix_user_activated = 1;
//						}
//					}
//					else
//					{
//						if(ledIsRunning())
//							stopLedMatrixThread();
//					}
//					hadState.relais_state = relaisP.port;
//				}
//				else if(result == config.rkeys.brightlight)
//				{
//					relaisP.port ^= 32;
//					sendPacket(&relaisP, RELAIS_PACKET);	
//					hadState.relais_state = relaisP.port;
//				}
//				else if(result == config.rkeys.light_off[0] || result == config.rkeys.light_off[1])
//				{
//					for(gpcounter = 0; gpcounter < 3; gpcounter++)
//					{
//						hadState.rgbModuleValues[gpcounter].red = 0;
//						hadState.rgbModuleValues[gpcounter].green = 0;
//						hadState.rgbModuleValues[gpcounter].blue = 0;
//					}
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.light_single_off[0])
//				{
//					hadState.rgbModuleValues[0].red = 0;
//					hadState.rgbModuleValues[0].green = 0;
//					hadState.rgbModuleValues[0].blue = 0;
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.light_single_off[1])
//				{
//					hadState.rgbModuleValues[1].red = 0;
//					hadState.rgbModuleValues[1].green = 0;
//					hadState.rgbModuleValues[1].blue = 0;
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.light_single_off[2])
//				{
//					hadState.rgbModuleValues[2].red = 0;
//					hadState.rgbModuleValues[2].green = 0;
//					hadState.rgbModuleValues[2].blue = 0;
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.light_on)
//				{
//					for(gpcounter = 0; gpcounter < 3; gpcounter++)
//					{
//						hadState.rgbModuleValues[gpcounter].red = 255;
//						hadState.rgbModuleValues[gpcounter].green = 255 ;
//						hadState.rgbModuleValues[gpcounter].blue = 0;
//					}
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.red)
//				{
//					for(gpcounter = 0; gpcounter < 3; gpcounter++)
//					{
//						incrementColor(&hadState.rgbModuleValues[gpcounter].red);
//					}
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.green)
//				{
//					for(gpcounter = 0; gpcounter < 3; gpcounter++)
//					{
//						incrementColor(&hadState.rgbModuleValues[gpcounter].green);
//					}
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.blue)
//				{
//					for(gpcounter = 0; gpcounter < 3; gpcounter++)
//					{
//						incrementColor(&hadState.rgbModuleValues[gpcounter].blue);
//					}
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.red_single[0])
//				{
//					incrementColor(&hadState.rgbModuleValues[0].red);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.red_single[1])
//				{
//					incrementColor(&hadState.rgbModuleValues[1].red);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.red_single[2])
//				{
//					incrementColor(&hadState.rgbModuleValues[2].red);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.green_single[0])
//				{
//					incrementColor(&hadState.rgbModuleValues[0].green);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.green_single[1])
//				{
//					incrementColor(&hadState.rgbModuleValues[1].green);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.green_single[2])
//				{
//					incrementColor(&hadState.rgbModuleValues[2].green);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.blue_single[0])
//				{
//					incrementColor(&hadState.rgbModuleValues[0].blue);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.blue_single[1])
//				{
//					incrementColor(&hadState.rgbModuleValues[1].blue);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.blue_single[2])
//				{
//					incrementColor(&hadState.rgbModuleValues[2].blue);
//					setCurrentRgbValues();
//				}
//				else if(result == config.rkeys.ledmatrix_toggle)
//				{
//					pthread_mutex_lock(&mutexLedmatrixToggle);
//					ledDisplayToggle();
//					pthread_mutex_unlock(&mutexLedmatrixToggle);
//				}
////				else if(result == config.rkeys.kill_and_unmount)
////				{
////					system("mpd --kill; sleep 5;umount /mnt/usbstick");
////				}
//				else if(result == 0)
//				{
//					verbose_printf(0,"decodeStream failed! Read line was: %s\r\n",buf);
//				}
//			} // endif res>1
//					
//		}
//	} // config.serial_activated
//	else
//	{
//	}
	return 0;
}

/*!
 *******************************************************************************
 * send data to the GLCD module connected to the base station
 *
 * the following data is transmitted: current date and time, last measured 
 * temperatures of outside and living room
 *******************************************************************************/
//void updateGlcd()
//{
//	struct tm *ptm;
//	time_t rawtime;
//	
//	time(&rawtime);
//	ptm = localtime(&rawtime);
//
//	glcdP.hour = ptm->tm_hour;
//	glcdP.minute = ptm->tm_min;
//	glcdP.second = ptm->tm_sec;
//	glcdP.day = ptm->tm_mday;
//	glcdP.month = ptm->tm_mon+1;
//	glcdP.year = ptm->tm_year;
//	glcdP.weekday = 0;
//	glcdP.temperature[0] = lastTemperature[3][1][0]; // draussen
//	glcdP.temperature[1] = lastTemperature[3][1][1]; 
//	glcdP.temperature[2] = lastTemperature[3][0][0]; // schlaf
//	glcdP.temperature[3] = lastTemperature[3][0][1];
//	
//	glcdP.wecker = 0;
//	sendPacket(&glcdP,GP_PACKET);
//}

static void incrementColor(uint8_t *color)
{
	*color +=64;
	if(*color == 0)
		*color = 255;
	if(*color == 63)
		*color = 0;
}
