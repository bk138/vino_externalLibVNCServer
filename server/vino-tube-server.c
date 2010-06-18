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

#include <glib/gi18n.h>

#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/contact.h>


#include "vino-tube-server.h"
#include "vino-dbus-error.h"
#include "vino-util.h"

G_DEFINE_TYPE (VinoTubeServer, vino_tube_server, VINO_TYPE_SERVER);

#define VINO_TUBE_SERVER_GET_PRIVATE(obj)\
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VINO_TYPE_TUBE_SERVER,\
    VinoTubeServerPrivate))

struct _VinoTubeServerPrivate
{
  TpChannel *tp_channel;
  gchar *alias;
  TpConnection *connection;
  gchar *filename;
  gulong signal_invalidated_id;
  VinoStatusTubeIcon *icon_tube;
  TpTubeChannelState state;
};

enum
{
  DISCONNECTED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_TUBE
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
vino_tube_server_dispose (GObject *object)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);

  g_signal_handler_disconnect (G_OBJECT (server->priv->tp_channel),
      server->priv->signal_invalidated_id);

  if (server->priv->tp_channel != NULL)
    {
      g_object_unref (server->priv->tp_channel);
      server->priv->tp_channel = NULL;
    }

  if (server->priv->icon_tube != NULL)
    {
      g_object_unref (server->priv->icon_tube);
      server->priv->icon_tube = NULL;
    }

  if (G_OBJECT_CLASS (vino_tube_server_parent_class)->dispose)
    G_OBJECT_CLASS (vino_tube_server_parent_class)->dispose (object);
}

static void
vino_tube_server_finalize (GObject *object)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);

  if (server->priv->alias != NULL)
    {
      g_free (server->priv->alias);
      server->priv->alias = NULL;
    }

  if (server->priv->filename != NULL)
    {
      g_free (server->priv->filename);
      server->priv->filename = NULL;
    }

  if (server->priv->connection != NULL)
    {
      g_object_unref (server->priv->connection);
      server->priv->connection = NULL;
    }

  dprintf (TUBE, "Destruction of a VinoTubeServer\n");

  if (G_OBJECT_CLASS (vino_tube_server_parent_class)->finalize)
    G_OBJECT_CLASS (vino_tube_server_parent_class)->finalize (object);
}

static void
vino_tube_server_set_connection (VinoTubeServer *server,
    TpConnection *connection)
{
  g_return_if_fail (VINO_IS_TUBE_SERVER (server));

  server->priv->connection = g_object_ref (connection);
}

static void
vino_tube_server_set_tube (VinoTubeServer *server,
    TpChannel *tube)
{
  g_return_if_fail (VINO_IS_TUBE_SERVER (server));

  server->priv->tp_channel = g_object_ref (tube);
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
    case PROP_CONNECTION:
      vino_tube_server_set_connection (server,
          g_value_get_object (value));
      break;
    case PROP_TUBE:
      vino_tube_server_set_tube (server, g_value_get_object (value));
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
    case PROP_CONNECTION:
      g_value_set_object (value, server->priv->connection);
      break;
    case PROP_TUBE:
      g_value_set_object (value, server->priv->tp_channel);
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
  self->priv->icon_tube = NULL;
  self->priv->state = TP_TUBE_CHANNEL_STATE_NOT_OFFERED;
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
      PROP_CONNECTION,
      g_param_spec_object ("connection",
      "TpConnection",
      "Connection of the stream tube",
      TP_TYPE_CONNECTION,
      G_PARAM_READWRITE   |
      G_PARAM_CONSTRUCT   |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TUBE,
      g_param_spec_object ("tube",
      "TpChannel",
      "Channel of the stream tube",
      TP_TYPE_CHANNEL,
      G_PARAM_READWRITE   |
      G_PARAM_CONSTRUCT   |
      G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (VinoTubeServerPrivate));
}

void
vino_tube_server_fire_closed (VinoTubeServer *server)
{
  VinoTubeServer *self = VINO_TUBE_SERVER (server);

  dprintf (TUBE, "Tube is closed\n");
  g_signal_emit (G_OBJECT (self), signals[DISCONNECTED], 0);
}

void
vino_tube_server_close_tube (VinoTubeServer *server)
{
  VinoTubeServer *self = VINO_TUBE_SERVER (server);

  tp_cli_channel_call_close (self->priv->tp_channel, -1,
          NULL, NULL, NULL, NULL);

  vino_tube_server_fire_closed (self);
}

static void
vino_tube_server_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer server)
{
  VinoTubeServer *self = VINO_TUBE_SERVER (server);
  const gchar *summary;
  gchar *body;

  summary = _("Share my desktop information");

  if (self->priv->state == TP_TUBE_CHANNEL_STATE_REMOTE_PENDING)
      body = g_strdup_printf
          (_("'%s' rejected the desktop sharing invitation."),
          vino_tube_server_get_alias (self));
  else
      body = g_strdup_printf
          (_("'%s' disconnected"),
          vino_tube_server_get_alias (self));

  vino_status_tube_icon_show_notif (self->priv->icon_tube, summary,
      (const gchar *)body, TRUE);

  g_free (body);

  self->priv->state = TP_TUBE_CHANNEL_STATE_NOT_OFFERED;
}

