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

#include "led_routines.h"
#include "fonts/arial_bold_14.h"
#include "fonts/Comic_8.h"
#include "fonts/Comic_10.h"
#include "had.h"
#include "mpd.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "led_routines"

static uint16_t charGetStart(char c);
static int initNetwork(void);
static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed);
static int putChar(char c, uint8_t color, struct _ledLine *ledLine);
static int charWidth(char c);
static int stringWidth(char *string);
static int shiftLeft(struct _ledLine *ledLine);
static int initNetwork(void);
static void ledRemoveFromFifo(void);
static gpointer ledMatrixStartThread(gpointer data);

static enum _screenToDraw current_screen;
static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed);

static uint8_t *font = Arial_Bold_14;

/* Diese Arrays werden nur zur Uebertragung ans Modul genutzt */
static uint16_t RED[4][16];
static uint16_t GREEN[4][16];

static struct _ledLine ledLineFifo[LED_FIFO_SIZE];

int led_line_fifo_time[LED_FIFO_SIZE];
int led_line_fifo_shift[LED_FIFO_SIZE];

static int client_sock;

int led_fifo_top = 0;
int led_fifo_bottom = 0;

static GMutex *mutex_is_running = NULL;
static GMutex *current_screen_mutex = NULL;
GThread *ledMatrixThread;
static int shutting_down;
static int running;

GAsyncQueue *async_queue_shutdown;
GAsyncQueue *async_queue_toggle_screen;
GAsyncQueue *async_queue_select_screen;
GAsyncQueue *async_queue_set_mpd_text;
GAsyncQueue *async_queue_insert_fifo;
GAsyncQueue *async_queue_set_static_text;

static int ledIsRunning(void)
{
    int is_running;
    g_mutex_lock(mutex_is_running);
    is_running = running;
    g_mutex_unlock(mutex_is_running);
    return is_running;
}

void endian_swap(uint16_t *x)
{
    *x = (*x>>8) | 
                (*x<<8);
}


static void updateDisplay(struct _ledLine ledLine)
{
    int bytes_send;
    int i,p,m;

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
#endif
#endif

            }
        }
    }
    bytes_send = send(client_sock, &RED, sizeof(RED),0);
    bytes_send = send(client_sock, &GREEN, sizeof(GREEN),0);
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

static int charWidth(char c)
{
    if(c == '\a' || c == '\b' || c == '\n' || c == '\r')
        return 0;
    else if(c == 32)
        return 4;
    else
        return font[6+c-font[4]] + 1;
}


static int stringWidth(char *string)
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


static int putChar(char c, uint8_t color, struct _ledLine *ledLine)
{

    uint8_t first_char = font[4];
    uint8_t char_count = font[5];
    uint8_t char_width;
    uint8_t char_height = font[3];

    uint16_t start;

    uint8_t i;

    if(c == '\n')
    {
        ledLine->x = 0;
        ledLine->y = 8;
        return 1;
    }

    /* if char is not in our font just leave */
    if(c < first_char || c > (first_char + char_count))
    {
        return 1;
    }
    
    /* Leerzeichen abfangen */
    if(c == 32)
        char_width = 4;
    else
    {
        char_width = font[6+c-first_char];
        start = charGetStart(c);
    }
    
    if((ledLine->x + char_width) >= LINE_LENGTH-50)
    {
        return 0;
    }

    if(c == ' ')
    {
        ledLine->x += 4;
        return 1;
    }

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
    return 1;
}

static void putString(char *string, struct _ledLine *ledLine)
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
        else if(!putChar(*string,color,ledLine))
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
}


static int shiftLeft(struct _ledLine *ledLine)
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
        return 0;
    }
    else
    {
        return 1;
    }
}

static int initNetwork(void)
{
    struct sockaddr_in server;

    client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client_sock < 0) {
        g_warning("Keine Verbindung zum LED-Modul");
        return -1;
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(config.led_matrix_port);
#ifdef _WIN32
        unsigned long addr;
        addr = inet_addr(config.led_matrix_ip);
        memcpy( (char *)&server.sin_addr, &addr, sizeof(addr));
#else
    inet_aton(config.led_matrix_ip, &server.sin_addr);
#endif
    

    if(connect(client_sock, (struct sockaddr*)&server, sizeof(server)) != 0)
    {
        g_warning("Konnte nicht zum LED-Modul verbinden");
        return -1;
    }   

    return 0;
}

