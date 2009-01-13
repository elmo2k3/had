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

/*!
* \file	hr20.h
* \brief	header for functions to communicate with openhr20
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __HR20_H__
#define __HR20_H__

/** struct holding complete status information from the hr20 device */
struct _hr20info
{
	int16_t tempis; /**< current temperature */
	int16_t tempset; /**< user set temperature */
	int8_t valve; /**< how open is the valve? in percent */
	int16_t voltage; /**< voltage of the batteries */
	int8_t mode; /**< mode, 1 for manual, 2 for automatic control */
};

/** read the status information 
 * \param *hr20info struct where the data will be stored
 */
extern int hr20GetStatus(struct _hr20info *hr20info);

/** set wanted temperature
 * \param temperature wanted temperature
 */
extern int hr20SetTemperature(int temperature);

/** set current date and time on hr20 device */
extern int hr20SetDateAndTime();

/** set mode to manual control */
extern void hr20SetModeManu();

/** set mode to automatic control */
extern void hr20SetModeAuto();

#endif

