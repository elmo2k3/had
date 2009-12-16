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
#ifndef __RFID_TAG_READER_H__
#define __RFID_TAG_READER_H__

/** @file rfid_tag_reader.h
 * Connection to the Rfid Tag Reader <http://www.mikrocontroller.net/topic/68442>
 */

#include <glib.h>

/**
 *	struct for one rfid tag reader
 */
struct RfidTagReader
{
	gchar tagid[11];	/**< currently scanned id of tag */
	gchar last_tagid[11]; /**< tag id of the last tag */
	guint tagposition;	/**< position in the tagid char array */
	guint serial_port_watcher; /**< glib watcher */
	gchar error_string[1024]; /**< last error should be stored here (not in use yet) */
	gboolean timeout_active; /**< timeout currently active? */
	void (*callback)(void*); /** function to call after tag was successfully scanned */
	void *user_data; /** data to pass to the function that is called */
};

/**
 * Create a new Rfid Tag Reader interface
 *
 * \param serial_device path to serial device (e.g. /dev/ttyS0)
 * \returns dynamically allocated struct of #RfidTagReader. use g_free later
 */
extern struct RfidTagReader *rfid_tag_reader_new(char *serial_device);

/**
 * get the last tagid that was scanned
 *
 * \param tag_reader is a #RfidTagReader
 * \returns string holding the id. don't free this
 */
extern gchar *rfid_tag_reader_last_tag(struct RfidTagReader *tag_reader);

/**
 * set the callback function when a tag was successfully scanned
 *
 * \param tag_reader is a #RfidTagReader
 * \param callback the function to call back
 * \param user_data data to pass to the function
 */
extern void rfid_tag_reader_set_callback(struct RfidTagReader *tag_reader, void *callback);

#endif
