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

#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#ifdef VINO_ENABLE_LIBNOTIFY
#include <glib/gi18n.h>
#include <libnotify/notify.h>
#endif

#include "vino-status-tube-icon.h"
#include "vino-enums.h"
#include "vino-util.h"

struct _VinoStatusTubeIconPrivate
{
  GtkMenu *menu;
  VinoTubeServer *server;
  GtkWidget *disconnect_dialog;
  VinoStatusTubeIconVisibility visibility;

#ifdef VINO_ENABLE_LIBNOTIFY
  NotifyNotification *new_client_notification;
#endif
};

G_DEFINE_TYPE (VinoStatusTubeIcon, vino_status_tube_icon, GTK_TYPE_STATUS_ICON);

enum
{
  PROP_0,
  PROP_SERVER,
  PROP_VISIBILITY
};

static void
vino_status_tube_icon_finalize (GObject *object)
{
  VinoStatusTubeIcon *icon = VINO_STATUS_TUBE_ICON (object);

#ifdef VINO_ENABLE_LIBNOTIFY
  if (icon->priv->new_client_notification != NULL)
    {
      notify_notification_close (icon->priv->new_client_notification, NULL);
      g_object_unref (icon->priv->new_client_notification);
      icon->priv->new_client_notification = NULL;
    }
#endif

  if (icon->priv->menu != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET(icon->priv->menu));
      icon->priv->menu = NULL;
    }

  if (icon->priv->disconnect_dialog != NULL)
    {
      gtk_widget_destroy (icon->priv->disconnect_dialog);
      icon->priv->disconnect_dialog = NULL;
    }

  G_OBJECT_CLASS (vino_status_tube_icon_parent_class)->finalize (object);
}

void
vino_status_tube_icon_update_state (VinoStatusTubeIcon *icon)
{
  char     *tooltip;
  gboolean visible;

  g_return_if_fail (VINO_IS_STATUS_TUBE_ICON (icon));

  visible = !vino_server_get_on_hold (VINO_SERVER (icon->priv->server));

  tooltip = g_strdup (_("Desktop sharing is enabled"));

  visible = visible && (icon->priv->visibility == VINO_STATUS_TUBE_ICON_VISIBILITY_ALWAYS);

  gtk_status_icon_set_tooltip_text (GTK_STATUS_ICON (icon), tooltip);
  gtk_status_icon_set_visible (GTK_STATUS_ICON (icon), visible);

  g_free (tooltip);
}

static void
vino_status_tube_icon_init (VinoStatusTubeIcon *icon)
{
  icon->priv = G_TYPE_INSTANCE_GET_PRIVATE (icon, VINO_TYPE_STATUS_TUBE_ICON, VinoStatusTubeIconPrivate);
#ifdef VINO_ENABLE_LIBNOTIFY
  icon->priv->new_client_notification = NULL;
#endif
}

