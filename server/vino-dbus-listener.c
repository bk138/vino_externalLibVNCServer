/*
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2006 Jonh Wendell <wendell@bani.com.br>
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
 *
 * Code taken from gnome-screensaver/src/gs-listener-dbus.c
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define  DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "vino-dbus-listener.h"
#include "vino-util.h"

#define VINO_DBUS_BUS_NAME  "org.gnome.Vino"
#define VINO_DBUS_INTERFACE "org.gnome.Vino"
#define VINO_DBUS_OBJ_PATH  "/org/gnome/vino/screens/%d"

static DBusHandlerResult vino_dbus_listener_message_handler (DBusConnection  *connection,
                                                             DBusMessage     *message,
                                                             void            *user_data);

#define VINO_DBUS_LISTENER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), VINO_TYPE_DBUS_LISTENER, VinoDBusListenerPrivate))

G_DEFINE_TYPE (VinoDBusListener, vino_dbus_listener, G_TYPE_OBJECT)

struct _VinoDBusListenerPrivate
{
  DBusConnection *connection;

  VinoServer     *server;
};

enum
{
  PROP_0,
  PROP_SERVER
};

static DBusObjectPathVTable
vino_dbus_listener_vtable = { NULL,
                              &vino_dbus_listener_message_handler,
                              NULL,
                              NULL,
                              NULL,
                              NULL };

static void
vino_dbus_listener_set_server (VinoDBusListener *listener,
                               VinoServer       *server)
{
  gboolean  acquired;
  DBusError buserror;
  gboolean  is_connected;
  GdkScreen       *screen;
  int              screen_num;

  g_return_if_fail (listener != NULL);

  listener->priv->server = server;

  if (! listener->priv->connection)
    g_error (_("failed to register with the message bus"));

  is_connected = dbus_connection_get_is_connected (listener->priv->connection);
  if (! is_connected)
    g_error (_("not connected to the message bus"));

  dbus_error_init (&buserror);

  screen     = vino_server_get_screen (listener->priv->server);
  screen_num = gdk_screen_get_number  (screen);

  if (! dbus_connection_register_object_path (listener->priv->connection,
                                              g_strdup_printf (VINO_DBUS_OBJ_PATH, screen_num),
                                              &vino_dbus_listener_vtable,
                                              listener))
    g_error (_("out of memory registering object path"));

  dprintf(DBUS, "Object registered in path " VINO_DBUS_OBJ_PATH "\n", screen_num);

  acquired = dbus_bus_request_name (listener->priv->connection,
                                    VINO_DBUS_BUS_NAME,
                                    0, &buserror) != -1;
  if (dbus_error_is_set (&buserror))
    g_error (buserror.message);

  dbus_error_free (&buserror);

  if ( !acquired)
    g_warning(_("Error in dbus"));
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
vino_dbus_listener_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
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
vino_dbus_listener_class_init (VinoDBusListenerClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = vino_dbus_listener_get_property;
  object_class->set_property = vino_dbus_listener_set_property;


  g_object_class_install_property (object_class,
				   PROP_SERVER,
				   g_param_spec_object ("server",
							"Server",
							"The server",
							VINO_TYPE_SERVER,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_NAME |
							G_PARAM_STATIC_NICK |
							G_PARAM_STATIC_BLURB));

  g_type_class_add_private (klass, sizeof (VinoDBusListenerPrivate));
}

static DBusHandlerResult
do_introspect (DBusConnection *connection,
               DBusMessage    *message,
               dbus_bool_t     local_interface)
{
  DBusMessage *reply;
  GString     *xml;
  char        *xml_string;

  /* standard header */
  xml = g_string_new ("<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
                      "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
                      "<node>\n"
                      "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                      "    <method name=\"Introspect\">\n"
                      "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
                      "    </method>\n"
                      "  </interface>\n");

  /* Vino interface */
  xml = g_string_append (xml,
                        "  <interface name=\"org.gnome.Vino\">\n"
                        "    <method name=\"GetServerPort\">\n"
                        "      <arg name=\"port\" direction=\"out\" type=\"u\"/>\n"
                        "    </method>\n"
                        "  </interface>\n");

  reply = dbus_message_new_method_return (message);

  xml = g_string_append (xml, "</node>\n");
  xml_string = g_string_free (xml, FALSE);

  dbus_message_append_args (reply,
                            DBUS_TYPE_STRING, &xml_string,
                            DBUS_TYPE_INVALID);

  g_free (xml_string);

  if (reply == NULL)
    g_error ("No memory");

  if (! dbus_connection_send (connection, reply, NULL))
    g_error ("No memory");

  dbus_message_unref (reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_get_server_port (VinoDBusListener  *listener,
                          DBusConnection    *connection,
                          DBusMessage       *message)
{
  DBusMessageIter  iter;
  DBusMessage     *reply;
  dbus_int32_t     port;

  reply = dbus_message_new_method_return (message);

  if (reply == NULL)
    g_error ("No memory");


  dbus_message_iter_init_append (reply, &iter);

  port = vino_server_get_port(listener->priv->server);

  dbus_message_iter_append_basic (&iter, DBUS_TYPE_INT32, &port);

  if (! dbus_connection_send (connection, reply, NULL))
                g_error ("No memory");
    
  dbus_message_unref (reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_dbus_handle_session_message (DBusConnection *connection,
                                      DBusMessage    *message,
                                      void           *user_data,
                                      dbus_bool_t     local_interface)
{
  VinoDBusListener *listener = VINO_DBUS_LISTENER (user_data);


  g_return_val_if_fail (connection != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
  g_return_val_if_fail (message != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

  if (dbus_message_is_method_call (message, VINO_DBUS_BUS_NAME, "GetServerPort"))
    {
      return listener_get_server_port (listener, connection, message);
    }

  if (dbus_message_is_method_call (message, "org.freedesktop.DBus.Introspectable", "Introspect"))
    {
      return do_introspect (connection, message, local_interface);
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static gboolean
vino_dbus_listener_dbus_init (VinoDBusListener *listener)
{
  DBusError error;

  dbus_error_init (&error);

  if (listener->priv->connection == NULL)
    {
      listener->priv->connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
      if (listener->priv->connection == NULL)
        {
          if (dbus_error_is_set (&error))
            {
              dprintf (DBUS, "couldn't connect to session bus: %s", error.message);
              dbus_error_free (&error);
            }
          return FALSE;
        }

      dbus_connection_setup_with_g_main (listener->priv->connection, NULL);
      dbus_connection_set_exit_on_disconnect (listener->priv->connection, FALSE);
    }

  return TRUE;
}

static void
vino_dbus_listener_init (VinoDBusListener *listener)
{
  listener->priv = VINO_DBUS_LISTENER_GET_PRIVATE (listener);

  vino_dbus_listener_dbus_init (listener);
}

VinoDBusListener *
vino_dbus_listener_new (VinoServer *server)
{
  g_return_val_if_fail (VINO_IS_SERVER (server), NULL);

  return g_object_new (VINO_TYPE_DBUS_LISTENER,
                       "server", server,
                       NULL);
}

VinoServer *
vino_dbus_listener_get_server (VinoDBusListener *listener)
{
  g_return_val_if_fail (VINO_IS_DBUS_LISTENER (listener), NULL);

  return listener->priv->server;
}

static DBusHandlerResult
vino_dbus_listener_message_handler (DBusConnection *connection,
                                    DBusMessage    *message,
                                    void           *user_data)
{
  g_return_val_if_fail (connection != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
  g_return_val_if_fail (message != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

  dprintf (DBUS, "obj_path=%s interface=%s method=%s destination=%s\n",
           dbus_message_get_path (message),
           dbus_message_get_interface (message),
           dbus_message_get_member (message),
           dbus_message_get_destination (message));

  if (dbus_message_is_method_call (message, "org.freedesktop.DBus", "AddMatch"))
    {
      DBusMessage *reply;

      reply = dbus_message_new_method_return (message);

      if (reply == NULL)
        g_error ("No memory");

      if (! dbus_connection_send (connection, reply, NULL))
        g_error ("No memory");

      dbus_message_unref (reply);

      return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
                strcmp (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0) {
        dbus_connection_unref (connection);

        return DBUS_HANDLER_RESULT_HANDLED;
    } else
        return listener_dbus_handle_session_message (connection, message, user_data, TRUE);

}
