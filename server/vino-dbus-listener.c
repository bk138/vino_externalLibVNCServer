/*
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2006 Jonh Wendell <wendell@bani.com.br>
 * Copyright (C) 2007 Mark McLoughlin <markmc@skynet.ie>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      William Jon McCann <mccann@jhu.edu>
 *      Jonh Wendell <wendell@bani.com.br>
 *      Mark McLoughlin <mark@skynet.ie>
 *
 * Code taken from gnome-screensaver/src/gs-listener-dbus.c
 */

#include "config.h"

#include "vino-dbus-listener.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <dbus/dbus-glib-bindings.h>

#include "vino-util.h"
#include "vino-mdns.h"
#ifdef VINO_ENABLE_HTTP_SERVER
#include "vino-http.h"
#endif

#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#else
#include "libvncserver/ifaddr/ifaddrs.h"
#endif

#define VINO_DBUS_INTERFACE "org.gnome.VinoScreen"
#define VINO_DBUS_BUS_NAME  "org.gnome.Vino"

G_DEFINE_TYPE (VinoDBusListener, vino_dbus_listener, G_TYPE_OBJECT)

struct _VinoDBusListenerPrivate
{
  VinoServer *server;
};

enum
{
  PROP_0,
  PROP_SERVER
};

static gboolean
vino_dbus_listener_get_external_port (VinoDBusListener *listener,
                                      gdouble *ret,
                                      GError **error);

static gboolean
vino_dbus_listener_get_internal_data (VinoDBusListener *listener,
                                      char ** hostname,
                                      char ** avahi_hostname,
                                      gdouble * port,
                                      GError **error);

#include "dbus-interface-glue.h"

static void vino_dbus_listener_set_server (VinoDBusListener *listener,
                                           VinoServer       *server);

static char *
get_local_hostname (VinoDBusListener *listener)
{
  char                *retval, buf[INET6_ADDRSTRLEN];
  struct ifaddrs      *myaddrs, *ifa; 
  void                *sin;
  const char          *server_iface;
  GHashTable          *ipv4, *ipv6;
  GHashTableIter      iter;
  gpointer            key, value;

  retval = NULL;
  server_iface = vino_server_get_network_interface (listener->priv->server);
  ipv4 = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
  ipv6 = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  getifaddrs (&myaddrs);
  for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr == NULL || ifa->ifa_name == NULL || (ifa->ifa_flags & IFF_UP) == 0)
	continue;

      switch (ifa->ifa_addr->sa_family)
	{
	  case AF_INET:
	    sin = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
	    inet_ntop (AF_INET, sin, buf, INET6_ADDRSTRLEN);
	    g_hash_table_insert (ipv4,
				 ifa->ifa_name,
				 g_strdup (buf));
	    break;

	  case AF_INET6:
	    sin = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
	    inet_ntop (AF_INET6, sin, buf, INET6_ADDRSTRLEN);
	    g_hash_table_insert (ipv6,
				 ifa->ifa_name,
				 g_strdup (buf));
	    break;
	  default: continue;
	}
    }

  if (server_iface && server_iface[0] != '\0')
    {
      if ((retval = g_strdup (g_hash_table_lookup (ipv4, server_iface))))
	goto the_end;
      if ((retval = g_strdup (g_hash_table_lookup (ipv6, server_iface))))
	goto the_end;
    }

  g_hash_table_iter_init (&iter, ipv4);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (strncmp (key, "lo", 2) == 0)
	continue;
      retval = g_strdup (value);
      goto the_end;
    }

  g_hash_table_iter_init (&iter, ipv6);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (strncmp (key, "lo", 2) == 0)
	continue;
      retval = g_strdup (value);
      goto the_end;
    }

  if ((retval = g_strdup (g_hash_table_lookup (ipv4, "lo"))))
    goto the_end;
  if ((retval = g_strdup (g_hash_table_lookup (ipv6, "lo"))))
    goto the_end;

  the_end:
  freeifaddrs (myaddrs); 
  g_hash_table_destroy (ipv4);
  g_hash_table_destroy (ipv6);

  return retval;
}


