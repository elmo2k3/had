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
#include <fcntl.h>
#include <math.h>

#include "base_station.h"
#include "led_routines.h"
#include "fonts/arial_bold_14.h"
#include "fonts/Comic_8.h"
#include "fonts/Comic_10.h"
#include "had.h"
#include "mpd.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "led_routines"

#ifndef _NO_FFTW3_
#include <fftw3.h>
static fftw_complex *fft_output;
static fftw_plan fft_plan;
#endif

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

#define SAMPLES 256
#define RESULTS (SAMPLES/2+1)
#define FREQ_PER_COL (RESULTS/64*4/5)
static int mpd_fifo_fd = 0;
static double *fft_input;
static unsigned int fft_magnitude[RESULTS];
static unsigned int col_magnitude_max;
static double col_magnitude[64];

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

static int mpdFifoInit(void)
{
    if(!config.mpd_fifo_activated)
        return 1;
    if(mpd_fifo_fd > 0)
        return 2;
    mpd_fifo_fd = open(config.mpd_fifo_file,O_RDONLY | O_NONBLOCK);
    if(mpd_fifo_fd < 0)
        return 1;
    
#ifndef _NO_FFTW3_
    fft_input = (double*)fftw_malloc(sizeof(double)*SAMPLES);
    fft_output = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*RESULTS);
    fft_plan = fftw_plan_dft_r2c_1d(SAMPLES, fft_input, fft_output, FFTW_ESTIMATE);
#endif
    return 0;
}

static void mpdFifoClose(void)
{
    if(mpd_fifo_fd <= 0)
        return;
    close(mpd_fifo_fd);
    mpd_fifo_fd = 0;
#ifndef _NO_FFTW3_
    fftw_free(fft_input);
    fftw_free(fft_output);
#endif
}

static void mpdFifoUpdate(void)
{
    static int16_t buf[SAMPLES*2];
    static int toggler = 0;
    ssize_t num;
    int i,p;

    if(mpd_fifo_fd <= 0)
        return;

    num = read(mpd_fifo_fd, buf, sizeof(buf));
    if(num < 0)
        return;

    for(i=num;i<SAMPLES;i++)
    {
        fft_input[i] = 0;
    }
    
    for(i=0;i<SAMPLES;i++)
    {
        fft_input[i] = buf[i];
    }
    
#ifndef _NO_FFTW3_
    fftw_execute(fft_plan);

    for(i=0;i<RESULTS;i++)
    {
        fft_magnitude[i] = sqrt(fft_output[i][0]*fft_output[i][0] + fft_output[i][1]*fft_output[i][1]);
    }
    col_magnitude_max = 0;
    for(i=0;i<64;i++)
    {
        col_magnitude[i] = 0;
        for(p=0;p<FREQ_PER_COL;p++)
        {
            col_magnitude[i] += fft_magnitude[p+i*FREQ_PER_COL];
        }
        col_magnitude[i] = col_magnitude[i]/FREQ_PER_COL;
        if(col_magnitude[i] > col_magnitude_max)
            col_magnitude_max = col_magnitude[i];
    }
#endif

}


static int ledIsRunning(void)
{
    int is_running;
    g_mutex_lock(mutex_is_running);
    is_running = running;
    g_mutex_unlock(mutex_is_running);
    return is_running;
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
    struct _ledLine ledLineScope;
    int shift_speed = 0;
    time_t rawtime;
    struct tm *ptm;
    int last_mpd_state = 0;
    char time_string[20];
    enum _screenToDraw screen_to_draw, *screen_switch;
    char *text_to_set;
    int shift;
    int lifetime;
    int red_on = 0;

    screen_to_draw = SCREEN_TIME;
    
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
    g_async_queue_ref(async_queue_insert_fifo);
    g_async_queue_ref(async_queue_set_static_text);

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
            else if(screen_to_draw == SCREEN_SCOPE)
            {
                mpdFifoInit();
                ledLineToDraw = &ledLineScope;
                mpdFifoUpdate();
                shift_speed = 0;
                int i;
                for(i=0;i<64;i++)
                {
                    uint8_t value = (uint8_t)(col_magnitude[i]/430000*(i+1));
                    if(value == 0) {
                        ledLineScope.column_red[i] = 0;
                        ledLineScope.column_green[i] = 0;
                    } else if(value == 1){
                        ledLineScope.column_red[i] = 0;
                        ledLineScope.column_green[i] = 0x8000;
                    } else if(value == 2){
                        ledLineScope.column_red[i] = 0;
                        ledLineScope.column_green[i] = 0xC000;
                    } else if(value == 3){
                        ledLineScope.column_red[i] = 0;
                        ledLineScope.column_green[i] = 0xE000;
                    } else if(value == 4){
                        ledLineScope.column_red[i] = 0;
                        ledLineScope.column_green[i] = 0xF000;
                    } else if(value == 5){
                        ledLineScope.column_red[i] = 0;
                        ledLineScope.column_green[i] = 0xF800;
                    } else if(value == 6){
                        ledLineScope.column_red[i] = 0;
                        ledLineScope.column_green[i] = 0xFC00;
                    } else if(value == 7){
                        ledLineScope.column_red[i] = 0xFE00 - 0xFC00;
                        ledLineScope.column_green[i] = 0xFE00;
                    } else if(value == 8){
                        ledLineScope.column_red[i] = 0xFF00 - 0xFC00;
                        ledLineScope.column_green[i] = 0xFF00;
                    } else if(value == 9){
                        ledLineScope.column_red[i] = 0xFF80 - 0xFC00;
                        ledLineScope.column_green[i] = 0xFF80;
                    } else if(value == 10){
                        ledLineScope.column_red[i] = 0xFFC0 - 0xFC00;
                        ledLineScope.column_green[i] = 0xFFC0;
                    } else if(value == 11){
                        ledLineScope.column_red[i] = 0xFFE0 - 0xFC00;
                        ledLineScope.column_green[i] = 0xFFC0;
                    } else if(value == 12){
                        ledLineScope.column_red[i] = 0xFFF0 - 0xFC00;
                        ledLineScope.column_green[i] = 0xFFC0;
                    } else if(value == 13){
                        ledLineScope.column_red[i] = 0xFFF8 - 0xFC00;
                        ledLineScope.column_green[i] = 0xFFC0;
                    } else if(value == 14){
                        ledLineScope.column_red[i] = 0xFFFC - 0xFC00;
                        ledLineScope.column_green[i] = 0xFFC0;
                    } else if(value >= 15){
                        ledLineScope.column_red[i] = 0xFFFE - 0xFC00;
                        ledLineScope.column_green[i] = 0xFFC0;
                    }
                }
            }
            g_mutex_lock(current_screen_mutex);
            current_screen = screen_to_draw;
            g_mutex_unlock(current_screen_mutex);
            ledDisplayMain(ledLineToDraw, shift_speed);
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
    g_async_queue_unref(async_queue_insert_fifo);
    g_async_queue_unref(async_queue_set_static_text);
    
    mpdFifoClose();
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
//        g_usleep(1000);
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

