/*
 * Copyright (C) 2008-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
* \file led_routines.c
* \brief    functions for the ledmatrix display
* \author   Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>

#include "led_routines.h"
#include "led_mpd_fifo.h"
#include "led_text_fifo.h"

#include "fonts/8x8.h"
#include "fonts/8x12.h"
#include "had.h"
#include "mpd.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "led_routines"

#define SHIFT_ALL 0
#define SHIFT_UPPER 1
#define SHIFT_LOWER 2

static int initNetwork(void);
static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed, int shift_what);
static int putChar(char c, const char (*font)[24], uint8_t color, struct _ledLine *ledLine);
static int stringWidth(char *string, const char (*font)[24]);
static int shiftLeft(struct _ledLine *ledLine, int section);
static gpointer ledMatrixStartThread(gpointer data);

static enum _screenToDraw current_screen;

/* Diese Arrays werden nur zur Uebertragung ans Modul genutzt */
static uint16_t RED[4][16];
static uint16_t GREEN[4][16];

static int client_sock;

static GMutex *mutex_is_running = NULL;
static GMutex *current_screen_mutex = NULL;
GThread *ledMatrixThread;
static int shutting_down;
static int running;

GAsyncQueue *async_queue_shutdown;
GAsyncQueue *async_queue_toggle_screen;
GAsyncQueue *async_queue_select_screen;
GAsyncQueue *async_queue_set_mpd_text;
GAsyncQueue *async_queue_push_fifo;
GAsyncQueue *async_queue_set_static_text;


int ledIsRunning(void)
{
    int is_running;
    g_mutex_lock(mutex_is_running);
    is_running = running;
    g_mutex_unlock(mutex_is_running);
    return is_running;
}


static void updateDisplay(struct _ledLine ledLine)
{
    int i,p,m;
    int status;
    struct sockaddr_in server;
    struct addrinfo hints;
    struct addrinfo *res, *addr_iterator;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(config.led_matrix_ip, NULL, &hints, &res);

    if(status)
        return;
    
    if(res)
    {
        if(res->ai_family == AF_INET) // ipv4
        {
            memcpy(&server, res->ai_addr,sizeof(server));
        }
        freeaddrinfo(res); // free the linked list
    }

    memset(&RED,0,sizeof(RED));
    memset(&GREEN,0,sizeof(GREEN));
    
    for(m=0;m<4;m++) // for every module
    {
        for(i=0;i<16;i++) // for every row
        {
            if(i==0)
            {
                RED[m][i] = 0;
                GREEN[m][i] = 0;
            }
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
#ifdef MIPSEB
#warning building for BIG ENDIAN // I'M THE DIRTIEST HACKER EVER BORN
                /* was there a shift yet? if no, print the unshifted arrays */
                if(p<8)
                {
                    if(ledLine.shift_position)
                    {
                        RED[m][i] |= ((ledLine.column_red_output[p+8+m*16] & (1<<i))>>(i)<<p);
                        GREEN[m][i] |= ((ledLine.column_green_output[p+8+m*16] & (1<<i))>>(i)<<p);
                    }
                    else
                    {
                        RED[m][i] |= ((ledLine.column_red[p+8+m*16] & (1<<i))>>(i)<<p);
                        GREEN[m][i] |= ((ledLine.column_green[p+8+m*16] & (1<<i))>>(i)<<p);
                    }
                }
                else
                {
                    if(ledLine.shift_position)
                    {
                        RED[m][i] |= ((ledLine.column_red_output[p-8+m*16] & (1<<i))>>(i)<<p);
                        GREEN[m][i] |= ((ledLine.column_green_output[p-8+m*16] & (1<<i))>>(i)<<p);
                    }
                    else
                    {
                        RED[m][i] |= ((ledLine.column_red[p-8+m*16] & (1<<i))>>(i)<<p);
                        GREEN[m][i] |= ((ledLine.column_green[p-8+m*16] & (1<<i))>>(i)<<p);
                    }
                }
#else
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
#endif

            }
        }
    }

