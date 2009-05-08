/*
 * © 2009, Collabora Ltd
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
 *      Arnaud Maillet <arnaud.maillet@collabora.co.uk>
 */

#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>


#include "vino-tube-server.h"
#include "vino-dbus-error.h"

G_DEFINE_TYPE (VinoTubeServer, vino_tube_server, VINO_TYPE_SERVER);

#define VINO_TUBE_SERVER_GET_PRIVATE(obj)\
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VINO_TYPE_TUBE_SERVER,\
    VinoTubeServerPrivate))

struct _VinoTubeServerPrivate
{
  TpChannel *tp_channel;
  gchar *connection_path;
  gchar *tube_path;
  GHashTable *channel_properties;
};

enum
{
  DISCONNECTED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CONNECTION_PATH,
  PROP_TUBE_PATH,
  PROP_CHANNEL_PROPERTIES
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
vino_tube_server_dispose (GObject *object)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);

  if (server->priv->tp_channel != NULL)
    {
      g_object_unref (server->priv->tp_channel);
      server->priv->tp_channel = NULL;
    }

  if (G_OBJECT_CLASS (vino_tube_server_parent_class)->dispose)
    G_OBJECT_CLASS (vino_tube_server_parent_class)->dispose (object);
}

static void
vino_tube_server_finalize (GObject *object)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);

  if (server->priv->connection_path != NULL)
    {
      g_free (server->priv->connection_path);
      server->priv->connection_path = NULL;
    }

  if (server->priv->tube_path != NULL)
    {
      g_free (server->priv->tube_path);
      server->priv->tube_path = NULL;
    }

  if (server->priv->channel_properties != NULL)
    {
      g_hash_table_destroy (server->priv->channel_properties);
      server->priv->channel_properties = NULL;
    }

  g_debug ("-- Destruction of a VinoTubeServer --\n");

  if (G_OBJECT_CLASS (vino_tube_server_parent_class)->finalize)
    G_OBJECT_CLASS (vino_tube_server_parent_class)->finalize (object);
}

static void
vino_tube_server_set_connection_path (VinoTubeServer *server,
    const gchar    *connection_path)
{
  g_return_if_fail (VINO_IS_TUBE_SERVER (server));

  server->priv->connection_path = g_strdup (connection_path);
}

static void
vino_tube_server_set_tube_path (VinoTubeServer *server,
    const gchar *tube_path)
{
  g_return_if_fail (VINO_IS_TUBE_SERVER (server));

  server->priv->tube_path = g_strdup (tube_path);
}

static void
vino_tube_server_set_channel_properties (VinoTubeServer *server,
    GHashTable *channel_properties)
{
  g_return_if_fail (VINO_IS_TUBE_SERVER (server));

  server->priv->channel_properties = g_boxed_copy
      (TP_HASH_TYPE_STRING_VARIANT_MAP, channel_properties);
}

