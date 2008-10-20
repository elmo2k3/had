/*
 * Copyright (C) 2007-2008 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

#include "network.h"
#include "had.h"
#include "serial.h"
#include "database.h"
#include "led_routines.h"

static int sock, leave;

static int net_blocked = 0;

pthread_t *network_client_thread[MAX_CLIENTS];

static void networkClientHandler(int client_sock);

static int8_t numConnectedClients = 0;

static void networkThreadStop(void)
{
	close(sock);
	leave = 1;
	verbose_printf(0, "NetworkThread stopped\n");
}

static void networkClientHandler(int client_sock)
{
	unsigned char buf[BUF_SIZE];
	int recv_size;

	int i;
	uint8_t modul,sensor;

	uint8_t relais;

	struct _rgbPacket rgbTemp;
	
	/* led-display stuff */
	uint16_t line_size;
	uint16_t led_count;
	char *led_line;

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
				if(rgbTemp.headP.address == 0x10)
					memcpy(&rgbP,&rgbTemp,sizeof(rgbTemp));
				else if(rgbTemp.headP.address == 0x11)
					memcpy(&rgbP1,&rgbTemp,sizeof(rgbTemp));
				else if(rgbTemp.headP.address == 0x12)
					memcpy(&rgbP2,&rgbTemp,sizeof(rgbTemp));
				
/*				rgbP.headP.address = GLCD_ADDRESS;
				rgbP.headP.command = RGB_PACKET;
				sendPacket(&rgbP,RGB_PACKET);
				rgbP.headP.command = 0;
*/
				break;

			case CMD_NETWORK_GET_RGB:
				send(client_sock,&rgbP, sizeof(rgbP),0);
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
						
						rgbP.smoothness = 0;
						rgbP1.smoothness = 0;
						rgbP2.smoothness = 0;
						usleep(100000);
						
						/* vorherige Farben zurueckschreiben */
						sendPacket(&rgbP,RGB_PACKET);
						sendPacket(&rgbP1,RGB_PACKET);
						sendPacket(&rgbP2,RGB_PACKET);
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

			case CMD_NETWORK_RELAIS:
				recv_size = recv(client_sock,&relais, sizeof(relais),0);
				relaisP.port = relais;
				verbose_printf(9, "Setting relais to ... %d\n",relaisP.port);
				sendPacket(&relaisP,RELAIS_PACKET);

				if(relaisP.port & 4)
				{
					if(config.led_matrix_activated && !ledIsRunning())
						pthread_create(&threads[2],NULL,(void*)&ledMatrixThread,NULL);
				}
				else
				{
					if(ledIsRunning())
						stopLedMatrixThread();
				}
					
				break;
			case CMD_NETWORK_GET_RELAIS:
				send(client_sock,&relaisP, sizeof(relaisP),0);
				break;
			
			case CMD_NETWORK_LED_DISPLAY_TEXT:
				recv(client_sock,&line_size, 2, 0);
				verbose_printf(9,"LED line_size: %d\n",line_size);
				led_line = malloc(sizeof(char)*line_size);
				recv(client_sock,&led_count, 2, 0);
				recv(client_sock,led_line,line_size,0);
				led_line[line_size] = '\0';

				ledPushToStack(led_line, COLOR_RED, 2, led_count);
				break;
						     
		}	
		usleep(1000);
	}while(buf[0] != CMD_NETWORK_QUIT);

	close(client_sock);
	numConnectedClients--;
}

void networkThread(void)
{
	struct sockaddr_in server,client;
	int client_sock;
	unsigned int len;
	int y=1;


	leave = 0;

	signal(SIGQUIT, (void*)networkThreadStop);
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
	}

	if(listen(sock, 5) == -1)
	{
		verbose_printf(0, "Error bei listen\n");
	}

	while(1)
	{
		len = sizeof(client);
		client_sock = accept(sock, (struct sockaddr*)&client, &len);
		verbose_printf(9, "Client %d connected\n",numConnectedClients+1);

		if(numConnectedClients < MAX_CLIENTS)
			pthread_create((void*)&network_client_thread[numConnectedClients++], NULL, (void*)networkClientHandler, (int*)client_sock);
		else
			verbose_printf(0, "Maximale Anzahl Clients erreicht (%d)\n",numConnectedClients);
	}
		
}