//    server.sin_family = AF_INET;
    server.sin_port = htons(config.led_matrix_port);
//#ifdef _WIN32
//        unsigned long addr;
//        addr = inet_addr(config.led_matrix_ip);
//        memcpy( (char *)&server.sin_addr, &addr, sizeof(addr));
//#else
//    inet_aton(config.led_matrix_ip, &server.sin_addr);
//#endif
    sendto(client_sock, &RED, sizeof(RED),0,(struct sockaddr*)&server,sizeof(server));
    sendto(client_sock, &GREEN, sizeof(GREEN),0,(struct sockaddr*)&server,sizeof(server));
    
    char buf[10];
    socklen_t length = sizeof(server);
    recvfrom(client_sock, &buf, 1,0,(struct sockaddr*)&server,&length);
}

static int charWidth(char c, const char (*font)[24])
{
    int i, count;

    count = 0;

    if(c == ' ')
        return 3;

    if(c == '\a' || c == '\b' || c == '\r')
        return 0;

    if(font[256][1] > 8)
    {
        for(i=0;i<font[256][0];i++)
        {
            if(font[(int)c][i*2] != 0 && font[(int)c][i*2+1] != 0)
               count++;
        }
    }
    else
    {
        for(i=0;i<font[256][0];i++)
        {
            if(font[(int)c][i] != 0)
                count++;
        }
    }
    return count+1;
}


static int stringWidth(char *string, const char (*font)[24])
{
    int width = 0;
    if(!string)
        string = "null";
    while(*string)
    {
        width += charWidth(*string++,font);
    }
    return width - 1;
}


static int putChar(char c, const char (*font)[24], uint8_t color, struct _ledLine *ledLine)
{
    int char_width = font[256][0];
    int char_height = font[256][1];

    int i;

    if(c == '\n')
    {
        ledLine->x = 0;
        ledLine->y = 8;
        return 1;
    }
    else if(c == ' ')
    {
        ledLine->x += 3;
        return 1;
    }
    
    if((ledLine->x + char_width) >= LINE_LENGTH-50)
    {
        return 0;
    }
    
    if(char_height > 8)
    {
        for(i=0;i<char_width;i++)
        {
            if(color == COLOR_RED)
            {
                if(ledLine->y > 0)
                    ledLine->column_red[i+ledLine->x] |= font[(int)c][i*2]<<ledLine->y & (0xFF<<ledLine->y);
                else
                    ledLine->column_red[i+ledLine->x] |= font[(int)c][i*2]>>-ledLine->y & (0xFF>>-ledLine->y);
                ledLine->column_red[i+ledLine->x] |= font[(int)c][i*2+1]<<(ledLine->y+8) & (0xFF<<(ledLine->y+8));
            }
            else if(color == COLOR_GREEN)
            {
                if(ledLine->y > 0)
                    ledLine->column_green[i+ledLine->x] |= font[(int)c][i*2]<<ledLine->y & (0xFF<<ledLine->y);
                else
                    ledLine->column_green[i+ledLine->x] |= font[(int)c][i*2]>>-ledLine->y & (0xFF>>-ledLine->y);
                ledLine->column_green[i+ledLine->x] |= font[(int)c][i*2+1]<<(ledLine->y+8) & (0xFF<<(ledLine->y+8));
            }
            else if(color == COLOR_AMBER)
            {
                if(ledLine->y > 0)
                    ledLine->column_red[i+ledLine->x] |= font[(int)c][i*2]<<ledLine->y & (0xFF<<ledLine->y);
                else
                    ledLine->column_red[i+ledLine->x] |= font[(int)c][i*2]>>-ledLine->y & (0xFF>>-ledLine->y);
                ledLine->column_red[i+ledLine->x] |= font[(int)c][i*2+1]<<(ledLine->y+8) & (0xFF<<(ledLine->y+8));
                if(ledLine->y > 0)
                    ledLine->column_green[i+ledLine->x] |= font[(int)c][i*2]<<ledLine->y & (0xFF<<ledLine->y);
                else
                    ledLine->column_green[i+ledLine->x] |= font[(int)c][i*2]>>-ledLine->y & (0xFF>>-ledLine->y);
                ledLine->column_green[i+ledLine->x] |= font[(int)c][i*2+1]<<(ledLine->y+8) & (0xFF<<(ledLine->y+8));
            }
            if(font[(int)c][i*2] == 0 && font[(int)c][i*2+1] == 0)
                ledLine->x -=1;
        }
    }
    else
    {
        for(i=0;i<char_width;i++)
        {
            if(color == COLOR_RED)
            {
                ledLine->column_red[i+ledLine->x] |= font[(int)c][i]<<ledLine->y & (0xFF<<ledLine->y);
            }
            else if(color == COLOR_GREEN)
            {
                ledLine->column_green[i+ledLine->x] |= font[(int)c][i]<<ledLine->y & (0xFF<<ledLine->y);
            }
            else if(color == COLOR_AMBER)
            {
                ledLine->column_red[i+ledLine->x] |= font[(int)c][i]<<ledLine->y & (0xFF<<ledLine->y);
                ledLine->column_green[i+ledLine->x] |= font[(int)c][i]<<ledLine->y & (0xFF<<ledLine->y);
            }
            if(font[(int)c][i] == 0)
                ledLine->x -=1;
        }
    }

    ledLine->x += char_width+1;
    return 1;
}

