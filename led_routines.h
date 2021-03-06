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
* \file led_routines.h
* \brief    header for functions for the ledmatrix display
* \author   Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __LED_ROUTINES_H__
#define __LED_ROUTINES_H__

#include <inttypes.h>
#include "mpd.h"


#define COLOR_RED 0
#define COLOR_GREEN 1
#define COLOR_AMBER 2

#define LINE_LENGTH 512

//#define LED_HEADFIRST

/** Struct holding a line drawable at the display
 *
 */
struct _ledLine
{
    uint16_t *column_red; /**< the red part of the string will be put here */
    uint16_t *column_green; /**< the green part of the string will be put here */
    uint16_t *column_red_output; /**< red part of the string, possibly shifted */
    uint16_t *column_green_output; /**< green part of the string, possibly shifted */
    int x; /**< current x position */
    int y; /**< current y position */
    int shift_position; /**< position of the output arrays */
};

enum _screenToDraw
{
    SCREEN_VOID,
    SCREEN_TIME,
    SCREEN_MPD,
    SCREEN_TEMPERATURES,
    SCREEN_STATIC_TEXT,
    SCREEN_SCOPE
};

/** Start main thread for the led-matrix-display
 *
 * May only be started once
 */
extern void ledMatrixStart(void);

/** Stop the led-matrix-display thread
 */
extern void ledMatrixStop(void);

extern void ledFifoInsert(char *string, int shift, int lifetime);
extern void ledMatrixToggle(void);
extern void ledMatrixInit(void);
extern void ledMatrixSelectScreen(enum _screenToDraw screen);
extern void ledMatrixSetMpdText(struct _artist_title *artist_title);
extern void ledMatrixSetStaticText(char *text);
extern enum _screenToDraw ledMatrixCurrentScreen(void);
extern int ledIsRunning(void);
extern int allocateLedLine(struct _ledLine *ledLine, int line_length);
extern void putString(char *string, const char (*font)[24],struct _ledLine *ledLine);
extern void freeLedLine(struct _ledLine *ledLine);

#endif

