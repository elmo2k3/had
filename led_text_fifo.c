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

#include <glib.h>
#include "led_routines.h"
#include "led_text_fifo.h"

#include "fonts/8x8.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "led_text_fifo"

static void ledRemoveFromFifo(void);
static struct _ledLine ledLineFifo[LED_FIFO_SIZE];

int led_fifo_top;
int led_fifo_bottom;

int led_line_fifo_time[LED_FIFO_SIZE];
int led_line_fifo_shift[LED_FIFO_SIZE];

void ledInternalInsertFifo(char *string, int shift, int lifetime)
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
    putString(string,font8x8,&ledLineFifo[led_fifo_top]);
    
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

int ledFifoEmpty(void)
{
    return led_fifo_top == led_fifo_bottom;
}

int ledFifoGetShiftSpeed(void)
{
    return led_line_fifo_shift[led_fifo_bottom];
}

struct _ledLine *ledFifoGetLine(void)
{
    struct _ledLine *led_line_return;
    led_line_return = &ledLineFifo[led_fifo_bottom];
    led_line_fifo_time[led_fifo_bottom]--;
    if(!led_line_fifo_time[led_fifo_bottom])
    {
        ledRemoveFromFifo();
    }
    return led_line_return;
}