void putString(char *string, const char (*font)[24],struct _ledLine *ledLine)
{
    static int color = COLOR_RED;

    if(!string)
        string = "null";
    while(*string)
    {
        if(*string == '\b')
            color = COLOR_GREEN;
        else if(*string == '\r')
            color = COLOR_RED;
        else if(*string == '\a')
            color = COLOR_AMBER;
        else if(!putChar(*string,font,color,ledLine))
        {
            return;
        }
        string++;
    }
}

static void clearScreen(struct _ledLine *ledLine)
{
    memset(ledLine->column_red,0,sizeof(uint16_t)*LINE_LENGTH);
    memset(ledLine->column_green,0,sizeof(uint16_t)*LINE_LENGTH);
    
    memset(ledLine->column_red_output,0,sizeof(uint16_t)*LINE_LENGTH);
    memset(ledLine->column_green_output,0,sizeof(uint16_t)*LINE_LENGTH);

    ledLine->x = 0;
    ledLine->y = 1;
    ledLine->shift_position = 0;
}


static int shiftLeft(struct _ledLine *ledLine, int section)
{
    int counter;

    int scroll_length;
    
    if(ledLine->x + 11 > LINE_LENGTH)
        scroll_length = LINE_LENGTH;
    else
        scroll_length = ledLine->x + 11;

    for(counter=0;counter< scroll_length - 1  ;counter++)
    {
        if(ledLine->shift_position + counter > scroll_length - 1)
        {
            if(section == 0)
            {
                ledLine->column_red_output[counter] = ledLine->column_red[counter + ledLine->shift_position - (scroll_length)];
                ledLine->column_green_output[counter] = ledLine->column_green[counter + ledLine->shift_position - (scroll_length)];
            }
            else if(section == 1) // upper 8 lines
            {
                ledLine->column_red_output[counter] = ledLine->column_red[counter + ledLine->shift_position - (scroll_length)] & 0xFF;
                ledLine->column_red_output[counter] |= ledLine->column_red[counter] & 0xFF00;
                ledLine->column_green_output[counter] = ledLine->column_green[counter + ledLine->shift_position - (scroll_length)] & 0xFF;
                ledLine->column_green_output[counter] |= ledLine->column_green[counter] & 0xFF00;
            }
            else if(section == 2) // lower 8 lines
            {
                ledLine->column_red_output[counter] = ledLine->column_red[counter + ledLine->shift_position - (scroll_length)] & 0xFF00;
                ledLine->column_red_output[counter] |= ledLine->column_red[counter] & 0xFF;
                ledLine->column_green_output[counter] = ledLine->column_green[counter + ledLine->shift_position - (scroll_length)] & 0xFF00;
                ledLine->column_green_output[counter] |= ledLine->column_green[counter] & 0xFF;
            }
        }
        else
        {
            if(section == 0)
            {
                ledLine->column_red_output[counter] = ledLine->column_red[ledLine->shift_position+counter];
                ledLine->column_green_output[counter] = ledLine->column_green[ledLine->shift_position+counter];
            }
            else if(section == 1)
            {
                ledLine->column_red_output[counter] = ledLine->column_red[ledLine->shift_position+counter] & 0xFF;
                ledLine->column_red_output[counter] |= ledLine->column_red[counter] & 0xFF00;
                ledLine->column_green_output[counter] = ledLine->column_green[ledLine->shift_position+counter] & 0xFF;
                ledLine->column_green_output[counter] |= ledLine->column_green[counter] & 0xFF00;
            }
            else if(section == 2)
            {
                ledLine->column_red_output[counter] = ledLine->column_red[ledLine->shift_position+counter] & 0xFF00;
                ledLine->column_red_output[counter] |= ledLine->column_red[counter] & 0xFF;
                ledLine->column_green_output[counter] = ledLine->column_green[ledLine->shift_position+counter] & 0xFF00;
                ledLine->column_green_output[counter] |= ledLine->column_green[counter] & 0xFF;
            }
        }
    }

    ledLine->shift_position++;
    
    if(ledLine->shift_position > ledLine->x + 11)
    {
        ledLine->shift_position = 1;
        return 0;
    }
    else
    {
        return 1;
    }
}