static int allocateLedLine(struct _ledLine *ledLine, int line_length)
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

static void freeLedLine(struct _ledLine *ledLine)
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

static void ledInternalInsertFifo(char *string, int shift, int lifetime)
{
    if(!ledIsRunning())
        return;

    int x;
    
    if(!allocateLedLine(&ledLineFifo[led_fifo_top], LINE_LENGTH))
    {
        g_warning("Could not allocate memory!!");
        return;
    }
    
    g_debug("String pushed to stack: %s",string);
    putString(string,&ledLineFifo[led_fifo_top]);
    
    x = ledLineFifo[led_fifo_top].x;

    lifetime *= (x+11);
    if(x > 64)
        lifetime -= 64;
    else
        shift = 0;
    led_line_fifo_time[led_fifo_top] = lifetime;
    led_line_fifo_shift[led_fifo_top] = shift;

    if(++led_fifo_top > (LED_FIFO_SIZE-1)) led_fifo_top = 0;
    g_debug("led_fifo_top: %d led_fifo_bottom: %d",led_fifo_top, led_fifo_bottom);
}

static void ledRemoveFromFifo(void)
{
    g_debug("String removed from fifo");
    freeLedLine(&ledLineFifo[led_fifo_bottom]);
    if(++led_fifo_bottom > (LED_FIFO_SIZE- 1)) led_fifo_bottom= 0;
    g_debug("led_fifo_top: %d led_fifo_bottom: %d",led_fifo_top, led_fifo_bottom);
}

