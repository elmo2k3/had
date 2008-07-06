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
		if(leave == 1)
			return;
		printf("Verbindung...\n");
		close(client_sock);
	}

}


