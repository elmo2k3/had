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
#include "main.h"
#include "serial.h"

static int sock, leave;

static void networkThreadStop(void)
{
	close(sock);
	leave = 1;
	printf("NetworkThread stopped\n");
}

void networkThread(void)
{
	struct sockaddr_in server,client;
	int client_sock;
	unsigned int len;
	int y=1;

	unsigned char buf[BUF_SIZE];
	int recv_size;

	int i;

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
		
		/* Erstes Byte = Kommando */
		recv_size = recv(client_sock, buf, 1, 0);
		switch(buf[0])
		{
			case CMD_NETWORK_RGB: 
				recv_size = recv(client_sock,&rgbP, sizeof(rgbP),0);
				printf("RGB Destination: %d Red: %d Green: %d Blue: %d Smoothness: %d\n",
						rgbP.headP.address,
						rgbP.red,
						rgbP.green,
						rgbP.blue,
						rgbP.smoothness);
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
//					usleep(50000);

					/* Achtung, hack: annahme beide Module gleiche Farbe */
					rgbP.headP.address = 1;
					sendPacket(&rgbP,RGB_PACKET);
					rgbP.headP.address = 3;
					sendPacket(&rgbP,RGB_PACKET);
//					usleep(50000);
				}
				break;
						     
		}	
		if(leave == 1)
			return;
//		printf("Verbindung...\n");
		close(client_sock);
	}

}




