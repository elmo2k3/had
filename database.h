/*
 * Copyright (C) 2007-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file    database.h
 * \brief   header for mysql database functions
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <time.h>

#include "mysql/mysql.h"
#include "had.h"

#define DATABASE_FIFO_SIZE 1024

/** get the daily graph
 * \param modul modul number
 * \param sensor sensor number
 * \param *graph graphPacket to store the graph in
 */
void getDailyGraph(int modul, int sensor, struct graphPacket *graph);

/** get the last measured temperature
 * \param modul modul
 * \param sensor sensor
 * \param *temp store celsius here
 * \param *temp_deci store decicelsius here
 */
void getLastTemperature(int modul, int sensor, int *temp, int *temp_deci);

/** insert measured temperature
 * \param modul modul
 * \param sensor sensor
 */
void databaseInsertTemperature(int modul, int sensor, float *temperature, time_t timestamp);
void databaseInsertDigitalValue(int modul, int sensor, int value, time_t timestamp); 

#endif

