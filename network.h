/*
 * Copyright (C) 2007-2008 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

#ifndef __NETWORK_H__
#define __NETWORK_H__

#define LISTENER_PORT 4123

extern void networkThread(void);

#define CMD_NETWORK_QUIT 0
#define CMD_NETWORK_RGB 1
#define CMD_NETWORK_GET_RGB 2
#define CMD_NETWORK_BLINK 3
#define CMD_NETWORK_GET_TEMPERATURE 4
#define CMD_NETWORK_GET_VOLTAGE 5
#define CMD_NETWORK_RELAIS 6
#define CMD_NETWORK_GET_RELAIS 7

#define BUF_SIZE 1024
#define MAX_CLIENTS 10

#endif

