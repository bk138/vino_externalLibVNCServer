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

#include <glib-object.h>
#include <gdk/gdk.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/telepathy-glib.h>

#include "vino-tube-servers-manager.h"
#include "vino-server.h"
#include "vino-tube-server.h"
#include "vino-dbus-error.h"
#include "vino-status-tube-icon.h"
#include "vino-util.h"

G_DEFINE_TYPE (VinoTubeServersManager, vino_tube_servers_manager,
    G_TYPE_OBJECT);

#define VINO_TUBE_SERVERS_MANAGER_GET_PRIVATE(obj)\
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VINO_TYPE_TUBE_SERVERS_MANAGER,\
    VinoTubeServersManagerPrivate))

static void handle_channels_cb (TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    GList *requests_satisfied,
    gint64 user_action_time,
    TpHandleChannelsContext *context,
    gpointer user_data);

struct _VinoTubeServersManagerPrivate
{
  GSList *vino_tube_servers;
  guint alternative_port;

  TpBaseClient *handler;
};

static void
vino_tube_servers_manager_dispose (GObject *object)
{
  VinoTubeServersManager *self = VINO_TUBE_SERVERS_MANAGER (object);
  GSList *l;

  for (l = self->priv->vino_tube_servers; l; l = l->next)
    g_object_unref (l->data);

  g_slist_free (self->priv->vino_tube_servers);
  self->priv->vino_tube_servers = NULL;

  dprintf (TUBE, "Destruction of the VinoTubeServersManager\n");

  if (G_OBJECT_CLASS (vino_tube_servers_manager_parent_class)->dispose)
    G_OBJECT_CLASS (vino_tube_servers_manager_parent_class)->dispose (object);
}

static void
vino_tube_servers_manager_class_init (VinoTubeServersManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  dprintf (TUBE, "Creation of the VinoTubeServersManager\n");

  gobject_class->dispose = vino_tube_servers_manager_dispose;

  g_type_class_add_private (klass, sizeof (VinoTubeServersManagerPrivate));
}

static void
vino_tube_servers_manager_init (VinoTubeServersManager *self)
{
  TpDBusDaemon *dbus;
  GError *error = NULL;

  self->priv = VINO_TUBE_SERVERS_MANAGER_GET_PRIVATE (self);
  self->priv->vino_tube_servers = NULL;
  self->priv->alternative_port = 26570;

  dbus = tp_dbus_daemon_dup (NULL);

  self->priv->handler = tp_simple_handler_new (dbus, FALSE, FALSE, "Vino",
      FALSE, handle_channels_cb, self, NULL);

  g_object_unref (dbus);

  tp_base_client_take_handler_filter (self->priv->handler, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN,
          TRUE,
        TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE, G_TYPE_STRING,
          "rfb",
        NULL));

  if (!tp_base_client_register (self->priv->handler, &error))
    {
      dprintf (TUBE, "Failed to register Handler: %s\n", error->message);
      g_error_free (error);
    }
}

static void
vino_tube_servers_manager_disconnected_cb (VinoTubeServer *server,
    gpointer object)
{
  VinoTubeServersManager *self = VINO_TUBE_SERVERS_MANAGER (object);
  self->priv->vino_tube_servers = g_slist_remove
      (self->priv->vino_tube_servers, server);
  g_object_unref (server);
}

VinoTubeServersManager *
vino_tube_servers_manager_new (void)
{
  return g_object_new (VINO_TYPE_TUBE_SERVERS_MANAGER, NULL);
}

static void
handle_channels_cb (TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    GList *requests_satisfied,
    gint64 user_action_time,
    TpHandleChannelsContext *context,
    gpointer user_data)
{
  VinoTubeServersManager *self = user_data;
  VinoTubeServer *server;
  GdkDisplay *display;
  GdkScreen *screen;
  /* the server is listenning only on lo as only the tube is supposed to
  connect to it */
  gchar * network_interface = "lo";
  GList *l;
  TpChannel *channel = NULL;

  for (l = channels; l != NULL; l = g_list_next (l))
    {
      TpChannel *chan = l->data;
      const gchar *service;

      if (tp_channel_get_channel_type_id (chan) !=
          TP_IFACE_QUARK_CHANNEL_TYPE_STREAM_TUBE)
        continue;

      if (tp_proxy_get_invalidated (chan) != NULL)
        continue;

      service = tp_asv_get_string (
          tp_channel_borrow_immutable_properties (chan),
          TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE);

      if (tp_strdiff (service, "rfb"))
        continue;

      channel = chan;
      break;
    }

  if (channel == NULL)
    {
      /* No stream tube channel ?! */
      GError error = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "No stream tube channel" };

      tp_handle_channels_context_fail (context, &error);
      return;
    }

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);

  server = g_object_new (VINO_TYPE_TUBE_SERVER,
      "display-status-icon",  0,
      "use-dbus-listener",    0,
      "prompt-enabled",       0,
      "view-only",            0,
      "network-interface",    network_interface,
      "use-alternative-port", 1,
      "alternative-port",     self->priv->alternative_port,
      "auth-methods",         1,
      "require-encryption",   0,
      "vnc-password",         NULL,
      "on-hold",              0,
      "screen",               screen,
      "lock-screen",          0,
      "disable-background",   0,
      "use-upnp",             0,
      "connection",           connection,
      "tube-path",            tp_proxy_get_object_path (channel),
      "channel-properties",   tp_channel_borrow_immutable_properties (channel),
      NULL);

  self->priv->vino_tube_servers = g_slist_prepend
      (self->priv->vino_tube_servers, server);

  g_signal_connect (G_OBJECT (server), "disconnected", G_CALLBACK
      (vino_tube_servers_manager_disconnected_cb), self);

  self->priv->alternative_port++;

  vino_tube_server_share_with_tube (server, NULL);

  tp_handle_channels_context_accept (context);
}