static int initNetwork(void)
{
    struct timeval t;
    t.tv_sec = 2;

    client_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(client_sock < 0) {
        g_warning("Keine Verbindung zum LED-Modul");
        return -1;
    }
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
    
    return 0;
}

int allocateLedLine(struct _ledLine *ledLine, int line_length)
{
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
    

    clearScreen(ledLine);

    ledLine->x = 0;
    ledLine->y = 1;
    ledLine->shift_position = 0;
    return 1;
}

void freeLedLine(struct _ledLine *ledLine)
{
    if(!ledLine->column_red)
        g_warning("ledLine.column_red was not allocated");
    if(!ledLine->column_green)
        g_warning("ledLine.column_green was not allocated");
    if(!ledLine->column_red_output)
        g_warning("ledLine.column_red_output was not allocated");
    if(!ledLine->column_green_output)
        g_warning("ledLine.column_green_output was not allocated");
    free(ledLine->column_red);
    free(ledLine->column_green);
    
    free(ledLine->column_red_output);
    free(ledLine->column_green_output);
}

static gpointer ledMatrixStartThread(gpointer data)
{
    g_debug("LedMatrixThread gestartet");

    struct _ledLine ledLineTime;
    struct _ledLine ledLineVoid;
    struct _ledLine ledLineMpd;
    struct _ledLine *ledLineToDraw;
    struct _ledLine ledLineStatic;
    struct _ledLine ledLineScope;
    int shift_speed = 0;
    int shift_what = SHIFT_ALL;
    time_t rawtime;
    struct tm *ptm;
    int last_mpd_state = 0;
    char time_string[20];
    enum _screenToDraw screen_to_draw, *screen_switch;
    char *text_to_set;
    int shift;
    int lifetime;
    struct _artist_title *artist_title;

    screen_to_draw = SCREEN_TIME;
    ledLineToDraw = &ledLineTime;
    
    if(initNetwork() < 0)
    {
        g_debug("Stopping led_matrix thread");
        running = 0;
        g_mutex_unlock(mutex_is_running);
        return NULL;
    }
    
    allocateLedLine(&ledLineStatic, LINE_LENGTH);
    allocateLedLine(&ledLineMpd, LINE_LENGTH);
    allocateLedLine(&ledLineTime, LINE_LENGTH);
    allocateLedLine(&ledLineVoid, LINE_LENGTH);
    allocateLedLine(&ledLineScope, LINE_LENGTH);
    
    g_async_queue_ref(async_queue_shutdown);
    g_async_queue_ref(async_queue_toggle_screen);
    g_async_queue_ref(async_queue_select_screen);
    g_async_queue_ref(async_queue_set_mpd_text);
    g_async_queue_ref(async_queue_push_fifo);
    g_async_queue_ref(async_queue_set_static_text);

    g_mutex_unlock(mutex_is_running);
    while(1)
    {
        if((artist_title = (struct _artist_title*)g_async_queue_try_pop(
            async_queue_set_mpd_text)))
        {
            clearScreen(&ledLineMpd);
            clearScreen(&ledLineScope);
            if(stringWidth(artist_title->artist,font8x8) > 64 ||
                stringWidth(artist_title->title,font8x8) > 64)
            {
                ledLineMpd.y = 1;
                putString("\r",font8x8,&ledLineMpd);
                putString(artist_title->artist,font8x8, &ledLineMpd);
                putString(" \a- \b",font8x8,&ledLineMpd);
                putString(artist_title->title,font8x8, &ledLineMpd);
            }
            else
            {
                ledLineMpd.x = (64-stringWidth(artist_title->artist,font8x8))/2;
                putString("\r",font8x8,&ledLineMpd);
                putString(artist_title->artist,font8x8, &ledLineMpd);
                putString("\n\b",font8x8,&ledLineMpd);
                ledLineMpd.x = (64-stringWidth(artist_title->title,font8x8))/2;
                putString(artist_title->title,font8x8, &ledLineMpd);
            }
            ledLineScope.y = 1;
            putString("\r",font8x8,&ledLineScope);
            putString(artist_title->artist,font8x8, &ledLineScope);
            putString(" \a- \b",font8x8,&ledLineScope);
            putString(artist_title->title,font8x8, &ledLineScope);
            free(artist_title);
        }
        
        if((text_to_set = (char*)g_async_queue_try_pop(
            async_queue_set_static_text)))
        {
            clearScreen(&ledLineStatic); 
            putString(text_to_set, font8x8,&ledLineStatic);
            free(text_to_set);
        }

        // check for incoming fifo data
        if((text_to_set = (char*)g_async_queue_try_pop(
            async_queue_push_fifo)))
        {
            shift = *(int*)g_async_queue_pop(async_queue_push_fifo);
            lifetime = *(int*)g_async_queue_pop(async_queue_push_fifo);
            ledInternalInsertFifo(text_to_set, shift, lifetime);
            free(text_to_set);
        }

        if(!ledFifoEmpty())
        {
            ledLineToDraw = ledFifoGetLine();
            shift_speed = ledFifoGetShiftSpeed();
            if(!ledFifoEmpty())
                ledDisplayMain(ledLineToDraw, shift_speed, shift_what);

        }
        /* Important! else doesn't work here */
        if(ledFifoEmpty())
        {
            if(g_async_queue_try_pop(async_queue_toggle_screen) != NULL)
            {
                switch(screen_to_draw)
                {
                    case SCREEN_MPD: screen_to_draw = SCREEN_TIME;
                                        break;
                    case SCREEN_TIME: screen_to_draw = SCREEN_TEMPERATURES; 
                                        break;
                    case SCREEN_TEMPERATURES: screen_to_draw = SCREEN_STATIC_TEXT;
                    case SCREEN_STATIC_TEXT: screen_to_draw = SCREEN_SCOPE;
                                        break;
                    case SCREEN_SCOPE: screen_to_draw = SCREEN_VOID;
                                        break;
                    case SCREEN_VOID: if(mpdGetState() == MPD_PLAYER_PLAY) screen_to_draw = SCREEN_MPD;
                                        else screen_to_draw = SCREEN_TIME;
                                        break;
                }
            }
            else if((screen_switch = (enum _screenToDraw*)g_async_queue_try_pop(
                async_queue_select_screen)) != NULL)
            {
                g_debug("switching screen");
                g_debug("switch value = %d", *screen_switch);
                switch(*screen_switch)
                {
                    case SCREEN_TIME: screen_to_draw = SCREEN_TIME;
                                      g_debug("screen switched to SCREEN_TIME"); break;
                    case SCREEN_MPD:  screen_to_draw = SCREEN_MPD;
                                      g_debug("screen switched to SCREEN_MPD"); break;
                    case SCREEN_TEMPERATURES: screen_to_draw = SCREEN_TEMPERATURES;
                                      g_debug("screen switched to SCREEN_TEMPERATURES"); break;
                    case SCREEN_STATIC_TEXT: screen_to_draw = SCREEN_STATIC_TEXT;
                                      g_debug("screen switched to SCREEN_STATIC_TEXT"); break;
                    case SCREEN_SCOPE: screen_to_draw = SCREEN_SCOPE;
                                      g_debug("screen switched to SCREEN_SCOPE"); break;
                    case SCREEN_VOID: screen_to_draw = SCREEN_VOID;
                                      g_debug("screen switched to SCREEN_VOID"); break;
                    default: break;
                }
            }
            else if(mpdGetState() == MPD_PLAYER_PLAY && last_mpd_state == 0)
            {
                last_mpd_state = 1;
                screen_to_draw = SCREEN_MPD;
            }
            else if(mpdGetState() != MPD_PLAYER_PLAY && last_mpd_state == 1)
            {
                last_mpd_state = 0;
                screen_to_draw = SCREEN_TIME;
            }
            else if(screen_to_draw == SCREEN_MPD)
            {
                ledLineToDraw = &ledLineMpd;
                if(ledLineMpd.x > 64)
                    shift_speed = 2;
                else
                    shift_speed = 0;
            }
            else if(screen_to_draw == SCREEN_TIME)
            {
                time(&rawtime);
                ptm = localtime(&rawtime);
                sprintf(time_string,"\r%02d:%02d:%02d",ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
                clearScreen(&ledLineTime);
                //ledLineTime.x = (64-stringWidth(time_string,font8x12)-4)/2;
                ledLineTime.x = 6;
                ledLineTime.y = -2;
                putString(time_string,font8x12,&ledLineTime);
                ledLineToDraw = &ledLineTime;
                shift_speed = 0;
            }
            else if(screen_to_draw == SCREEN_TEMPERATURES)
            {
                sprintf(time_string,"\r%2d.%02d\bC \r%2d.%02d\bC",
                    lastTemperature[3][1]/10,
                    lastTemperature[3][1]%10,
                    lastTemperature[3][0]/10,
                    lastTemperature[3][0]%10
                    );
                clearScreen(&ledLineTime);
                ledLineTime.x = (64-stringWidth(time_string,font8x8))/2;
                ledLineTime.y = 0;
                putString(time_string,font8x8,&ledLineTime);
                
                //font = Comic_8;
                ledLineTime.x = (64-stringWidth(time_string,font8x8))/2;
                ledLineTime.y = 8;
                putString("\a Out   Wohn",font8x8,&ledLineTime);
                ledLineToDraw = &ledLineTime;
                shift_speed = 0;
                //font = Arial_Bold_14;
            }
            else if(screen_to_draw == SCREEN_VOID)
            {
                ledLineToDraw = &ledLineVoid;
                shift_speed = 0;
            }
            else if(screen_to_draw == SCREEN_STATIC_TEXT)
            {
                ledLineToDraw = &ledLineStatic;
            }
            else if(screen_to_draw == SCREEN_SCOPE)
            {
                mpdFifoInit();
                ledLineToDraw = &ledLineScope;
                mpdFifoUpdate();
                int i;
                for(i=0;i<64;i++)
                {
                    uint8_t value = (uint8_t)(col_magnitude[i]/430000*(i+1));
                   // if(value == 0) {
                   //     ledLineScope.column_red[i] = 0;
                   //     ledLineScope.column_green[i] = 0;
                   // } else if(value == 1){
                   //     ledLineScope.column_red[i] = 0;
                   //     ledLineScope.column_green[i] = 0x8000;
                   // } else if(value == 2){
                   //     ledLineScope.column_red[i] = 0;
                   //     ledLineScope.column_green[i] = 0xC000;
                   // } else if(value == 3){
                   //     ledLineScope.column_red[i] = 0;
                   //     ledLineScope.column_green[i] = 0xE000;
                   // } else if(value == 4){
                   //     ledLineScope.column_red[i] = 0;
                   //     ledLineScope.column_green[i] = 0xF000;
                   // } else if(value == 5){
                   //     ledLineScope.column_red[i] = 0;
                   //     ledLineScope.column_green[i] = 0xF800;
                   // } else if(value == 6){
                   //     ledLineScope.column_red[i] = 0;
                   //     ledLineScope.column_green[i] = 0xFC00;
                   // } else if(value == 7){
                   //     ledLineScope.column_red[i] = 0xFE00 - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFE00;
                   // } else if(value == 8){
                   //     ledLineScope.column_red[i] = 0xFF00 - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFF00;
                   // } else if(value == 9){
                   //     ledLineScope.column_red[i] = 0xFF80 - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFF80;
                   // } else if(value == 10){
                   //     ledLineScope.column_red[i] = 0xFFC0 - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFFC0;
                   // } else if(value == 11){
                   //     ledLineScope.column_red[i] = 0xFFE0 - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFFC0;
                   // } else if(value == 12){
                   //     ledLineScope.column_red[i] = 0xFFF0 - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFFC0;
                   // } else if(value == 13){
                   //     ledLineScope.column_red[i] = 0xFFF8 - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFFC0;
                   // } else if(value == 14){
                   //     ledLineScope.column_red[i] = 0xFFFC - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFFC0;
                   // } else if(value >= 15){
                   //     ledLineScope.column_red[i] = 0xFFFE - 0xFC00;
                   //     ledLineScope.column_green[i] = 0xFFC0;
                   // }
                    if(value == 0) {
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 1){
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 2){
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0x8000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 3){
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0x8000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 4){
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xC000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 5){
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xC000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 6){
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xE000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 7){
                        ledLineScope.column_red[i] = 0 | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xE000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 8){
                        ledLineScope.column_red[i] = (0xF000 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 9){
                        ledLineScope.column_red[i] = (0xF000 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF000 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 10){
                        ledLineScope.column_red[i] = (0xF800 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF800 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 11){
                        ledLineScope.column_red[i] = (0xF800 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF800 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 12){
                        ledLineScope.column_red[i] = (0xFC00 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF800 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 13){
                        ledLineScope.column_red[i] = (0xFC00 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF800 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value == 14){
                        ledLineScope.column_red[i] = (0xFE00 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF800 | (ledLineScope.column_green[i] & 0xFF);
                    } else if(value >= 15){
                        ledLineScope.column_red[i] = (0xFE00 - 0xE000) | (ledLineScope.column_red[i] & 0xFF);
                        ledLineScope.column_green[i] = 0xF800 | (ledLineScope.column_green[i] & 0xFF);
                    }
                }
                shift_what = SHIFT_UPPER;
                shift_speed = 2;
            }
            g_mutex_lock(current_screen_mutex);
            current_screen = screen_to_draw;
            g_mutex_unlock(current_screen_mutex);
            ledDisplayMain(ledLineToDraw, shift_speed, shift_what);
            if(screen_to_draw != SCREEN_SCOPE)
                g_usleep(1000);
        }
        if(g_async_queue_try_pop(async_queue_shutdown) != NULL)
        {
            break;
        }
    }
    
    g_async_queue_unref(async_queue_shutdown);
    g_async_queue_unref(async_queue_toggle_screen);
    g_async_queue_unref(async_queue_select_screen);
    g_async_queue_unref(async_queue_set_mpd_text);
    g_async_queue_unref(async_queue_push_fifo);
    g_async_queue_unref(async_queue_set_static_text);
    
    mpdFifoClose();
    freeLedLine(&ledLineTime);
    freeLedLine(&ledLineVoid);
    freeLedLine(&ledLineMpd);
    g_mutex_lock(mutex_is_running);
    running = 0;
    g_mutex_unlock(mutex_is_running);
    return NULL;
}

static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed, int shift_what)
{
    if(shift_speed)
    {
        shiftLeft(ledLineToDraw,shift_what);
        updateDisplay(*ledLineToDraw);
        g_usleep(config.led_shift_speed);
    }
    else
    {
        updateDisplay(*ledLineToDraw);
    }
}

void ledMatrixInit(void)
{
    async_queue_shutdown = g_async_queue_new();
    async_queue_toggle_screen = g_async_queue_new();
    async_queue_select_screen = g_async_queue_new();
    async_queue_set_mpd_text = g_async_queue_new();
    async_queue_push_fifo = g_async_queue_new();
    async_queue_set_static_text = g_async_queue_new();

    mutex_is_running = g_mutex_new();
    current_screen_mutex = g_mutex_new();
    running = 0;
    shutting_down = 0;
}


enum _screenToDraw ledMatrixCurrentScreen()
{
    enum _screenToDraw screen;
    g_mutex_lock(current_screen_mutex);
    screen = current_screen;
    g_mutex_unlock(current_screen_mutex);
    return screen;
}

void ledMatrixSetMpdText(struct _artist_title *artist_title)
{
    struct _artist_title *s_artist_title;

    s_artist_title = malloc(sizeof(struct _artist_title));
    memcpy(s_artist_title, artist_title,sizeof(struct _artist_title));
    if(ledIsRunning())
    {
        g_async_queue_push(async_queue_set_mpd_text, (gpointer)s_artist_title);
    }
}

void ledMatrixSetStaticText(char *text)
{
    char *stext = g_strdup(text);
    if(ledIsRunning())
    {
        g_async_queue_push(async_queue_set_static_text, (gpointer)stext);
    }
}

void ledMatrixToggle(void)
{
    static gboolean toggle = TRUE;
    if(ledIsRunning())
    {
        g_async_queue_push(async_queue_toggle_screen, &toggle);
    }
}

void ledMatrixSelectScreen(enum _screenToDraw screen)
{
    static enum _screenToDraw sscreen;
    sscreen = screen;
    if(ledIsRunning())
    {
        g_async_queue_push(async_queue_select_screen, (gpointer)&sscreen);
    }
}

void ledMatrixStart(void)
{
    if(config.led_matrix_activated && !ledIsRunning())
    {
        g_mutex_lock(mutex_is_running);
        running = 1;
        ledMatrixThread = g_thread_create(ledMatrixStartThread, NULL, TRUE, NULL);
    }
}

void ledFifoInsert(char *string, int shift, int lifetime)
{
    static int sshift, slifetime;
    sshift = shift;
    slifetime = lifetime;
    char *sstring = g_strdup(string);
    if(ledIsRunning())
    {
        g_async_queue_push(async_queue_push_fifo, (gpointer)sstring);
        g_async_queue_push(async_queue_push_fifo, (gpointer)&sshift);
        g_async_queue_push(async_queue_push_fifo, (gpointer)&slifetime);
    }
}

void ledMatrixStop()
{
    static gboolean shutdown = TRUE;

    if(!ledIsRunning()) // something went terribly wrong
    {
        g_debug("LedMatrixThread not running");
        return;
    }
    g_debug("LedMatrixThread stopping");
    g_async_queue_push(async_queue_shutdown, &shutdown);
    g_debug("LedMatrixThread joining");
    g_thread_join(ledMatrixThread);
    g_debug("LedMatrixThread gestoppt");
}

