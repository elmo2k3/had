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
* \file	network.c
* \brief	had network server
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <openssl/md5.h>

#include "network.h"
#include "had.h"
#include "serial.h"
#include "led_routines.h"
#include "hr20.h"

static int sock, leave;

static int net_blocked = 0;

pthread_t network_client_thread[MAX_CLIENTS];

static void networkClientHandler(int client_sock);

static int8_t numConnectedClients = 0;

void networkThreadStop(void)
{
	leave = 1;
	verbose_printf(0, "NetworkThread stopped\n");
	close(sock);
}

static int networkAuthenticate(int client_sock)
{
	long int i;
	char buf[255];
	char pass_salted[200];
	uint32_t rawtime = (uint32_t)time(NULL);
	unsigned char digest[MD5_DIGEST_LENGTH];
	send(client_sock, &rawtime, sizeof(rawtime), 0);
	recv(client_sock, buf, MD5_DIGEST_LENGTH, 0);
	buf[MD5_DIGEST_LENGTH] = '\0';
	
	sprintf(pass_salted,"%s%ld",config.password, rawtime);
	MD5(pass_salted, strlen(pass_salted), digest);
	digest[MD5_DIGEST_LENGTH] = '\0';
	if(!strcmp(digest,buf))
	{
		return 1;
	}
	return 0;
}

