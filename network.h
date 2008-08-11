#ifndef __NETWORK_H__
#define __NETWORK_H__

#define LISTENER_PORT 4123

extern void networkThread(void);

#define CMD_NETWORK_RGB 1
#define CMD_NETWORK_GET_RGB 2
#define CMD_NETWORK_BLINK 3
#define CMD_NETWORK_GET_TEMPERATURE 4
#define CMD_NETWORK_GET_VOLTAGE 5
#define CMD_NETWORK_RELAIS 6
#define CMD_NETWORK_GET_RELAIS 7

#define BUF_SIZE 1024

//struct _rgbNetworkPacket
//{
//	int destination;
//	int red;
//	int green;
//	int blue;
//};
	
#endif

