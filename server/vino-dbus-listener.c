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
      if (strcmp (key, "lo") == 0)
	continue;
      retval = g_strdup (value);
      goto the_end;
    }

  g_hash_table_iter_init (&iter, ipv6);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (strcmp (key, "lo") == 0)
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

static void
vino_dbus_listener_class_init (VinoDBusListenerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = vino_dbus_listener_get_property;
  object_class->set_property = vino_dbus_listener_set_property;

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

static const char * introspect_xml =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "                      \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
  "    <method name=\"Introspect\">\n"
  "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "  <interface name=\"org.gnome.VinoScreen\">\n"
  "    <method name=\"GetInternalData\">\n"
  "      <arg name=\"hostname\" direction=\"out\" type=\"s\"/>\n"
  "      <arg name=\"avahi_hostname\" direction=\"out\" type=\"s\"/>\n"
  "      <arg name=\"port\" direction=\"out\" type=\"d\"/>\n"
  "    </method>\n"
  "    <method name=\"GetExternalPort\">\n"
  "      <arg name=\"port\" direction=\"out\" type=\"d\"/>\n"
  "    </method>\n"
  "    <signal name=\"ServerInfoChanged\">\n"
  "    </signal>\n"
  "  </interface>\n"
  "</node>\n";

static DBusHandlerResult
vino_dbus_listener_handle_introspect (VinoDBusListener *listener,
                                      DBusConnection   *connection,
                                      DBusMessage      *message)
{
  DBusMessage *reply;

  if (!(reply = dbus_message_new_method_return (message)))
    goto oom;

  if (!dbus_message_append_args (reply,
                                 DBUS_TYPE_STRING, &introspect_xml,
                                 DBUS_TYPE_INVALID))
    goto oom;

  if (!dbus_connection_send (connection, reply, NULL))
    goto oom;

  dbus_message_unref (reply);

  dprintf (DBUS, "Successfully handled '%s' message\n", dbus_message_get_member (message));

  return DBUS_HANDLER_RESULT_HANDLED;

 oom:
  g_error (_("Out of memory handling '%s' message"), dbus_message_get_member (message));
  return DBUS_HANDLER_RESULT_NEED_MEMORY;
}

static DBusHandlerResult
vino_dbus_listener_handle_get_server_port (VinoDBusListener *listener,
                                           DBusConnection   *connection,
                                           DBusMessage      *message)
{
  DBusMessage *reply;
  gint        port;

  if (!(reply = dbus_message_new_method_return (message)))
    goto oom;

  port = vino_server_get_external_port (listener->priv->server);

  if (!dbus_message_append_args (reply, DBUS_TYPE_INT32, &port, DBUS_TYPE_INVALID))
    goto oom;

  if (!dbus_connection_send (connection, reply, NULL))
    goto oom;
    
  dbus_message_unref (reply);

  dprintf (DBUS, "Successfully handled '%s' message\n", dbus_message_get_member (message));

  return DBUS_HANDLER_RESULT_HANDLED;

 oom:
  g_error (_("Out of memory handling '%s' message"), dbus_message_get_member (message));
  return DBUS_HANDLER_RESULT_NEED_MEMORY;
}

static DBusHandlerResult
vino_dbus_listener_handle_get_internal_data (VinoDBusListener *listener,
                                             DBusConnection   *connection,
                                             DBusMessage      *message)
{
  DBusMessage *reply;
  gint        port;
  char        *host = NULL;
  const char  *avahi_host;

  if (!(reply = dbus_message_new_method_return (message)))
    goto oom;

#ifdef VINO_ENABLE_HTTP_SERVER
  port = vino_get_http_server_port (listener->priv->server);
#else
  port = vino_server_get_port (listener->priv->server);
#endif

  host = get_local_hostname (listener);
  avahi_host = vino_mdns_get_hostname ();
  if (!dbus_message_append_args (reply,
                                 DBUS_TYPE_STRING, &host,
                                 DBUS_TYPE_STRING, &avahi_host,
                                 DBUS_TYPE_INT32, &port,
                                 DBUS_TYPE_INVALID))
    goto oom;

  if (!dbus_connection_send (connection, reply, NULL))
    goto oom;
    
  dbus_message_unref (reply);

  dprintf (DBUS, "Successfully handled '%s' message\n", dbus_message_get_member (message));

  return DBUS_HANDLER_RESULT_HANDLED;

 oom:

  g_free (host);
  g_error (_("Out of memory handling '%s' message"), dbus_message_get_member (message));
  return DBUS_HANDLER_RESULT_NEED_MEMORY;
}

static DBusHandlerResult
vino_dbus_listener_message_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
  VinoDBusListener *listener = VINO_DBUS_LISTENER (user_data);

  dprintf (DBUS, "D-Bus message: obj_path = '%s' interface = '%s' method = '%s' destination = '%s'\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));

  if (dbus_message_is_method_call (message,
                                   VINO_DBUS_INTERFACE,
                                   "GetInternalData"))
    {
      return vino_dbus_listener_handle_get_internal_data (listener,
                                                          connection,
                                                          message);
    }
  if (dbus_message_is_method_call (message,
                                   VINO_DBUS_INTERFACE,
                                   "GetExternalPort"))
    {
      return vino_dbus_listener_handle_get_server_port (listener,
                                                        connection,
                                                        message);
    }
  else if (dbus_message_is_method_call (message,
                                        "org.freedesktop.DBus.Introspectable",
                                        "Introspect"))
    {
      return vino_dbus_listener_handle_introspect (listener,
                                                   connection,
                                                   message);
    }
  else
    {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
}

static void
vino_dbus_listener_info_changed (VinoServer *server, GParamSpec *property, VinoDBusListener *listener)
{
  DBusMessage *message;
  gchar *obj_path;

  dprintf (DBUS, "Emitting ServerInfoChanged signal\n");

  obj_path = g_strdup_printf ("/org/gnome/vino/screens/%d",
                              gdk_screen_get_number (vino_server_get_screen (server)));

  message = dbus_message_new_signal (obj_path,
                                     VINO_DBUS_INTERFACE,
                                     "ServerInfoChanged");
  g_free (obj_path);

  if (!message)
    {
      g_warning ("Error creating signal\n");
      return;
    }

  if (!dbus_connection_send (vino_dbus_get_connection (), message, NULL))
    g_warning ("Error sending signal\n");

  dbus_message_unref (message);
}

static DBusObjectPathVTable vino_dbus_listener_vtable =
{
  NULL,                                  /* unregister_function */
  &vino_dbus_listener_message_handler    /* message_function */
};

static void
vino_dbus_listener_set_server (VinoDBusListener *listener,
                               VinoServer       *server)
{
  DBusConnection *connection;
  GdkScreen      *screen;
  char           *obj_path;

  g_assert (listener->priv->server == NULL);

  listener->priv->server = server;

  if (!(connection = vino_dbus_get_connection ()))
    return;

  screen = vino_server_get_screen (listener->priv->server);

  obj_path = g_strdup_printf ("/org/gnome/vino/screens/%d",
                              gdk_screen_get_number (screen));

  if (!dbus_connection_register_object_path (connection,
                                             obj_path,
                                             &vino_dbus_listener_vtable,
                                             listener))
    {
      g_error (_("Out of memory registering object path '%s'"), obj_path);
      g_free (obj_path);
      return;
    }

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

static DBusConnection *vino_dbus_connection = NULL;
static gboolean        vino_dbus_failed_to_connect = FALSE;

DBusConnection *
vino_dbus_get_connection (void)
{
  if (vino_dbus_connection == NULL && !vino_dbus_failed_to_connect)
    {
      DBusError error;

      dbus_error_init (&error);

      if ((vino_dbus_connection = dbus_bus_get (DBUS_BUS_SESSION, &error)))
        {
          dprintf (DBUS, "Successfully connected to the message bus\n");

          dbus_connection_setup_with_g_main (vino_dbus_connection, NULL);
          dbus_connection_set_exit_on_disconnect (vino_dbus_connection, FALSE);
        }
      else
        {
          vino_dbus_failed_to_connect = TRUE;
          g_printerr (_("Failed to open connection to bus: %s\n"),
                      error.message);
          dbus_error_free (&error);
        }
    }

  return vino_dbus_connection;
}

void
vino_dbus_unref_connection (void)
{
  if (vino_dbus_connection != NULL)
    dbus_connection_unref (vino_dbus_connection);
  vino_dbus_connection = NULL;
}

gboolean
vino_dbus_request_name (void)
{

  DBusConnection *connection;
  DBusError       error;
  int             result;

  if (!(connection = vino_dbus_get_connection ()))
    return FALSE;

  dbus_error_init (&error);

  result = dbus_bus_request_name (connection,
				  VINO_DBUS_BUS_NAME,
				  DBUS_NAME_FLAG_DO_NOT_QUEUE,
				  &error);
  if (dbus_error_is_set (&error))
    {
      g_printerr (_("Failed to acquire D-Bus name '%s'\n"),
                  error.message);
      dbus_error_free (&error);
      return FALSE;
    }

  if (result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    {
      g_warning (_("Remote Desktop server already running; exiting ...\n"));
      return FALSE;
    }

  dprintf (DBUS, "Successfully acquired D-Bus name '%s'\n", VINO_DBUS_BUS_NAME);
  return TRUE;
}
