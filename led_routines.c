/*
 * Copyright (C) 2008 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "led_routines.h"
#include "arial_bold_14.h"
#include "had.h"

#define LMS_IP "192.168.0.93"
#define LMS_PORT 9328

static void hilfsarray_to_normal(void);
static uint16_t charGetStart(char c);
static int initNetwork(void);

/* Diese Arrays werden nur zur Uebertragung ans Modul genutzt */
static uint16_t RED[4][16];
static uint16_t GREEN[4][16];

/* Dies sind die eigentlich genutzten Arrays. Grund: 
 * einfachere Handhabung! Jedes Element entspricht genau einer Spalte
 */
static uint16_t *column_red;
static uint16_t *column_green;

static uint16_t *column_red_current;
static uint16_t *column_green_current;

static uint16_t *column_mpd_red;
static uint16_t *column_mpd_green;

static uint16_t *column_general_red;
static uint16_t *column_general_green;

static int client_sock;
static int running;

static int position = 0;

static uint16_t *x,*y;

static int current_led_buffer;

int getCurrentLedBuffer(void)
{
	return current_led_buffer;
}

int ledIsRunning(void)
{
	return running;
}

void switchToBuffer(int numBuffer)
{
	static uint16_t x_mpd=0, x_general=0, y_mpd=0, y_general=0;
	switch(numBuffer)
	{
		case LED_BUFFER_MPD:
			x = &x_mpd;
			y = &y_mpd;
			column_red_current = column_mpd_red;
			column_green_current = column_mpd_green;
			current_led_buffer = numBuffer;
			break;
		case LED_BUFFER_GENERAL:
			x = &x_general;
			y = &y_general;
			column_red_current = column_general_red;
			column_green_current = column_general_green;
			current_led_buffer = numBuffer;
			break;
	}
}

void updateDisplay()
{
	int bytes_send;
	bytes_send = send(client_sock, &RED, sizeof(RED),0);
	bytes_send = send(client_sock, &GREEN, sizeof(GREEN),0);

}

/* Achtung, funktioniert derzeit nur fuer Arial_Bold_14 ! */
static uint16_t charGetStart(char c)
{
	uint8_t first_char = Arial_Bold_14[4];
	uint8_t char_height = Arial_Bold_14[3];

	uint8_t factor = 1;

	if(char_height > 8)
		factor = 2;

	uint8_t counter;
	uint16_t position = 0;

	for(counter=0;counter<(c-first_char);counter++)
	{
		position += Arial_Bold_14[6+counter]*factor;
	}

	return position;
}


static void hilfsarray_to_normal(void)
{
	int i,p,m;
	memset(&RED,0,sizeof(RED));
	memset(&GREEN,0,sizeof(GREEN));

	for(m=0;m<4;m++)
	{
		for(i=0;i<16;i++)
		{
			for(p=0;p<16;p++)
			{
				RED[m][i+*y] |= ((column_red[p+m*16] & (1<<i))>>(i)<<p);
				GREEN[m][i+*y] |= ((column_green[p+m*16] & (1<<i))>>(i)<<p);
			}
		}
	}
}

