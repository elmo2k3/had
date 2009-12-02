/*
 * Copyright (C) 2008-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

/*!
* \file	led_routines.c
* \brief	functions for the ledmatrix display
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
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
#include <glib.h>
#include <gmodule.h>

#include <had.h>
#include <config.h>

#include "led_matrix.h"
#include "fonts/arial_bold_14.h"
#include "fonts/arial_8.h"
#include "fonts/Comic_8.h"
#include "fonts/Comic_9.h"
#include "fonts/Comic_10.h"

static GMutex *led_matrix_mutex = NULL;
static gboolean led_matrix_exit;

static uint16_t charGetStart(char c);
static int initNetwork(void);
static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed);

//static uint8_t *font = &Arial_Bold_14;
static uint8_t *font = Arial_Bold_14;

/* Diese Arrays werden nur zur Uebertragung ans Modul genutzt */
static uint16_t RED[4][16];
static uint16_t GREEN[4][16];

static struct _ledLine ledLineStack[LED_MAX_STACK];

int led_line_stack_time[LED_MAX_STACK];
int led_line_stack_shift[LED_MAX_STACK];

static int client_sock;
static int running;
static int toggle;

volatile int led_stack_size = 0;

gpointer *ledMatrixThread(gpointer user_data);

gchar *g_module_check_init(void)
{
	GError *err = NULL;
	
	led_matrix_mutex = g_mutex_new();

	led_matrix_exit = FALSE;

	g_debug("module led_matrix loaded");
	g_thread_create(ledMatrixThread, NULL, FALSE, &err);

	if(err)
	{
		g_debug("led_matrix.c: error %s",err->message);
		g_error_free(err);
	}
	return NULL;
}

void g_module_unload(GModule *module)
{
	g_mutex_lock(led_matrix_mutex);
	led_matrix_exit = TRUE;
	g_mutex_unlock(led_matrix_mutex);
}

int ledIsRunning(void)
{
	return running;
}

void updateDisplay(struct _ledLine ledLine)
{
	int bytes_send;
	int i,p,m;
	
	g_mutex_lock(led_matrix_mutex);
	memset(&RED,0,sizeof(RED));
	memset(&GREEN,0,sizeof(GREEN));

	for(m=0;m<4;m++) // for every module
	{
		for(i=0;i<16;i++) // for every row
		{
			for(p=0;p<16;p++) // for every single led in row
			{
#ifdef LED_HEADFIRST
				/* was there a shift yet? if no, print the unshifted arrays */
				if(ledLine.shift_position)
				{
					RED[3-m][15-i] |= ((ledLine.column_red_output[15-p+m*16] & (1<<i))>>(i)<<p);
					GREEN[3-m][15-i] |= ((ledLine.column_green_output[15-p+m*16] & (1<<i))>>(i)<<p);
				}
				else
				{
					RED[3-m][15-i] |= ((ledLine.column_red[15-p+m*16] & (1<<i))>>(i)<<p);
					GREEN[3-m][15-i] |= ((ledLine.column_green[15-p+m*16] & (1<<i))>>(i)<<p);
				}
#else
				/* was there a shift yet? if no, print the unshifted arrays */
				if(ledLine.shift_position)
				{
					RED[m][i] |= ((ledLine.column_red_output[p+m*16] & (1<<i))>>(i)<<p);
					GREEN[m][i] |= ((ledLine.column_green_output[p+m*16] & (1<<i))>>(i)<<p);
				}
				else
				{
					RED[m][i] |= ((ledLine.column_red[p+m*16] & (1<<i))>>(i)<<p);
					GREEN[m][i] |= ((ledLine.column_green[p+m*16] & (1<<i))>>(i)<<p);
				}
#endif

			}
		}
	}
	bytes_send = send(client_sock, &RED, sizeof(RED),0);
	bytes_send = send(client_sock, &GREEN, sizeof(GREEN),0);
	g_mutex_unlock(led_matrix_mutex);

}

/* Achtung, funktioniert derzeit nur fuer font ! */
static uint16_t charGetStart(char c)
{
	uint8_t first_char = font[4];
	uint8_t char_height = font[3];

	uint8_t factor = 1;

	if(char_height > 8)
		factor = 2;

	uint8_t counter;
	uint16_t position = 0;

	for(counter=0;counter<(c-first_char);counter++)
	{
		position += font[6+counter]*factor;
	}

	return position;
}

