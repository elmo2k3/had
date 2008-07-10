#ifndef __SERIAL_H__
#define __SERIAL_H__

#define BAUDRATE B19200

int initSerial(char *device);
int readSerial(char *buf);
void sendPacket(void *packet, int type);
void sendRgbPacket(unsigned char address, unsigned char red, unsigned char green, unsigned char blue, unsigned char smoothness);

#endif