static void
vino_tube_server_set_property (GObject *object,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);

  switch (prop_id)
    {
    case PROP_CONNECTION_PATH:
      vino_tube_server_set_connection_path (server,
          g_value_get_string (value));
      break;
    case PROP_TUBE_PATH:
      vino_tube_server_set_tube_path (server, g_value_get_string (value));
      break;
    case PROP_CHANNEL_PROPERTIES:
      vino_tube_server_set_channel_properties (server,
          g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_tube_server_get_property (GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);

  switch (prop_id)
    {
    case PROP_CONNECTION_PATH:
      g_value_set_string (value, server->priv->connection_path);
      break;
    case PROP_TUBE_PATH:
      g_value_set_string (value, server->priv->tube_path);
      break;
    case PROP_CHANNEL_PROPERTIES:
      g_value_set_boxed (value, server->priv->channel_properties);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_tube_server_init (VinoTubeServer *self)
{
  self->priv = VINO_TUBE_SERVER_GET_PRIVATE (self);
  self->priv->tp_channel = NULL;
  self->priv->channel_properties = NULL;
}

static void
vino_tube_server_class_init (VinoTubeServerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = vino_tube_server_dispose;
  gobject_class->finalize = vino_tube_server_finalize;
  gobject_class->set_property = vino_tube_server_set_property;
  gobject_class->get_property = vino_tube_server_get_property;

  signals[DISCONNECTED] =
      g_signal_new ("disconnected",
      G_OBJECT_CLASS_TYPE (gobject_class),
      G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET (VinoTubeServerClass, disconnected),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE,
      0);

  g_object_class_install_property (gobject_class,
      PROP_CONNECTION_PATH,
      g_param_spec_string ("connection-path",
      "Connection path",
      "Connection path of the stream tube",
      NULL,
      G_PARAM_READWRITE   |
      G_PARAM_CONSTRUCT   |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TUBE_PATH,
      g_param_spec_string ("tube-path",
      "Tube path",
      "Tube path of the stream tube",
      NULL,
      G_PARAM_READWRITE   |
      G_PARAM_CONSTRUCT   |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CHANNEL_PROPERTIES,
      g_param_spec_boxed ("channel-properties",
      "Channel properties",
      "Channel properties of the stream tube",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READWRITE   |
      G_PARAM_CONSTRUCT   |
      G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (VinoTubeServerPrivate));
}

static void
vino_tube_server_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer server)
{
  VinoTubeServer *self = VINO_TUBE_SERVER (server);
  g_debug ("Tube is closed\n");
  g_signal_emit (G_OBJECT (self), signals[DISCONNECTED], 0);
}

static void
vino_tube_server_offer_cb (TpChannel *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  if (error != NULL)
    {
      g_printerr ("Impossible to offer the stream tube: %s\n",
          error->message);
      return;
    }
}

static void
vino_tube_server_channel_ready (TpChannel *channel,
    const GError *error,
    gpointer server)
{
  TpConnection *tp_connection;
  GHashTable *parameters;
  GValue address = {0,};
  gint port;

  parameters = g_hash_table_new (g_str_hash, g_str_equal);

  if (error != NULL)
    {
      g_printerr ("Impossible to create the channel: %s\n",
          error->message);
      return;
    }

  g_object_get (channel, "connection", &tp_connection, NULL);

  port = vino_server_get_port (VINO_SERVER (server));

  g_debug ("-- Creation of a VinoTubeServer!! port : %d --\n", port);

  g_signal_connect (G_OBJECT (channel), "invalidated",
      G_CALLBACK (vino_tube_server_invalidated_cb), server);

  g_value_init (&address, TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4);
  g_value_take_boxed (&address, dbus_g_type_specialized_construct
      (TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4));
  dbus_g_type_struct_set (&address, 0, "127.0.0.1", 1, port, G_MAXUINT);

  tp_cli_channel_type_stream_tube_call_offer (channel,
      -1, TP_SOCKET_ADDRESS_TYPE_IPV4, &address,
     TP_SOCKET_ACCESS_CONTROL_LOCALHOST, parameters,
     vino_tube_server_offer_cb, server, NULL, NULL);

  g_value_unset (&address);
  g_hash_table_destroy (parameters);
  g_object_unref (tp_connection);
}

static void
vino_tube_server_connection_ready (TpConnection *connection,
    const GError *error,
    gpointer object)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);
  GError *error_failed = NULL;

  if (connection == NULL)
    {
      g_printerr ("The connection is not ready: %s\n", error->message);
      return ;
    }

  server->priv->tp_channel = tp_channel_new_from_properties (connection,
      server->priv->tube_path, server->priv->channel_properties,
      &error_failed);

  if (server->priv->tp_channel == NULL)
    {
      g_printerr ("Error requesting tp channel: %s\n",
          error_failed->message);
      g_clear_error (&error_failed);
      return ;
    }

  tp_channel_call_when_ready (server->priv->tp_channel,
      vino_tube_server_channel_ready, server);
}

gboolean
vino_tube_server_share_with_tube (VinoTubeServer *server,
    GError **error)
{
  TpDBusDaemon *tp_dbus_daemon;
  TpConnection *tp_connection;
  GError *error_failed = NULL;

  tp_dbus_daemon = tp_dbus_daemon_dup (&error_failed);

  if (tp_dbus_daemon == NULL)
    {
      g_printerr ("Error requesting dbus daemon: %s\n",
          error_failed->message);
      g_clear_error (&error_failed);
      g_set_error (error, vino_dbus_error_quark (),
          VINO_DBUS_ERROR_FAILED,
          "Error requesting dbus daemon");
      return FALSE;
    }

  tp_connection = tp_connection_new (tp_dbus_daemon, NULL,
      server->priv->connection_path, &error_failed);

  if (tp_connection == NULL)
    {
      g_printerr ("Error requesting tp connection: %s\n",
          error_failed->message);
      g_clear_error (&error_failed);
      g_set_error (error, vino_dbus_error_quark (),
          VINO_DBUS_ERROR_FAILED,
          "Error requesting tp connection");
      return FALSE;
    }

  tp_connection_call_when_ready (tp_connection,
      vino_tube_server_connection_ready, server);

  g_object_unref (tp_connection);
  g_object_unref (tp_dbus_daemon);

  return TRUE;
}