static void
vino_status_tube_icon_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  VinoStatusTubeIcon *icon = VINO_STATUS_TUBE_ICON (object);

  switch (prop_id)
    {
    case PROP_SERVER:
      icon->priv->server = g_value_get_object (value);
      break;
    case PROP_VISIBILITY:
      vino_status_tube_icon_set_visibility (icon, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_status_tube_icon_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  VinoStatusTubeIcon *icon = VINO_STATUS_TUBE_ICON (object);

  switch (prop_id)
    {
    case PROP_SERVER:
      g_value_set_object (value, icon->priv->server);
      break;
    case PROP_VISIBILITY:
      g_value_set_enum (value, icon->priv->visibility);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

VinoStatusTubeIcon*
vino_status_tube_icon_new (VinoTubeServer *server,
    GdkScreen *screen)
{
  g_return_val_if_fail (VINO_IS_TUBE_SERVER (server), NULL);
  g_return_val_if_fail (GDK_IS_SCREEN  (screen), NULL);

  return g_object_new (VINO_TYPE_STATUS_TUBE_ICON,
      "icon-name", "preferences-desktop-remote-desktop",
      "server", server, "screen", screen, NULL);
}

static void
vino_status_tube_icon_preferences (VinoStatusTubeIcon *icon)
{
  GdkScreen *screen;
  GError *error = NULL;

  screen = gtk_status_icon_get_screen (GTK_STATUS_ICON (icon));
  if (!gdk_spawn_command_line_on_screen (screen, "vino-preferences", &error))
    {
      vino_util_show_error (_("Error displaying preferences"),
          error->message, NULL);
      g_error_free (error);
    }
}

static void
vino_status_tube_icon_help (VinoStatusTubeIcon *icon)
{
  GdkScreen *screen;
  GError    *error = NULL;

  screen = gtk_status_icon_get_screen (GTK_STATUS_ICON (icon));
  if (!gtk_show_uri (screen, "ghelp:user-guide?goscustdesk-90",
      GDK_CURRENT_TIME, &error))
    {
      vino_util_show_error (_("Error displaying help"), error->message, NULL);
      g_error_free (error);
    }
}

static void
vino_status_tube_icon_disconnect_client (VinoStatusTubeIcon *icon,
    gint response)
{
  gtk_widget_destroy (icon->priv->disconnect_dialog);
  icon->priv->disconnect_dialog = NULL;

  if (response == GTK_RESPONSE_OK)
    {
      vino_tube_server_close_tube (icon->priv->server);
    }
}

static void
vino_status_tube_icon_disconnect_confirm (VinoStatusTubeIcon *icon)
{
  char      *primary_msg;
  char      *secondary_msg;

  if (icon->priv->disconnect_dialog)
  {
    gtk_window_present (GTK_WINDOW(icon->priv->disconnect_dialog));
    return;
  }

  /* Translators: %s is the alias of the telepathy contact */
  primary_msg   = g_strdup_printf
      (_("Are you sure you want to disconnect '%s'?"),
      vino_tube_server_get_alias (icon->priv->server));
  secondary_msg = g_strdup_printf
      (_("The remote user '%s' will be disconnected. Are you sure?"),
      vino_tube_server_get_alias (icon->priv->server));

  icon->priv->disconnect_dialog = gtk_message_dialog_new (NULL,
      GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_CANCEL, "%s", primary_msg);

  gtk_window_set_skip_taskbar_hint (GTK_WINDOW
      (icon->priv->disconnect_dialog), FALSE);

  gtk_dialog_add_button (GTK_DIALOG (icon->priv->disconnect_dialog),
      _("Disconnect"), GTK_RESPONSE_OK);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG
      (icon->priv->disconnect_dialog), "%s", secondary_msg);

  g_signal_connect_swapped (icon->priv->disconnect_dialog, "response",
      G_CALLBACK (vino_status_tube_icon_disconnect_client),
      icon);

  gtk_widget_show_all (GTK_WIDGET(icon->priv->disconnect_dialog));

  g_free (primary_msg);
  g_free (secondary_msg);
}

static void
vino_status_tube_icon_popup_menu (GtkStatusIcon *status_icon, guint button,
    guint32 timestamp)
{
  VinoStatusTubeIcon *icon = VINO_STATUS_TUBE_ICON (status_icon);
  GtkWidget *item;
  char *str;

  icon->priv->menu = (GtkMenu*) gtk_menu_new ();

  item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
      gtk_image_new_from_stock (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU));
  g_signal_connect_swapped (item, "activate",
      G_CALLBACK (vino_status_tube_icon_preferences), icon);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  /* Translators: %s is the alias of the telepathy contact */
  str = g_strdup_printf (_("Disconnect %s"),
      vino_tube_server_get_alias (icon->priv->server));

  item  = gtk_image_menu_item_new_with_label (str);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
  gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU));

  g_signal_connect_swapped (item, "activate",
      G_CALLBACK (vino_status_tube_icon_disconnect_confirm), icon);

  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  g_free (str);

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  item = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
      gtk_image_new_from_stock (GTK_STOCK_HELP, GTK_ICON_SIZE_MENU));
  g_signal_connect_swapped (item, "activate",
      G_CALLBACK (vino_status_tube_icon_help), icon);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  gtk_menu_popup (GTK_MENU (icon->priv->menu), NULL, NULL,
      gtk_status_icon_position_menu, icon,
      button, timestamp);
  if (button == 0)
    gtk_menu_shell_select_first (GTK_MENU_SHELL (icon->priv->menu), FALSE);
}

