/*
 * Copyright (C) 2010 Bjoern Biesenbach <bjoern@bjoern-b.de> (minor changes)
 * Copyright (C) 2003-2010 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "client_internal.h"
#include "fifo_buffer.h"
#include "socket_util.h"
#include "command.h"
#include "version.h"

#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>

#define LOG_LEVEL_SECURE G_LOG_LEVEL_INFO

static const char GREETING[] = "OK HAD " GIT_VERSION "\n";

void client_new(int fd, const struct sockaddr *sa, size_t sa_length, int uid)
{
    static unsigned int next_client_num;
    struct client *client;
    char *remote;

    assert(fd >= 0);

#ifdef HAVE_LIBWRAP
    if (sa->sa_family != AF_UNIX) {
        char *hostaddr = sockaddr_to_string(sa, sa_length, NULL);
        const char *progname = g_get_prgname();

        struct request_info req;
        request_init(&req, RQ_FILE, fd, RQ_DAEMON, progname, 0);

        fromhost(&req);

        if (!hosts_access(&req)) {
            /* tcp wrappers says no */
            g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE,
                  "libwrap refused connection (libwrap=%s) from %s",
                  progname, hostaddr);

            g_free(hostaddr);
            close(fd);
            return;
        }

        g_free(hostaddr);
    }
#endif  /* HAVE_WRAP */

    if (client_list_is_full()) {
        g_warning("Max Connections Reached!");
        close(fd);
        return;
    }

    client = g_new0(struct client, 1);

#ifndef G_OS_WIN32
    client->channel = g_io_channel_unix_new(fd);
#else
    client->channel = g_io_channel_win32_new_socket(fd);
#endif
    /* GLib is responsible for closing the file descriptor */
    g_io_channel_set_close_on_unref(client->channel, true);
    /* NULL encoding means the stream is binary safe; the MPD
       protocol is UTF-8 only, but we are doing this call anyway
       to prevent GLib from messing around with the stream */
    g_io_channel_set_encoding(client->channel, NULL, NULL);
    /* we prefer to do buffering */
    g_io_channel_set_buffered(client->channel, false);

    client->source_id = g_io_add_watch(client->channel,
                       G_IO_IN|G_IO_ERR|G_IO_HUP,
                       client_in_event, client);

    client->input = fifo_buffer_new(4096);

    client->permission = PERMISSION_ADMIN;
    client->uid = uid;

    client->last_activity = g_timer_new();

    client->cmd_list = NULL;
    client->cmd_list_OK = -1;
    client->cmd_list_size = 0;

    client->deferred_send = g_queue_new();
    client->deferred_bytes = 0;
    client->num = next_client_num++;

    client->send_buf_used = 0;

//  (void)send(fd, GREETING, sizeof(GREETING) - 1,0);
//  client_puts(client,GREETING);   

    client_list_add(client);

    remote = sockaddr_to_string(sa, sa_length, NULL);
    g_debug("[%u] opened from %s", client->num, remote);
    g_free(remote);
}

static void
deferred_buffer_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    struct deferred_buffer *buffer = data;
    g_free(buffer);
}

void
client_close(struct client *client)
{
    client_list_remove(client);

    client_set_expired(client);

    g_timer_destroy(client->last_activity);

    if (client->cmd_list) {
        free_cmd_list(client->cmd_list);
        client->cmd_list = NULL;
    }

    g_queue_foreach(client->deferred_send, deferred_buffer_free, NULL);
    g_queue_free(client->deferred_send);

    fifo_buffer_free(client->input);

    g_debug("[%u] closed", client->num);
    g_free(client);
}
