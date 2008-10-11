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
#include "mpd.h"

#define LMS_IP "192.168.0.93"
#define LMS_PORT 9328

static uint16_t charGetStart(char c);
static int initNetwork(void);
static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed);

/* Diese Arrays werden nur zur Uebertragung ans Modul genutzt */
static uint16_t RED[4][16];
static uint16_t GREEN[4][16];

static struct _ledLine ledLineStack[LED_MAX_STACK];

int led_line_stack_time[LED_MAX_STACK];
int led_line_stack_shift[LED_MAX_STACK];

static int client_sock;
static int running;

int led_stack_size = 0;


int ledIsRunning(void)
{
	return running;
}

void updateDisplay(struct _ledLine ledLine)
{
	int bytes_send;
	int i,p,m;

	memset(&RED,0,sizeof(RED));
	memset(&GREEN,0,sizeof(GREEN));

	for(m=0;m<4;m++)
	{
		for(i=0;i<16;i++)
		{
			for(p=0;p<16;p++)
			{
				/* was there a shift yet? if no, print the unshifted arrays */
				if(ledLine.shift_position)
				{
					RED[m][i+ledLine.y] |= ((ledLine.column_red_output[p+m*16] & (1<<i))>>(i)<<p);
					GREEN[m][i+ledLine.y] |= ((ledLine.column_green_output[p+m*16] & (1<<i))>>(i)<<p);
				}
				else
				{
					RED[m][i+ledLine.y] |= ((ledLine.column_red[p+m*16] & (1<<i))>>(i)<<p);
					GREEN[m][i+ledLine.y] |= ((ledLine.column_green[p+m*16] & (1<<i))>>(i)<<p);
				}

			}
		}
	}
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


int putChar(char c, uint8_t color, struct _ledLine *ledLine)
{

	uint8_t first_char = Arial_Bold_14[4];
	uint8_t char_count = Arial_Bold_14[5];
	uint8_t char_width;

	uint16_t start;

	uint8_t i;

	/* if char is not in our font just leave */
	if(c < first_char || c > (first_char + char_count))
		return 1;
	
	/* Leerzeichen abfangen */
	if(c == 32)
		char_width = 4;
	else
	{
		char_width = Arial_Bold_14[6+c-first_char];
		start = charGetStart(c);
	}
	
	if((ledLine->x + char_width) >= LINE_LENGTH-50)
		return 0;

	if(c == 32)
	{
		ledLine->x += 4;
		return 1;
	}

	for(i=0;i<char_width;i++)
	{
		if(color == COLOR_RED)
		{
			ledLine->column_red[i+ledLine->x] = Arial_Bold_14[6+char_count+start+i];
		}
		else if(color == COLOR_GREEN)
		{
			ledLine->column_green[i+ledLine->x] = Arial_Bold_14[6+char_count+start+i];
		}
		else if(color == COLOR_AMBER)
		{
			ledLine->column_red[i+ledLine->x] = Arial_Bold_14[6+char_count+start+i];
			ledLine->column_green[i+ledLine->x] = Arial_Bold_14[6+char_count+start+i];
		}
	}
	/* unteren Teil der Zeichen schreiben (noch nicht dynamisch fuer verschiedene Schriftgroessen) */
	for(i=0;i<char_width;i++)
	{
		if(color == COLOR_RED)
		{
			/* Man erklaere mir was ich hier geschrieben. Aber funktionieren tuts! :-) */
			ledLine->column_red[i+ledLine->x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
		}
		else if(color == COLOR_GREEN)
		{
			ledLine->column_green[i+ledLine->x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
		}
		else if(color == COLOR_AMBER)
		{
			ledLine->column_red[i+ledLine->x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
			ledLine->column_green[i+ledLine->x] |= Arial_Bold_14[6+char_count+start+i+char_width]<<6;
		}
	}

	ledLine->x += char_width + 1;

	return 1;
}

void putString(char *string, uint8_t color, struct _ledLine *ledLine)
{
	if(!string)
		string = "null";
	while(*string)
	{
		if(!putChar(*string++,color,ledLine))
			return;
	}
}

void clearScreen(struct _ledLine *ledLine)
{
	memset(ledLine->column_red,0,sizeof(uint16_t)*LINE_LENGTH);
	memset(ledLine->column_green,0,sizeof(uint16_t)*LINE_LENGTH);
	
	memset(ledLine->column_red_output,0,sizeof(uint16_t)*LINE_LENGTH);
	memset(ledLine->column_green_output,0,sizeof(uint16_t)*LINE_LENGTH);

	ledLine->x = 0;
	ledLine->y = 1;
}


int shiftLeft(struct _ledLine *ledLine)
{
	int counter;
	
	for(counter=0;counter< ledLine->x + 10;counter++)
	{
		if(ledLine->shift_position + counter > (ledLine->x + 10))
		{
			ledLine->column_red_output[counter] = ledLine->column_red[counter + ledLine->shift_position - (ledLine->x +11)];
			ledLine->column_green_output[counter] = ledLine->column_green[counter + ledLine->shift_position - (ledLine->x +11)];
		}
		else
		{
			ledLine->column_red_output[counter] = ledLine->column_red[ledLine->shift_position+counter];
			ledLine->column_green_output[counter] = ledLine->column_green[ledLine->shift_position+counter];
		}
	}

	ledLine->shift_position++;
	
	if(ledLine->shift_position > ledLine->x + 11)
	{
		ledLine->shift_position = 1;
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
		verbose_printf(0,"Keine Verbindung zum LED-Modul\n");
	server.sin_family = AF_INET;
	server.sin_port = htons(config.led_matrix_port);
	inet_aton(config.led_matrix_ip, &server.sin_addr);
	

	if(connect(client_sock, (struct sockaddr*)&server, sizeof(server)) != 0)
	{
		verbose_printf(0,"Konnte nicht zum LED-Modul verbinden\n");
		return -1;
	}	

	return 0;
}

void stopLedMatrixThread()
{
	verbose_printf(9,"LedMatrixThread gestoppt\n");
	running = 0;
}

void allocateLedLine(struct _ledLine *ledLine, int line_length)
{
	ledLine->column_red = malloc(sizeof(uint16_t)*line_length);
	ledLine->column_green = malloc(sizeof(uint16_t)*line_length);
	
	ledLine->column_red_output = malloc(sizeof(uint16_t)*line_length);
	ledLine->column_green_output = malloc(sizeof(uint16_t)*line_length);
	
	clearScreen(ledLine);

	ledLine->x = 0;
	ledLine->y = 0;
	ledLine->shift_position = 0;
}

void freeLedLine(struct _ledLine ledLine)
{
	free(ledLine.column_red);
	free(ledLine.column_green);
	
	free(ledLine.column_red_output);
	free(ledLine.column_green_output);
}

void ledPushToStack(char *string, int color, int shift, int lifetime)
{
	int x;
	verbose_printf(9,"String pushed to stack: %s\n",string);
	allocateLedLine(&ledLineStack[led_stack_size], LINE_LENGTH);
	putString(string, color, &ledLineStack[led_stack_size]);
	
	x = ledLineStack[led_stack_size].x;

	lifetime *= (x+11);
	if(x > 64)
		lifetime -= 64;
	else
		shift = 0;
	led_line_stack_time[led_stack_size] = lifetime;
	led_line_stack_shift[led_stack_size] = shift;
	led_stack_size++;
	running = 1;
}

static void ledPopFromStack(void)
{
	verbose_printf(9,"String popped from stack\n");
	freeLedLine(ledLineStack[led_stack_size-1]);
	led_stack_size--;

}

void ledMatrixThread(void)
{
	verbose_printf(9,"LedMatrixThread gestartet\n");

	struct _ledLine ledLineTime;
	struct _ledLine *ledLineToDraw;
	int shift_speed = 0;
	time_t rawtime;
	struct tm *ptm;


	char time_string[20];

	running = 1;
	
	allocateLedLine(&ledLineTime, LINE_LENGTH);

	initNetwork();

	ledLineToDraw = &ledLineTime;
	while(running)
	{
		if(led_stack_size)
		{
			ledLineToDraw = &ledLineStack[led_stack_size-1];
			led_line_stack_time[led_stack_size-1]--;
			shift_speed = led_line_stack_shift[led_stack_size-1];
			if(!led_line_stack_time[led_stack_size-1])
			{
				ledPopFromStack();
			}
		}
		/* Important! else doesn't work here */
		if(!led_stack_size)
		{
			if(mpdGetState() == MPD_PLAYER_PLAY)
			{
				ledLineToDraw = &ledLineMpd;
				shift_speed = 2;
			}
			else
			{
				time(&rawtime);
				ptm = localtime(&rawtime);
				sprintf(time_string,"  %02d:%02d:%02d",ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
				clearScreen(&ledLineTime);
				putString(time_string,COLOR_RED,&ledLineTime);
				ledLineToDraw = &ledLineTime;
				shift_speed = 0;
			}
		}
		
		ledDisplayMain(ledLineToDraw, shift_speed);
	}

	freeLedLine(ledLineTime);
	
	close(client_sock);

	pthread_exit(0);
}

static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed)
{
	static int counter = 0;
	if(shift_speed)
	{
		shiftLeft(ledLineToDraw);
		updateDisplay(*ledLineToDraw);
		usleep(20000);
	}
	else
	{
		updateDisplay(*ledLineToDraw);
		usleep(1000);
	}
}