static void networkClientHandler(int client_sock)
{
	/* very important:
	 * what we need here is a kind of "garbage collector" for unnormally
	 * exited client connections. every one of these leaves back a thread
	 * that will never be closed
	 */

	unsigned char buf[BUF_SIZE];
	int recv_size;

	int i, gpcounter;
	uint8_t modul,sensor;

	uint8_t relais;

	struct _rgbPacket rgbTemp;

	struct _hadState hadStateTemp;
	
	/* led-display stuff */
	uint16_t line_size;
	uint16_t led_count;

	char led_line[1024];

	struct _hr20info hr20info;
	int16_t temperature;
	int8_t slot;
	int8_t mode;

	if(!networkAuthenticate(client_sock) && config.password[0])
	{
		verbose_printf(0,"wrong password!\n");
		buf[0] = CMD_NETWORK_AUTH_FAILURE;
		send(client_sock, buf, 1, 0);
		close(client_sock);
		numConnectedClients--;
		return;
	}
	else
	{
		buf[0] = CMD_NETWORK_AUTH_SUCCESS;
		send(client_sock, buf, 1, 0);
	}
	do
	{
		buf[0] = 255;
		/* Erstes Byte = Kommando */
		recv_size = recv(client_sock, buf, 1, 0);
		switch(buf[0])
		{
			case CMD_NETWORK_RGB: 
				recv_size = recv(client_sock,&rgbTemp, sizeof(rgbTemp),0);
				sendPacket(&rgbTemp,RGB_PACKET);
				if(rgbTemp.headP.address >= 16 && rgbTemp.headP.address < 19)
				{
					hadState.rgbModuleValues[rgbTemp.headP.address-0x10].red = rgbTemp.red;
					hadState.rgbModuleValues[rgbTemp.headP.address-0x10].green = rgbTemp.green;
					hadState.rgbModuleValues[rgbTemp.headP.address-0x10].blue = rgbTemp.blue;
					hadState.rgbModuleValues[rgbTemp.headP.address-0x10].smoothness = rgbTemp.smoothness;
				}
				else
					verbose_printf(0,"RGB-Module address %d out of range!\n",rgbTemp.headP.address);
				break;
			case CMD_NETWORK_BLINK:
				if(!net_blocked)
				{
					net_blocked = 1;
					for(i=0;i<2;i++)
					{
						/* alle Module rot */
						sendRgbPacket(0x10,255,0,0,0);
						sendRgbPacket(0x11,255,0,0,0);
						sendRgbPacket(0x12,255,0,0,0);
						
						usleep(100000);
						
						/* vorherige Farben zurueckschreiben */
						for(gpcounter = 0; gpcounter < 3; gpcounter++)
						{
							sendRgbPacket(gpcounter+0x10, hadState.rgbModuleValues[gpcounter].red, 
								hadState.rgbModuleValues[gpcounter].green,
								hadState.rgbModuleValues[gpcounter].blue,
								0);
						}
						usleep(100000);
					}
					net_blocked = 0;
				}
				break;

			case CMD_NETWORK_GET_TEMPERATURE:
				recv(client_sock,&modul, sizeof(modul), 0);
				recv(client_sock,&sensor, sizeof(sensor), 0);

				send(client_sock, &lastTemperature[modul][sensor][0], sizeof(int16_t), 0);
				send(client_sock, &lastTemperature[modul][sensor][1], sizeof(int16_t), 0);
				break;
		
			case CMD_NETWORK_GET_VOLTAGE:
				recv(client_sock,&modul,sizeof(modul), 0);
				send(client_sock, &lastVoltage[modul], sizeof(int16_t), 0);
				break;

			case CMD_NETWORK_GET_RELAIS:
				send(client_sock,&relaisP, sizeof(relaisP),0);
				break;
			
			case CMD_NETWORK_LED_DISPLAY_TEXT:
				recv(client_sock,&line_size, 2, 0);
				verbose_printf(9,"LED line_size: %d\n",line_size);
				recv(client_sock,&led_count, 2, 0);
				recv(client_sock,led_line,line_size,0);
				led_line[line_size] = '\0';

				ledPushToStack(led_line, 2, led_count);
				break;
			
			case CMD_NETWORK_BASE_LCD_ON:
				setBaseLcdOn();
				glcdP.backlight = 1;
				updateGlcd();
				break;

			case CMD_NETWORK_BASE_LCD_OFF:
				setBaseLcdOff();
				glcdP.backlight = 0;
				updateGlcd();
				break;

			case CMD_NETWORK_BASE_LCD_TEXT:
				recv(client_sock, &line_size, 2, 0);
				verbose_printf(9,"LCD line_size: %d\n",line_size);
				recv(client_sock, led_line, line_size, 0);
				sendBaseLcdText(led_line);
				break;

			case CMD_NETWORK_GET_HAD_STATE:
				send(client_sock, &hadState, sizeof(hadState),0);
				break;

			case CMD_NETWORK_SET_HAD_STATE:
				recv(client_sock, &hadStateTemp, sizeof(hadState),0);

				// check if user switched off ledmatrix
				if(hadStateTemp.ledmatrix_user_activated != 
						hadState.ledmatrix_user_activated)
				{
					if(hadStateTemp.ledmatrix_user_activated &&
							config.led_matrix_activated &&
							!ledIsRunning() &&
							(hadState.relais_state & 4))
					{
						pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
						pthread_detach(threads[2]);
					}
					else if(!hadStateTemp.ledmatrix_user_activated &&
							ledIsRunning())
					{
						stopLedMatrixThread();
					}
					else
						hadStateTemp.ledmatrix_user_activated = 0;
				}

				// check if state of hifi rack changed
				if(hadStateTemp.relais_state != hadState.relais_state)
				{
					if(hadStateTemp.relais_state & 4)
					{
						if(config.led_matrix_activated && !ledIsRunning() &&
								hadStateTemp.ledmatrix_user_activated)
						{
							pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
							pthread_detach(threads[2]);
						}
					}
					else
					{
						if(ledIsRunning())
							stopLedMatrixThread();
					}
					relaisP.port = hadStateTemp.relais_state;
					sendPacket(&relaisP,RELAIS_PACKET);
				}


				memcpy(&hadState, &hadStateTemp, sizeof(hadState));
				break;

			case CMD_NETWORK_GET_HR20:
				memset(&hr20info, 0, sizeof(hr20info));
				if(config.hr20_activated)
					hr20GetStatus(&hr20info);
				send(client_sock, &hr20info, sizeof(hr20info), 0);
				break;

			case CMD_NETWORK_SET_HR20_TEMPERATURE:
				recv(client_sock, &temperature, sizeof(temperature),0);
				if(config.hr20_activated)
					hr20SetTemperature((int)temperature);
				break;

			case CMD_NETWORK_SET_HR20_AUTO_TEMPERATURE:
				recv(client_sock, &slot, sizeof(slot),0);
				recv(client_sock, &temperature, sizeof(temperature),0);
				if(config.hr20_activated)
					hr20SetAutoTemperature((int)slot, (int)temperature);
				break;
			
			case CMD_NETWORK_SET_HR20_MODE:
				recv(client_sock, &mode, sizeof(mode),0);
				if(mode == HR20_MODE_MANU)
				{
					if(config.hr20_activated)
						hr20SetModeManu();
				}
				else if(mode == HR20_MODE_AUTO)
				{
					if(config.hr20_activated)
						hr20SetModeAuto();
				}

				break;
			case CMD_NETWORK_OPEN_DOOR:
				open_door();
				break;
		}

		usleep(1000);
	// recv is blocking ... if we get a zero from it, we assume the connection
	// has been terminated
	if(!recv_size)
		verbose_printf(1,"Connection ungracefully terminated ... \n");
	}while(buf[0] != CMD_NETWORK_QUIT && recv_size);

	close(client_sock);
	numConnectedClients--;
}

void networkThread(void)
{
	struct sockaddr_in server,client;
	int client_sock;
	unsigned int len;
	int y=1;


	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if( sock < 0 )
	{
		verbose_printf(0, "Listener Socket konnte nicht erstellt werden!\n");
	}
	
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(LISTENER_PORT);

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));

	if(bind(sock,(struct sockaddr*)&server, sizeof( server)) < 0)
	{
		verbose_printf(0, "Konnte TCP Server nicht starten\n");
		return;
	}

	if(listen(sock, 5) == -1)
	{
		verbose_printf(0, "Error bei listen\n");
		return;
	}

	while(1)
	{
		len = sizeof(client);
		client_sock = accept(sock, (struct sockaddr*)&client, &len);
		verbose_printf(9, "Client %d connected\n",numConnectedClients+1);

		if(numConnectedClients < MAX_CLIENTS)
		{
			pthread_create(&network_client_thread[numConnectedClients++], NULL, (void*)networkClientHandler, (int*)client_sock);
			pthread_detach(network_client_thread[numConnectedClients-1]);
		}
		else
			verbose_printf(0, "Maximale Anzahl Clients erreicht (%d)\n",numConnectedClients);
	}
		
}