static void
vino_tube_server_state_changed (TpChannel *channel,
    guint state,
    gpointer object,
    GObject *weak_object)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (object);
  const gchar *summary;
  gchar *body;

  summary = _("Share my desktop information");

  switch (state)
    {
      case TP_TUBE_CHANNEL_STATE_OPEN:
        body = g_strdup_printf
            (_("'%s' is remotely controlling your desktop."),
            vino_tube_server_get_alias (server));
        vino_status_tube_icon_show_notif (server->priv->icon_tube, summary,
            (const gchar*) body, FALSE);
        g_free (body);
        server->priv->state = TP_TUBE_STATE_OPEN;
        break;
      case TP_TUBE_CHANNEL_STATE_REMOTE_PENDING:
        body =  g_strdup_printf
            (_("Waiting for '%s' to connect to the screen."),
            vino_tube_server_get_alias (server));
        vino_status_tube_icon_show_notif (server->priv->icon_tube, summary,
            (const gchar*) body, FALSE);
        g_free (body);
        server->priv->state = TP_TUBE_STATE_REMOTE_PENDING;
        break;
    }
}

static void
vino_tube_server_offer_cb (TpChannel *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  if (error != NULL)
    {
      dprintf (TUBE, "Impossible to offer the stream tube: %s\n", error->message);
      return;
    }
}

static gchar *
vino_tube_server_contact_get_avatar_filename (TpContact *contact,
    const gchar *token,
    VinoTubeServer *self)
{
  gchar *avatar_path;
  gchar *avatar_file;
  gchar *token_escaped;
  TpConnection *connection;
  gchar *cm;
  gchar *protocol;

  if (contact == NULL)
    return NULL;

  token_escaped = tp_escape_as_identifier (token);

  connection = tp_contact_get_connection (contact);

  if (!tp_connection_parse_object_path (connection, &protocol, &cm))
    {
      dprintf (TUBE, "Impossible to parse object path\n");
      return NULL;
    }

  avatar_path = g_build_filename (g_get_user_cache_dir (),
      "telepathy",
      "avatars",
      cm,
      protocol,
      NULL);
  g_mkdir_with_parents (avatar_path, 0700);

  avatar_file = g_build_filename (avatar_path, token_escaped, NULL);

  g_free (token_escaped);
  g_free (avatar_path);
  g_free (cm);
  g_free (protocol);

  return avatar_file;
}

static void
vino_tube_server_factory_handle_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer self,
    GObject *weak_object)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (self);
  TpContact *contact;
  const gchar *token;

  if (error != NULL)
    {
      dprintf (TUBE, "Impossible to get the contact name: %s\n", error->message);
      return;
    }

  contact = contacts[0];
  server->priv->alias = g_strdup (tp_contact_get_alias (contact));
  token = tp_contact_get_avatar_token (contact);

  if (!tp_strdiff (token, ""))
    {
      server->priv->filename = NULL;
    }
  else
    {
      server->priv->filename = vino_tube_server_contact_get_avatar_filename
          (contact, token, self);
    }
}

gboolean
vino_tube_server_share_with_tube (VinoTubeServer *server,
    GError **error)
{
  TpHandle handle;
  TpContactFeature features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN };
  GHashTable *parameters;
  GValue address = {0,};
  gint port;
  GdkScreen *screen;

  parameters = g_hash_table_new (g_str_hash, g_str_equal);

  screen = gdk_screen_get_default ();
  server->priv->icon_tube = vino_status_tube_icon_new (server,
      screen);

  vino_status_tube_icon_set_visibility (server->priv->icon_tube,
      VINO_STATUS_TUBE_ICON_VISIBILITY_ALWAYS);

  tp_cli_channel_interface_tube_connect_to_tube_channel_state_changed
      (server->priv->tp_channel, vino_tube_server_state_changed, server,
       NULL, NULL, error);

  if (error != NULL)
    {
      dprintf (TUBE, "Failed to connect state channel\n");
      return FALSE;
    }

  handle = tp_channel_get_handle (server->priv->tp_channel, NULL);

  tp_connection_get_contacts_by_handle (server->priv->connection, 1, &handle,
      G_N_ELEMENTS(features), features, vino_tube_server_factory_handle_cb,
      server, NULL, NULL);

  port = vino_server_get_port (VINO_SERVER (server));

  dprintf (TUBE, "Creation of a VinoTubeServer, port : %d\n", port);

  server->priv->signal_invalidated_id = g_signal_connect (
      server->priv->tp_channel, "invalidated",
      G_CALLBACK (vino_tube_server_invalidated_cb), server);

  g_value_init (&address, TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4);
  g_value_take_boxed (&address, dbus_g_type_specialized_construct
      (TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4));
  dbus_g_type_struct_set (&address, 0, "127.0.0.1", 1, port, G_MAXUINT);

  tp_cli_channel_type_stream_tube_call_offer (server->priv->tp_channel,
      -1, TP_SOCKET_ADDRESS_TYPE_IPV4, &address,
     TP_SOCKET_ACCESS_CONTROL_LOCALHOST, parameters,
     vino_tube_server_offer_cb, server, NULL, NULL);

  g_value_unset (&address);
  g_hash_table_destroy (parameters);

  return TRUE;
}

const gchar*
vino_tube_server_get_alias (VinoTubeServer *self)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (self);
  return (const gchar*)server->priv->alias;
}

const gchar*
vino_tube_server_get_avatar_filename (VinoTubeServer *self)
{
  VinoTubeServer *server = VINO_TUBE_SERVER (self);
  return (const gchar*)server->priv->filename;
}
