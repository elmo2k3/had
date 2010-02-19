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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <poll.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "network.h"
#include "had.h"
#include "commands.h"
#include "version.h"

static GList *clients = NULL;
static guint num_clients;

static gboolean listen_in_event
(GIOChannel *source, GIOCondition condition, gpointer server);

static gboolean client_in_event
(GIOChannel *source, GIOCondition condition, gpointer server);

static void client_process(struct client *client);


static gboolean client_in_event
(GIOChannel *source, GIOCondition condition, gpointer user_data)
{
    struct client *client = user_data;
    GIOStatus status;
    gint ret;
    gchar *line;
    gchar *str_return;
    gsize terminator_pos;
    gsize bytes_read;
    gchar **lines;
    
    if(condition != G_IO_IN)
    {
        network_client_disconnect(client);
        return FALSE;
    }
    g_debug("working on client input");
    //status = g_io_channel_read_line(source, &line, &bytes_read, &terminator_pos, NULL);
    status = g_io_channel_read_chars(source, client->command_buf, 
        sizeof(client->command_buf), &bytes_read, NULL);

    switch(status)
    {
        case G_IO_STATUS_NORMAL:
            client_process(client);
        case G_IO_STATUS_AGAIN:
            g_debug("status again received");
            break;
        case G_IO_STATUS_ERROR:
            g_debug("status error received");
        case G_IO_STATUS_EOF:
            g_debug("eof received");
            network_client_disconnect(client);
            return FALSE;
            break;
    }
    return TRUE;
/*    struct pollfd pollfd = { client->fd, POLLHUP, POLLHUP };
    if(!poll(&pollfd, 1, 1))
        g_io_channel_flush(client->channel, NULL);
    else
    {
        g_io_channel_shutdown(client->channel, FALSE, NULL);
        verbose_printf(0,"poll error\n");
    }*/
    return TRUE;
}

static gchar *split_buf_into_lines(struct client *client)
{
    gchar *temp, *temp2;
    gchar *pos_to_return;

    if(client->line_new_pos == -1) // first run
    {
        client->line_new_pos = client->command_buf;
    }
    if(client->line_new_pos == NULL)
        return NULL;
    if(client->line_new_pos == '\0')
        return NULL;
    
    pos_to_return = client->line_new_pos;

    temp = strchr(client->line_new_pos, '\r');
    temp2 = strchr(client->line_new_pos, '\n');

    if(!temp2) // no \n found
        return NULL;
    else
    {
        *temp2 = '\0';
        client->line_new_pos = temp+1;
    }
    if(temp)
        *temp = '\0';
    return pos_to_return;
}

static void client_process(struct client *client)
{
    int ret;
    gchar *line;

    client->line_new_pos = -1;
    g_debug("client->command_buf = %s",client->command_buf);
    while(line = split_buf_into_lines(client))
    {
        g_debug("working on line %s",line);
        if((ret = commands_process(client,line)) == COMMANDS_OK)
        {
            if(g_io_channel_write_chars(client->channel, CMD_SUCCESSFULL,
                sizeof(CMD_SUCCESSFULL), NULL, NULL) != G_IO_STATUS_NORMAL)
            {
                network_client_disconnect(client);
                return;
            }
            
        }
        else if (ret == COMMANDS_FAIL)
        {
            if(g_io_channel_write_chars(client->channel, CMD_FAIL,
                sizeof(CMD_FAIL), NULL, NULL) != G_IO_STATUS_NORMAL)
            {
                network_client_disconnect(client);
                return;
            }
        }
        else if (ret == COMMANDS_DENIED)
        {
            if(g_io_channel_write_chars(client->channel, CMD_DENIED,
                sizeof(CMD_DENIED), NULL, NULL) != G_IO_STATUS_NORMAL)
            {
                network_client_disconnect(client);
                return;
            }
        }
        else if (ret == COMMANDS_DISCONNECT)
        {
            network_client_disconnect(client);
            return;
        }
    }
    struct pollfd pollfd = { client->fd, POLLHUP, POLLHUP };
    if(!poll(&pollfd, 1, 1))
        g_io_channel_flush(client->channel, NULL);
    else
    {
        g_io_channel_shutdown(client->channel, FALSE, NULL);
        g_debug("poll error");
    }
}

