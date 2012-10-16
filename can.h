/*
 * Copyright (C) 2009-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
#ifndef __CAN_H__
#define __CAN_H__

#include <glib.h>
#include <stdint.h>

struct Hr20State
{
    int16_t tempis; /**< current temperature */
    int16_t tempset; /**< user set temperature */
    int8_t valve; /**< how open is the valve? in percent */
    int16_t voltage; /**< voltage of the batteries */
    int8_t mode; /**< mode, 1 for manual, 2 for automatic control */
    int16_t auto_temperature[4];
	uint32_t data_timestamp;
	uint8_t error_code;
	uint8_t window_open;
	uint8_t data_valid;
};

struct CanNode
{
    int address;
    char name[64];
    char relais_name[6][64];
    int relais_state;
    int relais_address[6];
    int relais_relais[6];
    time_t time_last_active;
    time_t uptime;
	int version;
	int voltage;
    struct Hr20State hr20_state;
};

extern void can_init(void);
extern struct CanNode *can_get_node(int address);
extern void can_set_relais(int address, int relais, int state);
extern void can_toggle_relais(int address, int relais);
extern void can_set_temperature(int address, int temperature);
extern void can_set_mode_manu(int address);
extern void can_set_mode_auto(int address);
extern void can_set_date(int address);
#endif