static void
vino_status_tube_icon_activate (GtkStatusIcon *icon)
{
  vino_status_tube_icon_preferences (VINO_STATUS_TUBE_ICON (icon));
}

static void
vino_status_tube_icon_class_init (VinoStatusTubeIconClass *klass)
{
  GObjectClass       *gobject_class     = G_OBJECT_CLASS (klass);
  GtkStatusIconClass *status_icon_class = GTK_STATUS_ICON_CLASS (klass);

  gobject_class->finalize     = vino_status_tube_icon_finalize;
  gobject_class->set_property = vino_status_tube_icon_set_property;
  gobject_class->get_property = vino_status_tube_icon_get_property;

  status_icon_class->activate   = vino_status_tube_icon_activate;
  status_icon_class->popup_menu = vino_status_tube_icon_popup_menu;

  g_object_class_install_property (gobject_class,
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
  g_object_class_install_property (gobject_class,
      PROP_VISIBILITY,
      g_param_spec_enum ("visibility",
      "Icon visibility",
      "When the icon must be shown",
      VINO_TYPE_STATUS_TUBE_ICON_VISIBILITY,
      VINO_STATUS_TUBE_ICON_VISIBILITY_CLIENT,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (VinoStatusTubeIconPrivate));
}

void
vino_status_tube_icon_set_visibility (VinoStatusTubeIcon *icon,
    VinoStatusTubeIconVisibility  visibility)
{
  g_return_if_fail (VINO_IS_STATUS_TUBE_ICON (icon));
  g_return_if_fail (visibility != VINO_STATUS_TUBE_ICON_VISIBILITY_INVALID);

  if (visibility != icon->priv->visibility)
    {
      icon->priv->visibility = visibility;
      vino_status_tube_icon_update_state (icon);
    }
}

#ifdef VINO_ENABLE_LIBNOTIFY
static void
vino_status_tube_icon_show_invalidated_notif_closed
    (VinoStatusTubeIcon *icon)
{
  dprintf (TUBE, "Notification was closed");
  vino_tube_server_fire_closed (icon->priv->server);
}
#endif

void
vino_status_tube_icon_show_notif (VinoStatusTubeIcon *icon,
    const gchar *summary, const gchar *body, gboolean invalidated)
{
#ifdef VINO_ENABLE_LIBNOTIFY
#define NOTIFICATION_TIMEOUT 5

  GError *error;
  const gchar *filename = NULL;

  if (!notify_is_initted () &&  !notify_init (g_get_application_name ()))
    {
      g_printerr (_("Error initializing libnotify\n"));
      return;
    }

  if (icon->priv->new_client_notification != NULL)
    {
      notify_notification_close (icon->priv->new_client_notification, NULL);
      g_object_unref (icon->priv->new_client_notification);
      icon->priv->new_client_notification = NULL;
    }

  filename = vino_tube_server_get_avatar_filename (icon->priv->server);

  if (filename == NULL)
      filename = "stock_person";

  icon->priv->new_client_notification =
      notify_notification_new (summary, body, filename);

  notify_notification_set_timeout (icon->priv->new_client_notification,
      NOTIFICATION_TIMEOUT * 1000);

  if (invalidated)
    g_signal_connect_swapped (icon->priv->new_client_notification, "closed",
        G_CALLBACK (vino_status_tube_icon_show_invalidated_notif_closed),
        icon);

  error = NULL;
  if (!notify_notification_show (icon->priv->new_client_notification, &error))
    {
      g_printerr (_("Error while displaying notification bubble: %s\n"),
                  error->message);
      g_error_free (error);
    }

#undef NOTIFICATION_TIMEOUT
#else
  if (invalidated)
    vino_tube_server_fire_closed (icon->priv->server);
#endif /* VINO_ENABLE_LIBNOTIFY */
}

