/*
 * Copyright (C) 2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
#ifndef __BASE_STATION_H__
#define __BASE_STATION_H__

#include <glib.h>

extern int base_station_init(char *serial_device);

/** Send a packet
 *
 * @param *packet pointer to struct holding the data
 * @param type packet type, see had.h
 */
void sendPacket(void *packet, int type);

/** Send a RGB packet
 *
 * \param address address of the rgb modul. Currently can be 1,3 or 4
 * \param red red color
 * \param green green color
 * \param blue blue color
 * \param smoothness time for overblending
 */
void sendRgbPacket(unsigned char address, unsigned char red, unsigned char green, unsigned char blue, unsigned char smoothness);

/** Activate the background light of the basestation
 */
void setBaseLcdOn();

/** Deactivate the background light of the basestation
 */
void setBaseLcdOff();

/** Send text to the basestation lcd display
 *
 * @param *text text to show, max 32 chars
 */
void sendBaseLcdText(char *text);

/** let the base station beep */
void setBeepOn();

/** turn the base station beep off */
extern void setBeepOff();
extern void setCurrentRgbValues();
extern void open_door();
extern void updateGlcd();
extern void base_station_rgb_blink_all(int num);
extern void set_hifi(int on);
extern int base_station_hifi_is_on(void);
extern void base_station_hifi_on(void);
extern void base_station_hifi_off(void);

#endif