void network_client_disconnect(struct client *client)
{
    clients = g_list_remove(clients, client);
    num_clients--;
    if(client->source_id)
    {
        g_source_remove(client->source_id);
        client->source_id = 0;
    }
    if(client->channel)
    {
        g_io_channel_unref(client->channel);
        client->channel = NULL;
        verbose_printf(0,"client %d disconnected\n",client->num);
        g_free(client);
    }
}


static gboolean listen_in_event
(GIOChannel *source, GIOCondition condition, gpointer data)
{
    struct NetworkServer *server = (struct NetworkServer*)data;
    int fd;
    struct sockaddr_storage sa;
    socklen_t sa_length = sizeof(sa);
    int flags;
    struct client *client;

    fd = accept(server->fd, (struct sockaddr*)&sa, &sa_length);
    if(fd >= 0)
    {
        char servername[NI_MAXSERV];
        /* set nonblocking. copy&paste from mpd */
        while ((flags = fcntl(fd, F_GETFL)) < 0 && errno == EINTR);
        flags |= O_NONBLOCK;
        while ((fcntl(fd, F_SETFL, flags)) < 0 && errno == EINTR);
        
        if(num_clients >= 10)
        {
            close(fd);
            return TRUE;
        }
        client = g_new0(struct client, 1);
        clients = g_list_prepend(clients, client);
        client->num = num_clients;
        client->fd = fd;
        client->permission = 0;
        client->channel = g_io_channel_unix_new(fd);
        getnameinfo((struct sockaddr*)&sa, sa_length, client->addr_string, NI_MAXHOST,
            servername,sizeof(servername), NI_NUMERICHOST|NI_NUMERICSERV);
        client->addr_string[30] = '\0';
        verbose_printf(0,"client %d %s connected\n",num_clients, client->addr_string);    
        num_clients++;
        g_sprintf(client->random_number,"%d",g_random_int());
        g_io_channel_set_close_on_unref(client->channel, TRUE);
        g_io_channel_set_encoding(client->channel, NULL, NULL);
//        g_io_channel_set_buffered(client->channel, FALSE);
        client->source_id = g_io_add_watch(client->channel, G_IO_IN|G_IO_ERR|G_IO_HUP,
                            client_in_event, client);
//        network_client_printf(client, GREETING, VERSION);
//        g_io_channel_flush(client->channel, NULL);
    }
    return TRUE;
}

        
struct NetworkServer *network_server_new(void)
{
    struct sockaddr_in serveraddr;
    int y = 1;
    struct NetworkServer *server;
    GIOChannel *channel;

    server = g_new0(struct NetworkServer, 1);

    server->fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( server->fd < 0 )
    {
        g_fprintf(stderr,"Could not create socket\n");
        g_free(server);
        return NULL;
    }
    
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(4123);

    setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));

    if(bind(server->fd,(struct sockaddr*)&serveraddr, sizeof( serveraddr)) < 0)
    {
        g_fprintf(stderr,"Could not start tcp server\n");
        g_free(server);
        return NULL;
    }

    if(listen(server->fd, 5) == -1)
    {
        g_fprintf(stderr,"Could not create listener\n");
        g_free(server);
        return NULL;
    }
    
    channel = g_io_channel_unix_new(server->fd);
    server->listen_watch = g_io_add_watch(channel, G_IO_IN, listen_in_event, server);
    g_io_channel_unref(channel);

    return server;
}

gboolean network_client_printf(struct client *client, char *format, ...)
{
    va_list args, tmp;
    int length;
    gchar *buffer;

    va_start(args, format);
    va_copy(tmp, args);
    length = vsnprintf(NULL, 0, format, tmp);
    va_end(tmp);

    if(length <= 0)
        return FALSE;

    buffer = g_malloc(length + 1);
    vsnprintf(buffer, length + 1, format, args);
    if(g_io_channel_write_chars(client->channel, buffer, length, NULL, NULL) != G_IO_STATUS_NORMAL)
    {
        network_client_disconnect(client);
        g_free(buffer);
        va_end(args);
        return FALSE;
    }
    g_free(buffer);
    va_end(args);
    return TRUE;
}
