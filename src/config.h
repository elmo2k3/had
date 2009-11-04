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
* \file	config.c
* \brief	config file handling
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __HAD_CONFIG_H__
#define __HAD_CONFIG_H__

#include <string.h>
#include <glib.h>

const gchar *config_search_dirs[] = {".", "/etc/", NULL};


gint hadConfigGetInt(gchar *group, gchar *key, gint default_int);
gboolean hadConfigGetBool(gchar *group, gchar *key, gboolean default_bool);
gchar *hadConfigGetString(gchar *group, gchar *key, gchar *default_string);
void hadConfigSetInt(gchar *group, gchar *key, int int_to_set);
void hadConfigSetBool(gchar *group, gchar *key, int bool_to_set);
void hadConfigSetString(gchar *group, gchar *key, gchar *string_to_set);

void hadConfigInit(void);
void hadConfigUnload(void);

#endif