static void
vino_dbus_listener_set_property (GObject       *object,
                                 guint          prop_id,
                                 const GValue  *value,
                                 GParamSpec    *pspec)
{
  VinoDBusListener *listener = VINO_DBUS_LISTENER (object);

  switch (prop_id)
    {
    case PROP_SERVER:
      vino_dbus_listener_set_server (listener, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_dbus_listener_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  VinoDBusListener *listener = VINO_DBUS_LISTENER (object);

  switch (prop_id)
    {
    case PROP_SERVER:
      g_value_set_object (value, listener->priv->server);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_dbus_listener_init (VinoDBusListener *listener)
{
  listener->priv = G_TYPE_INSTANCE_GET_PRIVATE (listener, VINO_TYPE_DBUS_LISTENER, VinoDBusListenerPrivate);
}

static guint signal_server_info_changed = 0;

static void
vino_dbus_listener_class_init (VinoDBusListenerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = vino_dbus_listener_get_property;
  object_class->set_property = vino_dbus_listener_set_property;

  signal_server_info_changed = g_signal_new ("server_info_changed", 
      G_OBJECT_CLASS_TYPE (klass), 
      (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
				   PROP_SERVER,
				   g_param_spec_object ("server",
							"Server",
							"The server",
							VINO_TYPE_SERVER,
							G_PARAM_READWRITE      |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_NAME    |
							G_PARAM_STATIC_NICK    |
							G_PARAM_STATIC_BLURB));

  g_type_class_add_private (klass, sizeof (VinoDBusListenerPrivate));
}

VinoDBusListener *
vino_dbus_listener_new (VinoServer *server)
{
  g_return_val_if_fail (VINO_IS_SERVER (server), NULL);

  return g_object_new (VINO_TYPE_DBUS_LISTENER,
                       "server", server,
                       NULL);
}

static gboolean
vino_dbus_listener_get_external_port (VinoDBusListener *listener,
                                      gdouble *ret,
                                      GError **error)
{
  *ret = vino_server_get_external_port (listener->priv->server);

  return TRUE;
}


static gboolean
vino_dbus_listener_get_internal_data (VinoDBusListener *listener,
                                      char ** hostname,
                                      char ** avahi_hostname,
                                      gdouble * port,
                                      GError **error)
{
#ifdef VINO_ENABLE_HTTP_SERVER
  *port = (gdouble)vino_get_http_server_port (listener->priv->server);
#else
  *port = (gdouble)vino_server_get_port (listener->priv->server);
#endif

  *hostname = get_local_hostname (listener);

  *avahi_hostname = g_strdup (vino_mdns_get_hostname ());

  return TRUE;
}

static void
vino_dbus_listener_info_changed (VinoServer *server,
                                 GParamSpec *property,
                                 VinoDBusListener *listener)
{
  dprintf (DBUS, "Emitting ServerInfoChanged signal\n");
  g_signal_emit (listener, signal_server_info_changed, 0);
}

static void
vino_dbus_listener_set_server (VinoDBusListener *listener,
                               VinoServer       *server)
{
  DBusGConnection *conn;
  GdkScreen       *screen;
  char            *obj_path;

  g_assert (listener->priv->server == NULL);

  listener->priv->server = server;

  if (!(conn = vino_dbus_get_connection ()))
    return;

  screen = vino_server_get_screen (listener->priv->server);

  obj_path = g_strdup_printf ("/org/gnome/vino/screens/%d",
                              gdk_screen_get_number (screen));

  dbus_g_connection_register_g_object (conn, obj_path, G_OBJECT (listener));

  dprintf (DBUS, "Object registered at path '%s'\n", obj_path);

  g_signal_connect (server, "notify::alternative-port",
      G_CALLBACK (vino_dbus_listener_info_changed),
      listener);

  g_free (obj_path);
}

VinoServer *
vino_dbus_listener_get_server (VinoDBusListener *listener)
{
  g_return_val_if_fail (VINO_IS_DBUS_LISTENER (listener), NULL);

  return listener->priv->server;
}

static DBusGConnection * vino_dbus_connection = NULL;
static gboolean        vino_dbus_failed_to_connect = FALSE;

DBusGConnection *
vino_dbus_get_connection (void)
{
  DBusConnection * dbus_conn;
  if (vino_dbus_connection == NULL && !vino_dbus_failed_to_connect)
    {
      GError * error = NULL;

      if ((vino_dbus_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error)))
        {
          dprintf (DBUS, "Successfully connected to the message bus\n");
          dbus_conn = dbus_g_connection_get_connection (vino_dbus_connection);
          dbus_connection_set_exit_on_disconnect (dbus_conn, FALSE);
        }
      else
        {
          vino_dbus_failed_to_connect = TRUE;
          g_printerr ("Failed to open connection to bus: %s\n",
              error->message);
          g_error_free (error);
        }
    }

  return vino_dbus_connection;
}

void
vino_dbus_unref_connection (void)
{
  if (vino_dbus_connection != NULL)
    dbus_g_connection_unref (vino_dbus_connection);
  vino_dbus_connection = NULL;
}

gboolean
vino_dbus_request_name (void)
{

  DBusGConnection *connection;
  GError *error = NULL;
  DBusGProxy     *bus_proxy;
  int request_name_result;

  if (!(connection = vino_dbus_get_connection ()))
    return FALSE;

  dbus_g_object_type_install_info (VINO_TYPE_DBUS_LISTENER,
      &dbus_glib_vino_dbus_listener_object_info);

  bus_proxy = dbus_g_proxy_new_for_name (connection,
      "org.freedesktop.DBus",
      "/org/freedesktop/DBus",
      "org.freedesktop.DBus");

  if (!dbus_g_proxy_call(bus_proxy, "RequestName", &error, 
      G_TYPE_STRING, VINO_DBUS_BUS_NAME, G_TYPE_UINT, 0, G_TYPE_INVALID,
      G_TYPE_UINT, &request_name_result, G_TYPE_INVALID))
    {
      g_debug ("Failed to request name: %s",
                error ? error->message : "No error given");
      g_clear_error (&error);
      return FALSE ;
    }

  if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    {
      g_warning (_("Remote Desktop server already running; exiting ...\n"));
      return FALSE;
    }

  dprintf (DBUS, "Successfully acquired D-Bus name '%s'\n", VINO_DBUS_BUS_NAME);
  return TRUE;
}
