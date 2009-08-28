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

struct _VinoTubeServersManagerPrivate
{
  GSList *vino_tube_servers;
  guint alternative_port;
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
  self->priv = VINO_TUBE_SERVERS_MANAGER_GET_PRIVATE (self);
  self->priv->vino_tube_servers = NULL;
  self->priv->alternative_port = 26570;
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

gboolean
vino_tube_servers_manager_share_with_tube
    (VinoTubeServersManager * object, const gchar *connection_path,
    const gchar *tube_path, GHashTable *channel_properties, GError **error)
{
  VinoTubeServersManager *self = VINO_TUBE_SERVERS_MANAGER (object);
  VinoTubeServer *server;
  GdkDisplay *display;
  GdkScreen *screen;

  /* the server is listenning only on lo as only the tube is supposed to
  connect to it */
  gchar * network_interface = "lo";

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
      "connection-path",      connection_path,
      "tube-path",            tube_path,
      "channel-properties",   channel_properties,
      NULL);

  self->priv->vino_tube_servers = g_slist_prepend
      (self->priv->vino_tube_servers, server);

  g_signal_connect (G_OBJECT (server), "disconnected", G_CALLBACK
      (vino_tube_servers_manager_disconnected_cb), self);

  self->priv->alternative_port++;

  return vino_tube_server_share_with_tube (server, error);
}

VinoTubeServersManager *
vino_tube_servers_manager_new (void)
{
  return g_object_new (VINO_TYPE_TUBE_SERVERS_MANAGER, NULL);
}