void putChar(char c, uint8_t color)
{
	/* Leerzeichen abfangen */
	if(c == 32)
	{
		*x += 4;
		return;
	}

	uint8_t first_char = Arial_Bold_14[4];
	uint8_t char_count = Arial_Bold_14[5];
	uint8_t char_width = Arial_Bold_14[6+c-first_char];

	uint16_t start = charGetStart(c);

	uint8_t i;

	for(i=0;i<char_width;i++)
	{
		if(color == COLOR_RED)
		{
			column_red_current[i+*x] = Arial_Bold_14[6+char_count+start+i];
		}
		else if(color == COLOR_GREEN)
		{
			column_green_current[i+*x] = Arial_Bold_14[6+char_count+start+i];
		}
		else if(color == COLOR_AMBER)
		{
			column_red_current[i+*x] = Arial_Bold_14[6+char_count+start+i];
			column_green_current[i+*x] = Arial_Bold_14[6+char_count+start+i];
		}
	}
	for(i=0;i<char_width;i++)
	{
		if(color == COLOR_RED)
		{
			/* Man erklaere mir was ich hier geschrieben. Aber funktionieren tuts! :-) */
			column_red_current[i+*x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
		}
		else if(color == COLOR_GREEN)
		{
			column_green_current[i+*x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
		}
		else if(color == COLOR_AMBER)
		{
			column_red_current[i+*x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
			column_green_current[i+*x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
		}
	}

	hilfsarray_to_normal();

	/* Bei Bedarf wieder an den Anfang gehen */
	if(*x + char_width +1 < 511)
		*x += char_width + 1;
	else
		*x = 0;
}

void putString(char *string, uint8_t color)
{
	if(!string)
		string = "null";
	while(*string)
	{
		putChar(*string++,color);
	}
}

void clearScreen(void)
{
	memset(column_red_current,0,sizeof(uint16_t)*512);
	memset(column_green_current,0,sizeof(uint16_t)*512);
	*x = 0;
	*y = 1;
}


int shiftLeft(void)
{
	int counter;
	
	position++;

	for(counter=0;counter< *x + 10;counter++)
	{
		if(position + counter > (*x + 10))
		{
			column_red[counter] = column_red_current[counter + position - (*x +11)];
			column_green[counter] = column_green_current[counter + position - (*x +11)];
		}
		else
		{
			column_red[counter] = column_red_current[position+counter];
			column_green[counter] = column_green_current[position+counter];
		}
	}

	
	hilfsarray_to_normal();
	
	if(position > *x + 10)
	{
		position = 0;
		return 0;
	}
	else
		return 1;
}

static int initNetwork(void)
{
	struct sockaddr_in server;

	client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(client_sock < 0)
		printf("Client_sock konnte nicht erstellt werden\n");
	server.sin_family = AF_INET;
	server.sin_port = htons(config.led_matrix_port);
	inet_aton(config.led_matrix_ip, &server.sin_addr);
	

	if(connect(client_sock, (struct sockaddr*)&server, sizeof(server)) != 0)
	{
		printf("Konnte nicht verbinden\n");
		return -1;
	}	

	return 0;
}

void stopLedMatrixThread()
{
	verbose_printf(9,"LedMatrixThread gestoppt\n");
	running = 0;
}

void ledMatrixThread(void)
{
	
	verbose_printf(9,"LedMatrixThread gestartet\n");
	int counter;

	time_t rawtime;
	struct tm *ptm;

	char time_string[20];

	running = 1;

	column_red = malloc(512 * sizeof(uint16_t));
	column_green = malloc(512 * sizeof(uint16_t));
	
	column_mpd_red = malloc(512 * sizeof(uint16_t));
	column_mpd_green = malloc(512 * sizeof(uint16_t));
	
	column_general_red = malloc(512 * sizeof(uint16_t));
	column_general_green = malloc(512 * sizeof(uint16_t));

	if(!column_red || !column_green || !column_mpd_red
			|| !column_mpd_green || !column_general_red
			|| !column_general_green)
	{
		printf("Could not allocate memory!\n");
		exit(EXIT_FAILURE);
	}

	switchToBuffer(LED_BUFFER_GENERAL);

	putString("00:00:00",COLOR_GREEN);

	switchToBuffer(LED_BUFFER_MPD);

	if(column_red == 0)
	{
		printf("Kein Speicher?\n");
		exit(EXIT_FAILURE);
	}
	initNetwork();
	clearScreen();

	while(running)
	{
//		memcpy(column_red,column_red_current,512 * sizeof(uint16_t));
//		memcpy(column_green,column_green_current,512 * sizeof(uint16_t));
		
		switchToBuffer(LED_BUFFER_MPD);

		while(shiftLeft())
		{
			updateDisplay();
			usleep(20000);
		}

		sleep(2);

		switchToBuffer(LED_BUFFER_GENERAL);
		
		for(counter=0;counter<10;counter++)
		{
			time(&rawtime);
			ptm = localtime(&rawtime);
			sprintf(time_string,"  %02d:%02d:%02d",ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
			clearScreen();
			putString(time_string,COLOR_RED);

			memcpy(column_red,column_red_current,512 * sizeof(uint16_t));
			memcpy(column_green,column_green_current,512 * sizeof(uint16_t));

			updateDisplay();
			sleep(1);
		}

	}
	
	free(column_red);
	free(column_green);
	
	free(column_mpd_red);
	free(column_mpd_green);
	
	free(column_general_red);
	free(column_general_green);

	close(client_sock);
}