static gpointer ledMatrixStartThread(gpointer data)
{
    g_debug("LedMatrixThread gestartet");

    struct _ledLine ledLineTime;
    struct _ledLine ledLineVoid;
    struct _ledLine ledLineMpd;
    struct _ledLine *ledLineToDraw;
    struct _ledLine ledLineStatic;
    int shift_speed = 0;
    time_t rawtime;
    struct tm *ptm;
    int last_mpd_state = 0;
    char time_string[20];
    enum _screenToDraw screen_to_draw, *screen_switch;
    char *text_to_set;
    int shift;
    int lifetime;

    screen_to_draw = SCREEN_TIME;
    
    if(initNetwork() < 0)
    {
        g_debug("Stopping led_matrix thread");
        running = 0;
        g_mutex_unlock(mutex_is_running);
        return NULL;
    }
    
    allocateLedLine(&ledLineTime, LINE_LENGTH);
    allocateLedLine(&ledLineVoid, LINE_LENGTH); 
    allocateLedLine(&ledLineMpd, LINE_LENGTH);
    
    g_async_queue_ref(async_queue_shutdown);
    g_async_queue_ref(async_queue_toggle_screen);
    g_async_queue_ref(async_queue_select_screen);
    g_async_queue_ref(async_queue_set_mpd_text);
    g_async_queue_ref(async_queue_insert_fifo);
    g_async_queue_ref(async_queue_set_static_text);

    ledLineToDraw = &ledLineTime;
    
    g_mutex_unlock(mutex_is_running);
    while(1)
    {
        if((text_to_set = (char*)g_async_queue_try_pop(
            async_queue_set_mpd_text)))
        {
            clearScreen(&ledLineMpd); 
            putString(text_to_set, &ledLineMpd);
            free(text_to_set);
        }
        
        if((text_to_set = (char*)g_async_queue_try_pop(
            async_queue_set_static_text)))
        {
            clearScreen(&ledLineStatic); 
            putString(text_to_set, &ledLineStatic);
            free(text_to_set);
        }

        // check for incoming fifo data
        if((text_to_set = (char*)g_async_queue_try_pop(
            async_queue_insert_fifo)))
        {
            shift = *(int*)g_async_queue_pop(async_queue_insert_fifo);
            lifetime = *(int*)g_async_queue_pop(async_queue_insert_fifo);
            ledInternalInsertFifo(text_to_set, shift, lifetime);
            free(text_to_set);
        }

        if(led_fifo_bottom != led_fifo_top)
        {
            ledLineToDraw = &ledLineFifo[led_fifo_bottom];
            led_line_fifo_time[led_fifo_bottom]--;
            shift_speed = led_line_fifo_shift[led_fifo_bottom];
            if(!led_line_fifo_time[led_fifo_bottom])
            {
                ledRemoveFromFifo();
            }
            else
                ledDisplayMain(ledLineToDraw, shift_speed);

        }
        /* Important! else doesn't work here */
        if(led_fifo_bottom == led_fifo_top)
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
               //                         break;
                    case SCREEN_STATIC_TEXT: screen_to_draw = SCREEN_VOID;
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
                if(ledLineMpd.x >= 63)
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
                ledLineTime.x = (64-stringWidth(time_string))/2;
                ledLineTime.y = 2;
                putString(time_string,&ledLineTime);
                ledLineToDraw = &ledLineTime;
                shift_speed = 0;
            }
            else if(screen_to_draw == SCREEN_TEMPERATURES)
            {
                font = Comic_10;
                sprintf(time_string,"\r%2d.%02d\bC \r%2d.%02d\bC",
                    lastTemperature[3][1][0],
                    lastTemperature[3][1][1]/100,
                    lastTemperature[3][0][0],
                    lastTemperature[3][0][1]/100
                    );
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
            else if(screen_to_draw == SCREEN_STATIC_TEXT)
            {
                ledLineToDraw = &ledLineStatic;
            }
            g_mutex_lock(current_screen_mutex);
            current_screen = screen_to_draw;
            g_mutex_unlock(current_screen_mutex);
            ledDisplayMain(ledLineToDraw, shift_speed);
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
    g_async_queue_unref(async_queue_insert_fifo);
    g_async_queue_unref(async_queue_set_static_text);

    freeLedLine(&ledLineTime);
    freeLedLine(&ledLineVoid);
    freeLedLine(&ledLineMpd);
    g_debug("close status = %d",close(client_sock));
    g_mutex_lock(mutex_is_running);
    running = 0;
    g_mutex_unlock(mutex_is_running);
    return NULL;
}

static void ledDisplayMain(struct _ledLine *ledLineToDraw, int shift_speed)
{
    if(shift_speed)
    {
        shiftLeft(ledLineToDraw);
        updateDisplay(*ledLineToDraw);
        g_usleep(config.led_shift_speed);
    }
    else
    {
        updateDisplay(*ledLineToDraw);
        g_usleep(1000);
    }
}

void ledMatrixInit(void)
{
    async_queue_shutdown = g_async_queue_new();
    async_queue_toggle_screen = g_async_queue_new();
    async_queue_select_screen = g_async_queue_new();
    async_queue_set_mpd_text = g_async_queue_new();
    async_queue_insert_fifo = g_async_queue_new();
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

void ledMatrixSetMpdText(char *text)
{
    char *stext = strdup(text);
    if(ledIsRunning())
    {
        g_async_queue_push(async_queue_set_mpd_text, (gpointer)stext);
    }
}

void ledMatrixSetStaticText(char *text)
{
    char *stext = strdup(text);
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
    g_usleep(1000000);
    if(config.led_matrix_activated && !ledIsRunning())
    {
        g_mutex_lock(mutex_is_running);
        running = 1;
        ledMatrixThread = g_thread_create(ledMatrixStartThread, NULL, TRUE, NULL);
    }
}

void ledInsertFifo(char *string, int shift, int lifetime)
{
    static int sshift, slifetime;
    sshift = shift;
    slifetime = lifetime;
    char *sstring = strdup(string);
    if(ledIsRunning())
    {
        g_async_queue_push(async_queue_insert_fifo, (gpointer)sstring);
        g_async_queue_push(async_queue_insert_fifo, (gpointer)&sshift);
        g_async_queue_push(async_queue_insert_fifo, (gpointer)&slifetime);
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

