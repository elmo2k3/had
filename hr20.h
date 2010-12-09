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

/*!
* \file hr20.h
* \brief    header for functions to communicate with openhr20
* \author   Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __HR20_H__
#define __HR20_H__

#include <glib.h>

void hr20Init(void);
gboolean hr20update();

float hr20GetTemperatureIs();
float hr20GetTemperatureSet();
float hr20GetAutoTemperature(int slot);
float hr20GetVoltage();
int   hr20GetValve();
int   hr20GetMode();

/** set wanted temperature
 * \param temperature wanted temperature
 */
int hr20SetTemperature(int temperature);
int hr20SetAutoTemperature(int slot, int temperature);

/** set current date and time on hr20 device */
int hr20SetDateAndTime();

/** set mode to manual control */
void hr20SetModeManu();

/** set mode to automatic control */
void hr20SetModeAuto();


#endif