int charWidth(char c)
{
	if(c == '\a' || c == '\b' || c == '\n' || c == '\r')
		return 0;
	else if(c == 32)
		return 4;
	else
		return font[6+c-font[4]] + 1;
}


int stringWidth(char *string)
{
	int width = 0;
	if(!string)
		string = "null";
	while(*string)
	{
		width += charWidth(*string++);
	}
	return width - 1;
}


int putChar(char c, uint8_t color, struct _ledLine *ledLine)
{

	uint8_t first_char = font[4];
	uint8_t char_count = font[5];
	uint8_t char_width;
	uint8_t char_height = font[3];

	uint16_t start;

	uint8_t i;

	if(c == '\n')
	{
		g_mutex_lock(led_matrix_mutex);
		ledLine->x = 0;
		ledLine->y = 8;
		g_mutex_unlock(led_matrix_mutex);
		return 1;
	}

	/* if char is not in our font just leave */
	if(c < first_char || c > (first_char + char_count))
		return 1;
	
	/* Leerzeichen abfangen */
	if(c == 32)
		char_width = 4;
	else
	{
		char_width = font[6+c-first_char];
		start = charGetStart(c);
	}
	
	if((ledLine->x + char_width) >= LINE_LENGTH-50)
		return 0;

	if(c == ' ')
	{
		g_mutex_lock(led_matrix_mutex);
		ledLine->x += 4;
		g_mutex_unlock(led_matrix_mutex);
		return 1;
	}

	g_mutex_lock(led_matrix_mutex);

	for(i=0;i<char_width;i++)
	{
		if(color == COLOR_RED)
		{
			ledLine->column_red[i+ledLine->x] |= font[6+char_count+start+i]<<ledLine->y;
		}
		else if(color == COLOR_GREEN)
		{
			ledLine->column_green[i+ledLine->x] |= font[6+char_count+start+i]<<ledLine->y;
		}
		else if(color == COLOR_AMBER)
		{
			ledLine->column_red[i+ledLine->x] |= font[6+char_count+start+i]<<ledLine->y;
			ledLine->column_green[i+ledLine->x] |= font[6+char_count+start+i]<<ledLine->y;
		}
	}
	/* unteren Teil der Zeichen schreiben (noch nicht dynamisch fuer verschiedene Schriftgroessen) */
	for(i=0;i<char_width;i++)
	{
		if(color == COLOR_RED)
		{
			/* Man erklaere mir was ich hier geschrieben. Aber funktionieren tuts! :-) */
			ledLine->column_red[i+ledLine->x] |= font[6+char_count+start+i+char_width]<<(char_height - 8)<<ledLine->y;
		}
		else if(color == COLOR_GREEN)
		{
			ledLine->column_green[i+ledLine->x] |= font[6+char_count+start+i+char_width]<<(char_height - 8)<<ledLine->y;
		}
		else if(color == COLOR_AMBER)
		{
			ledLine->column_red[i+ledLine->x] |= font[6+char_count+start+i+char_width]<<(char_height - 8)<<ledLine->y;
			ledLine->column_green[i+ledLine->x] |= font[6+char_count+start+i+char_width]<<(char_height -8)<<ledLine->y;
		}
	}

	ledLine->x += char_width + 1;
	g_mutex_unlock(led_matrix_mutex);

	return 1;
}

void putString(char *string, struct _ledLine *ledLine)
{
	int color = ledLine->current_color;
	if(!string)
		string = "null";
	while(*string)
	{
		if(*string == '\b')
		{
			g_mutex_lock(led_matrix_mutex);
			ledLine->current_color = COLOR_GREEN;
			g_mutex_unlock(led_matrix_mutex);
		}
		else if(*string == '\r')
		{
			g_mutex_lock(led_matrix_mutex);
			ledLine->current_color = COLOR_RED;
			g_mutex_unlock(led_matrix_mutex);
		}
		else if(*string == '\a')
		{
			g_mutex_lock(led_matrix_mutex);
			ledLine->current_color = COLOR_AMBER;
			g_mutex_unlock(led_matrix_mutex);
		}
		else if(!putChar(*string,color,ledLine))
			return;
		string++;
	}
}

void clearScreen(struct _ledLine *ledLine)
{
	g_mutex_lock(led_matrix_mutex);
	memset(ledLine->column_red,0,sizeof(uint16_t)*LINE_LENGTH);
	memset(ledLine->column_green,0,sizeof(uint16_t)*LINE_LENGTH);
	
	memset(ledLine->column_red_output,0,sizeof(uint16_t)*LINE_LENGTH);
	memset(ledLine->column_green_output,0,sizeof(uint16_t)*LINE_LENGTH);

	ledLine->x = 0;
	ledLine->y = 1;
	g_mutex_unlock(led_matrix_mutex);
}


