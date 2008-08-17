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

static int sock, leave;

pthread_t *network_client_thread[MAX_CLIENTS];

static void networkClientHandler(int client_sock);

static int8_t numConnectedClients = 0;

static void networkThreadStop(void)
{
	close(sock);
	leave = 1;
	printf("NetworkThread stopped\n");
}

static void networkClientHandler(int client_sock)
{
	unsigned char buf[BUF_SIZE];
	int recv_size;

	int i;
	uint8_t modul,sensor;
	signed char celsius, decicelsius;

	uint8_t relais;
	do
	{
		buf[0] = 255;
		/* Erstes Byte = Kommando */
		recv_size = recv(client_sock, buf, 1, 0);
		switch(buf[0])
		{
			case CMD_NETWORK_RGB: 
				recv_size = recv(client_sock,&rgbP, sizeof(rgbP),0);
				sendPacket(&rgbP,RGB_PACKET);
				break;

			case CMD_NETWORK_GET_RGB:
				send(client_sock,&rgbP, sizeof(rgbP),0);
				break;

			case CMD_NETWORK_BLINK:
				for(i=0;i<2;i++)
				{
					sendRgbPacket(1,255,0,0,0);
					sendRgbPacket(3,255,0,0,0);
					
					rgbP.smoothness = 0;
					/* Achtung, hack: annahme beide Module gleiche Farbe */
					rgbP.headP.address = 1;
					sendPacket(&rgbP,RGB_PACKET);
					rgbP.headP.address = 3;
					sendPacket(&rgbP,RGB_PACKET);
				}
				break;

			case CMD_NETWORK_GET_TEMPERATURE:
				recv(client_sock,&modul, sizeof(modul), 0);
				recv(client_sock,&sensor, sizeof(sensor), 0);

				celsius = lastTemperature[modul][sensor][0];
				decicelsius = lastTemperature[modul][sensor][1];

				send(client_sock, &lastTemperature[modul][sensor][0], sizeof(celsius), 0);
				send(client_sock, &lastTemperature[modul][sensor][1], sizeof(celsius), 0);
				break;
			
			case CMD_NETWORK_RELAIS:
				recv_size = recv(client_sock,&relais, sizeof(relais),0);
				relaisP.port = relais;
				printf("Setting relais to ... %d\n",relaisP.port);
				sendPacket(&relaisP,RELAIS_PACKET);
				break;
			case CMD_NETWORK_GET_RELAIS:
				send(client_sock,&relaisP, sizeof(relaisP),0);
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
		printf("Listener Socket konnte nicht erstellt werden!\n");
	}
	
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(LISTENER_PORT);

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));

	if(bind(sock,(struct sockaddr*)&server, sizeof( server)) < 0)
	{
		printf("Konnte TCP Server nicht starten\n");
	}

	if(listen(sock, 5) == -1)
	{
		printf("Error bei listen\n");
	}

	while(1)
	{
		len = sizeof(client);
		client_sock = accept(sock, (struct sockaddr*)&client, &len);
		printf("Client %d connected\n",numConnectedClients+1);

		if(numConnectedClients < MAX_CLIENTS)
			pthread_create((void*)&network_client_thread[numConnectedClients++], NULL, (void*)networkClientHandler, (int*)client_sock);
		else
			printf("Maximale Anzahl Clients erreicht (%d)\n",numConnectedClients);
	}
		
}




