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

#include "listen.h"
#include "socket_util.h"
#include "client.h"
#include "fd_util.h"
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "listen"

#define DEFAULT_PORT    4123

struct listen_socket {
    struct listen_socket *next;

    int fd;

    guint source_id;
};

static struct listen_socket *listen_sockets;
int listen_port;

static gboolean
listen_in_event(GIOChannel *source, GIOCondition condition, gpointer data);

static bool
listen_add_address(int pf, const struct sockaddr *addrp, socklen_t addrlen,
           GError **error)
{
    char *address_string;
    int fd;
    struct listen_socket *ls;
    GIOChannel *channel;

    address_string = sockaddr_to_string(addrp, addrlen, NULL);
    if (address_string != NULL) {
        g_debug("binding to socket address %s", address_string);
        g_free(address_string);
    }

    fd = socket_bind_listen(pf, SOCK_STREAM, 0, addrp, addrlen, 5, error);
    if (fd < 0)
        return false;

    ls = g_new(struct listen_socket, 1);
    ls->fd = fd;

    channel = g_io_channel_unix_new(fd);
    ls->source_id = g_io_add_watch(channel, G_IO_IN,
                       listen_in_event, GINT_TO_POINTER(fd));
    g_io_channel_unref(channel);

    ls->next = listen_sockets;
    listen_sockets = ls;

    return true;
}

/**
 * Add a listener on a port on all IPv4 interfaces.
 *
 * @param port the TCP port
 * @param error location to store the error occuring, or NULL to ignore errors
 * @return true on success
 */
static bool
listen_add_port_ipv4(unsigned int port, GError **error)
{
    struct sockaddr_in sin;
    const struct sockaddr *addrp = (const struct sockaddr *)&sin;
    socklen_t addrlen = sizeof(sin);

    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(port);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;

    return listen_add_address(PF_INET, addrp, addrlen, error);
}

#ifdef HAVE_IPV6
/**
 * Add a listener on a port on all IPv6 interfaces.
 *
 * @param port the TCP port
 * @param error location to store the error occuring, or NULL to ignore errors
 * @return true on success
 */
static bool
listen_add_port_ipv6(unsigned int port, GError **error)
{
    struct sockaddr_in6 sin;
    const struct sockaddr *addrp = (const struct sockaddr *)&sin;
    socklen_t addrlen = sizeof(sin);

    memset(&sin, 0, sizeof(sin));
    sin.sin6_port = htons(port);
    sin.sin6_family = AF_INET6;

    return listen_add_address(PF_INET6, addrp, addrlen, error);
}
#endif /* HAVE_IPV6 */

/**
 * Add a listener on a port on all interfaces.
 *
 * @param port the TCP port
 * @param error location to store the error occuring, or NULL to ignore errors
 * @return true on success
 */
static bool
listen_add_port(unsigned int port, GError **error)
{
    bool success;
#ifdef HAVE_IPV6
    bool success6;
    GError *error2 = NULL;
#endif

    g_debug("binding to any address");

#ifdef HAVE_IPV6
#warning hello
    success6 = listen_add_port_ipv6(port, &error2);
    if (!success6) {
        if (
            (error2->code != EAFNOSUPPORT && error2->code != EINVAL &&
             error2->code != EPROTONOSUPPORT)) {
            g_propagate_error(error, error2);
            return false;
        }

        /* although MPD was compiled with IPv6 support, this
           host does not have it - ignore this error */
        g_error_free(error2);
    }
#endif

    success = listen_add_port_ipv4(port, error);
    if (!success) {
#ifdef HAVE_IPV6
        if (success6)
            /* non-critical: IPv6 listener is
               already set up */
            g_clear_error(error);
        else
#endif
            return false;
    }

    return true;
}

#ifdef HAVE_UN
/**
 * Add a listener on a Unix domain socket.
 *
 * @param path the absolute socket path
 * @param error location to store the error occuring, or NULL to ignore errors
 * @return true on success
 */
static bool
listen_add_path(const char *path, GError **error)
{
    size_t path_length;
    struct sockaddr_un s_un;
    const struct sockaddr *addrp = (const struct sockaddr *)&s_un;
    socklen_t addrlen = sizeof(s_un);
    bool success;

    path_length = strlen(path);
    if (path_length >= sizeof(s_un.sun_path)) {
        g_set_error(error, listen_quark(), 0,
                "unix socket path is too long");
        return false;
    }

    unlink(path);

    s_un.sun_family = AF_UNIX;
    memcpy(s_un.sun_path, path, path_length + 1);

    success = listen_add_address(PF_UNIX, addrp, addrlen, error);
    if (!success)
        return false;

    /* allow everybody to connect */
    chmod(path, 0666);

    return true;
}
#endif /* HAVE_UN */

bool
listen_global_init(GError **error_r)
{
    int port = DEFAULT_PORT;
    bool success;
    GError *error = NULL;

    /* no "bind_to_address" configured, bind the
       configured port on all interfaces */

    success = listen_add_port(port, &error);
    if (!success) {
        g_propagate_prefixed_error(error_r, error,
                       "Failed to listen on *:%d: ",
                       port);
        return false;
    }

    listen_port = port;
    return true;
}

void listen_global_finish(void)
{
    g_debug("listen_global_finish called");

    while (listen_sockets != NULL) {
        struct listen_socket *ls = listen_sockets;
        listen_sockets = ls->next;

        g_source_remove(ls->source_id);
        close(ls->fd);
        g_free(ls);
    }
}

static int get_remote_uid(int fd)
{
#ifdef HAVE_STRUCT_UCRED
    struct ucred cred;
    socklen_t len = sizeof (cred);

    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
        return 0;

    return cred.uid;
#else
    (void)fd;
    return -1;
#endif
}

static gboolean
listen_in_event(G_GNUC_UNUSED GIOChannel *source,
        G_GNUC_UNUSED GIOCondition condition,
        gpointer data)
{
    int listen_fd = GPOINTER_TO_INT(data), fd;
    struct sockaddr_storage sa;
    size_t sa_length = sizeof(sa);

    fd = accept_cloexec_nonblock(listen_fd, (struct sockaddr*)&sa,
                     &sa_length);
    if (fd >= 0) {
        client_new(fd, (struct sockaddr*)&sa, sa_length,
               get_remote_uid(fd));
    } else if (fd < 0 && errno != EINTR) {
        g_warning("Problems accept()'ing");
    }

    return true;
}
