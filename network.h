/*
 * Copyright (C) 2007-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*!
* \file	network.h
* \brief	header for network server
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#define LISTENER_PORT 4123

extern void networkThread(void);

/* Commands for had */
#define CMD_NETWORK_QUIT 0
#define CMD_NETWORK_RGB 1
#define CMD_NETWORK_GET_RGB 2
#define CMD_NETWORK_BLINK 3
#define CMD_NETWORK_GET_TEMPERATURE 4
#define CMD_NETWORK_GET_VOLTAGE 5
#define CMD_NETWORK_RELAIS 6
#define CMD_NETWORK_GET_RELAIS 7
#define CMD_NETWORK_LED_DISPLAY_TEXT 8
#define CMD_NETWORK_BASE_LCD_ON 9
#define CMD_NETWORK_BASE_LCD_OFF 10
#define CMD_NETWORK_BASE_LCD_TEXT 11
#define CMD_NETWORK_GET_HAD_STATE 12
#define CMD_NETWORK_SET_HAD_STATE 13
#define CMD_NETWORK_GET_HR20 14
#define CMD_NETWORK_SET_HR20_TEMPERATURE 15
#define CMD_NETWORK_SET_HR20_MODE 16
#define CMD_NETWORK_SET_HR20_AUTO_TEMPERATURE 17

#define CMD_NETWORK_AUTH_FAILURE 0
#define CMD_NETWORK_AUTH_SUCCESS 1

#define HR20_MODE_MANU 1
#define HR20_MODE_AUTO 2

#define BUF_SIZE 1024
#define MAX_CLIENTS 10

#endif

