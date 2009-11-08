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
 * \file	modules.h
 * \brief	module handling
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/
#ifndef __MODULE_H__
#define __MODULE_H__

#include <gmodule.h>

struct _had_module
{
	GModule *module;
	gchar name[20];
	gchar full_name[24];
	gboolean loaded;
	gchar **depencies;
};

static GList *had_modules;

void hadModulesInit(void);

#endif

