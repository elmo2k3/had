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

#ifndef __LED_ROUTINES_H__
#define __LED_ROUTINES_H__

/** @file led_routines.h
 * Module for the led-matrix-display
 */

#include <inttypes.h>


#define COLOR_RED 0
#define COLOR_GREEN 1
#define COLOR_AMBER 2

#define LINE_LENGTH 512
#define LED_MAX_STACK 30

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

/** 
 * Checks if the thread for the led-matrix is running
 *
 * @returns returns 0 on stopped, 1 on running
 */
extern int ledIsRunning(void);

/** Start main thread for the led-matrix-display
 *
 * May only be started once
 */
extern void ledMatrixThread(void);

/** Stop the led-matrix-display thread
 */
extern void stopLedMatrixThread(void);

/** Transmit to display
 *
 * Takes the _output arrays of _ledLine, converts them to the right format
 * and sends the data to the display
 *
 * @param ledLine the line to output
 */
extern void updateDisplay(struct _ledLine ledLine);

/** Allocate memory for a line
 *
 * Uses malloc so freeLedLine(..) has to be called later to avoid memory leaks
 *
 * @param *ledLine reference to the line
 * @param line_length IMPORTANT: by now, only use LINE_LENGTH
 */
extern int allocateLedLine(struct _ledLine *ledLine, int line_length);

/** Free memory for a line
 *
 * Beware, no checking here. If ledLine was freed yet you'll get a segfault
 *
 * @param ledLine line to be freed
 */
extern void freeLedLine(struct _ledLine ledLine);

/** Append a character to line
 *
 * @param c character to draw
 * @param color can be COLOR_RED, COLOR_GREEN or COLOR_AMBER
 * @param *ledLine pointer to the struct where the character should be drawn
 * 
 * @return 0 on failure (LINE_LENGTH reached), 1 on success
 */
extern int putChar(char c, uint8_t color, struct _ledLine *ledLine);

/** Append a string to line
 *
 * @param *string null-terminated string
 * @param color can be COLOR_RED, COLOR_GREEN or COLOR_AMBER
 * @param *ledLine reference to the line
 */
extern void putString(char *string, uint8_t color, struct _ledLine *ledLine);

/** Clear a line
 *
 * Clears everything in the line and set back all positions
 */
extern void clearScreen(struct _ledLine *ledLine);

/** Shift a line left
 *
 * The line gets shifted left by one column. An extra space of 10 columns is
 * added at the end of the line to gurantee spacing between the end and the 
 * beginng of the string.
 *
 * @return 0 when the beginning of line is at the beginng of the display, 1 otherwise
 */
extern int shiftLeft(struct _ledLine *ledLine);

/** 
 * Push a string onto the display stack
 *
 * The string will automatically be scrolled if it does not fit into the display.
 * It will be scrolled completely for lifetime cycles.
 * When called while stack is not empty, the new string will be shown first for lifetime
 *
 * @param *string null-terminated string
 * @param color can be COLOR_RED, COLOR_GREEN or COLOR_AMBER
 * @param shift currently ignored
 * @param lifetime num of cycled this string will be displayed
 */
extern void ledPushToStack(char *string, int color, int shift, int lifetime);


#endif

