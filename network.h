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
#ifndef __NETWORK_H__
#define __NETWORK_H__

/** @file network.h
 * Implementation of the network server
 */

#include <netdb.h>
#include <glib.h>

#define GREETING "Welcome to had %s\r\nType commands to get a list of commands\r\n\r\n"
#define CMD_SUCCESSFULL "OK\r\n"
#define CMD_FAIL "FAIL\r\n"
#define CMD_DENIED "DENIED\r\n"

enum NetworkClientPermission
{
	NETWORK_CLIENT_PERMISSION_NONE,
	NETWORK_CLIENT_PERMISSION_READ,
	NETWORK_CLIENT_PERMISSION_ADMIN
};

/*!
 * a tcp server
 */
struct NetworkServer
{
	int fd;	/**< socket descriptor */
	guint listen_watch;	/**< glib channel watcher */
};

/*!
 * a network client
 */
struct client
{
	int fd;					/**< socket descriptor */
	guint num;				/**< number of the client, beginning with zero */
	GIOChannel *channel; 	/**< glib io channel */
	guint source_id; 		/**< glib source */
	gint permission;		/** permission level of the client (NONE,READ,ADMIN) */
	gchar random_number[11]; /** random string used for authentification */
	gchar addr_string[NI_MAXHOST];
	gchar command_buf[2048];
	gchar *line_new_pos;
};

/**
 * Create a new TCP Server
 *
 * \param database is a #TagDatabase
 * \returns dynamically allocated data of the server. use g_free later
 */
extern struct NetworkServer *network_server_new(void);

/**
 * send a string to a client. printf style
 *
 * \param client is a #client
 * \param format like in printf
 * \returns 0 on failure, 1 on success
 */
extern gboolean network_client_printf(struct client *client, char *format, ...);

/**
 * disconnect a client
 *
 * \param client is a #client
 */
extern void network_client_disconnect(struct client *client);

#endif