int shiftLeft(struct _ledLine *ledLine)
{
	int counter;

	int scroll_length;
	
	g_mutex_lock(led_matrix_mutex);
	if(ledLine->x + 11 > LINE_LENGTH)
		scroll_length = LINE_LENGTH;
	else
		scroll_length = ledLine->x + 11;

	for(counter=0;counter< scroll_length - 1  ;counter++)
	{
		if(ledLine->shift_position + counter > scroll_length - 1)
		{
			ledLine->column_red_output[counter] = ledLine->column_red[counter + ledLine->shift_position - (scroll_length)];
			ledLine->column_green_output[counter] = ledLine->column_green[counter + ledLine->shift_position - (scroll_length)];
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
		g_mutex_unlock(led_matrix_mutex);
		return 0;
	}
	
	g_mutex_unlock(led_matrix_mutex);
	return 1;
}

static int initNetwork(void)
{
	struct sockaddr_in server;

	client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(client_sock < 0)
		g_debug("Keine Verbindung zum LED-Modul\n");
	server.sin_family = AF_INET;
	server.sin_port = htons(hadConfigGetInt("mod_led_matrix","port",9328));
	inet_aton(hadConfigGetString("mod_led_matrix","host","192.168.0.93"), &server.sin_addr);
	

	if(connect(client_sock, (struct sockaddr*)&server, sizeof(server)) != 0)
	{
		g_debug("Konnte nicht zum LED-Modul verbinden\n");
		return -1;
	}	

	return 0;
}

void stopLedMatrixThread()
{
	g_debug("LedMatrixThread gestoppt\n");
	running = 0;
}

int allocateLedLine(struct _ledLine *ledLine, int line_length)
{
	g_mutex_lock(led_matrix_mutex);
	ledLine->column_red = calloc(sizeof(uint16_t), line_length);
	if(!ledLine->column_red)
		return 0;
	ledLine->column_green = calloc(sizeof(uint16_t), line_length);
	if(!ledLine->column_green)
		return 0;
	
	ledLine->column_red_output = calloc(sizeof(uint16_t), line_length);
	if(!ledLine->column_red_output)
		return 0;
	ledLine->column_green_output = calloc(sizeof(uint16_t), line_length);
	if(!ledLine->column_green_output)
		return 0;
	
	g_mutex_unlock(led_matrix_mutex);

	clearScreen(ledLine);

	g_mutex_lock(led_matrix_mutex);
	ledLine->x = 0;
	ledLine->y = 1;
	ledLine->shift_position = 0;
	g_mutex_unlock(led_matrix_mutex);
	return 1;
}

void freeLedLine(struct _ledLine *ledLine)
{
	g_mutex_lock(led_matrix_mutex);
	free(ledLine->column_red);
	free(ledLine->column_green);
	
	free(ledLine->column_red_output);
	free(ledLine->column_green_output);
	g_mutex_unlock(led_matrix_mutex);
}

void ledPushToStack(char *string, int shift, int lifetime)
{
	if(!running)
		return;
	g_mutex_lock(led_matrix_mutex);
	if(led_stack_size + 1 < LED_MAX_STACK)
	{
		int x;
		if(!allocateLedLine(&ledLineStack[led_stack_size], LINE_LENGTH))
		{
			g_mutex_unlock(led_matrix_mutex);
			return;
		}
		
		putString(string,&ledLineStack[led_stack_size]);
		
		x = ledLineStack[led_stack_size].x;

		lifetime *= (x+11);
		if(x > 64)
			lifetime -= 64;
		else
			shift = 0;
		led_line_stack_time[led_stack_size] = lifetime;
		led_line_stack_shift[led_stack_size] = shift;
		led_stack_size++;
	}
	else
	{
	}
	g_mutex_unlock(led_matrix_mutex);
}

static void ledPopFromStack(void)
{
	freeLedLine(&ledLineStack[led_stack_size-1]);
	g_mutex_lock(led_matrix_mutex);
	led_stack_size--;
	g_mutex_unlock(led_matrix_mutex);

}

gpointer *ledMatrixThread(gpointer user_data)
{
	if(running) // something went terribly wrong
		return;

	enum _screenToDraw
	{
		SCREEN_VOID,
		SCREEN_TIME,
		SCREEN_MPD,
		SCREEN_TEMPERATURES
	}screen_to_draw;

	struct _ledLine ledLineTime;
	struct _ledLine ledLineVoid;
	struct _ledLine *ledLineToDraw;
	int shift_speed = 0;
	time_t rawtime;
	struct tm *ptm;
	int last_mpd_state = 0;

	char time_string[20];

	running = 1;
	toggle = 0;
	screen_to_draw = SCREEN_TIME;
	
	allocateLedLine(&ledLineTime, LINE_LENGTH);
	allocateLedLine(&ledLineVoid, LINE_LENGTH);
	clearScreen(&ledLineVoid);

	if(initNetwork() < 0)
	{
		g_debug("ledmatrix.c: could not init network");
		running = 0;
	}

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
			else
				ledDisplayMain(ledLineToDraw, shift_speed);

		}
		/* Important! else doesn't work here */
		if(!led_stack_size)
		{
			if(toggle)
			{
				switch(screen_to_draw)
				{
					case SCREEN_MPD: screen_to_draw = SCREEN_TIME;
										break;
					case SCREEN_TIME: screen_to_draw = SCREEN_TEMPERATURES; 
										break;
					case SCREEN_TEMPERATURES: screen_to_draw = SCREEN_VOID;
										break;
//					case SCREEN_VOID: if(mpdGetState() == MPD_PLAYER_PLAY) screen_to_draw = SCREEN_MPD;
//										else screen_to_draw = SCREEN_TIME;
										break;
				}
				toggle = 0;
			}
//			else if(mpdGetState() == MPD_PLAYER_PLAY && last_mpd_state == 0)
//			{
//				last_mpd_state = 1;
//				screen_to_draw = SCREEN_MPD;
//			}
//			else if(mpdGetState() != MPD_PLAYER_PLAY && last_mpd_state == 1)
//			{
//				last_mpd_state = 0;
//				screen_to_draw = SCREEN_TIME;
//			}
//			else if(screen_to_draw == SCREEN_MPD)
//			{
//				ledLineToDraw = &ledLineMpd;
//				if(ledLineMpd.x >= 63)
//					shift_speed = 2;
//				else
//					shift_speed = 0;
//			}
			else if(screen_to_draw == SCREEN_TIME)
			{
				time(&rawtime);
				ptm = localtime(&rawtime);
				sprintf(time_string,"\r%02d:%02d:%02d",ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
				clearScreen(&ledLineTime);
				ledLineTime.x = (64-stringWidth(time_string))/2;
				ledLineTime.y = 2;
				putString(time_string,&ledLineTime);
				ledLineToDraw = &ledLineTime;
				shift_speed = 0;
			}
			else if(screen_to_draw == SCREEN_TEMPERATURES)
			{
				font = Comic_10;
//				sprintf(time_string,"\r%2d.%02d\bC \r%2d.%02d\bC",
//					lastTemperature[3][1][0],
//					lastTemperature[3][1][1]/100,
//					lastTemperature[3][0][0],
//					lastTemperature[3][0][1]/100
//					);
				clearScreen(&ledLineTime);
				ledLineTime.x = (64-stringWidth(time_string))/2;
				ledLineTime.y = 0;
				putString(time_string,&ledLineTime);
				
				font = Comic_8;
				ledLineTime.x = (64-stringWidth(time_string))/2;
				ledLineTime.y = 8;
				putString("\a Out   Wohn",&ledLineTime);
				ledLineToDraw = &ledLineTime;
				shift_speed = 0;
				font = Arial_Bold_14;
			}
			else if(screen_to_draw == SCREEN_VOID)
			{
				ledLineToDraw = &ledLineVoid;
				shift_speed = 0;
			}
			ledDisplayMain(ledLineToDraw, shift_speed);
		}
		
	}

	freeLedLine(&ledLineTime);
	freeLedLine(&ledLineVoid);
	
	close(client_sock);
}

static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed)
{
	static int counter = 0;
	if(shift_speed)
	{
		shiftLeft(ledLineToDraw);
		updateDisplay(*ledLineToDraw);
//		usleep(config.led_shift_speed);
	}
	else
	{
		updateDisplay(*ledLineToDraw);
		usleep(1000);
	}
	if(led_matrix_exit)
	{
		g_mutex_lock(led_matrix_mutex);
		led_matrix_exit = FALSE;
		g_mutex_unlock(led_matrix_mutex);
		return;
	}
}

void ledDisplayToggle(void)
{
	toggle = 1;
}


