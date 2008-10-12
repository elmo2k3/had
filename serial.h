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

#ifndef __SERIAL_H__
#define __SERIAL_H__

#define BAUDRATE B19200

/** @file serial.h
 */

/** Init serial device
 * @return 0 on failure, 1 on success
 */
int initSerial(char *device);

/** Read a line
 *
 * @param *buf char buffer should be 255
 * @return number of read bytes
 */
int readSerial(char *buf);

/** Send a packet
 *
 * @param *packet pointer to struct holding the data
 * @param type packet type, see had.h
 */
void sendPacket(void *packet, int type);

/** Send a RGB packet
 *
 * @param address address of the rgb modul. Currently can be 1,3 or 4
 */
void sendRgbPacket(unsigned char address, unsigned char red, unsigned char green, unsigned char blue, unsigned char smoothness);

#endif

