/*
 * Copyright (C) 2006 Jonh Wendell
 * Copyright (C) 2007 Mark McLoughlin
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
 *      Jonh Wendell <wendell@bani.com.br>
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>
#include <gtk/gtk.h>
#include <string.h>
#ifdef VINO_ENABLE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include "vino-status-icon.h"
#include "vino-enums.h"
#include "vino-util.h"

struct _VinoStatusIconPrivate
{
  GtkMenu    *menu;
  VinoServer *server;
  GSList     *clients;
  GtkWidget  *disconnect_dialog;
  VinoStatusIconVisibility visibility;

#ifdef VINO_ENABLE_LIBNOTIFY
  NotifyNotification *new_client_notification;
#endif
};

G_DEFINE_TYPE (VinoStatusIcon, vino_status_icon, GTK_TYPE_STATUS_ICON);

enum
{
  PROP_0,
  PROP_SERVER,
  PROP_VISIBILITY
};

typedef struct
{
  VinoStatusIcon *icon;
  VinoClient     *client;
}VinoStatusIconNotify;

static gboolean vino_status_icon_show_new_client_notification (gpointer user_data);

static void
vino_status_icon_finalize (GObject *object)
{
  VinoStatusIcon *icon = VINO_STATUS_ICON (object);

#ifdef VINO_ENABLE_LIBNOTIFY
  if (icon->priv->new_client_notification)
    g_object_unref (icon->priv->new_client_notification);
  icon->priv->new_client_notification = NULL;
#endif

  if (icon->priv->menu)
    gtk_widget_destroy (GTK_WIDGET(icon->priv->menu));
  icon->priv->menu = NULL;

  if (icon->priv->clients)
    g_slist_free (icon->priv->clients);
  icon->priv->clients = NULL;

  if (icon->priv->disconnect_dialog)
    gtk_widget_destroy (icon->priv->disconnect_dialog);
  icon->priv->disconnect_dialog = NULL;

  G_OBJECT_CLASS (vino_status_icon_parent_class)->finalize (object);
}

void
vino_status_icon_update_state (VinoStatusIcon *icon)
{
  char     *tooltip;
  gboolean visible;

  g_return_if_fail (VINO_IS_STATUS_ICON (icon));

  visible = !vino_server_get_on_hold (icon->priv->server);

  tooltip = g_strdup (_("Desktop sharing is enabled"));
  
  if (icon->priv->clients != NULL)
    {
      int n_clients;

      n_clients = g_slist_length (icon->priv->clients);

      tooltip = g_strdup_printf (ngettext ("One person is connected",
                                           "%d people are connected",
                                           n_clients),
                                 n_clients);
      visible = (visible) && ( (icon->priv->visibility == VINO_STATUS_ICON_VISIBILITY_CLIENT) ||
			     (icon->priv->visibility == VINO_STATUS_ICON_VISIBILITY_ALWAYS) );
    }
  else
    visible = visible && (icon->priv->visibility == VINO_STATUS_ICON_VISIBILITY_ALWAYS);

  gtk_status_icon_set_tooltip_text (GTK_STATUS_ICON (icon), tooltip);
  gtk_status_icon_set_visible (GTK_STATUS_ICON (icon), visible);

  g_free (tooltip);
}

static void
vino_status_icon_init (VinoStatusIcon *icon)
{
  icon->priv = G_TYPE_INSTANCE_GET_PRIVATE (icon, VINO_TYPE_STATUS_ICON, VinoStatusIconPrivate);
}

static void
vino_status_icon_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  VinoStatusIcon *icon = VINO_STATUS_ICON (object);

  switch (prop_id)
    {
    case PROP_SERVER:
      icon->priv->server = g_value_get_object (value);
      break;
    case PROP_VISIBILITY:
      vino_status_icon_set_visibility (icon, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_status_icon_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  VinoStatusIcon *icon = VINO_STATUS_ICON (object);

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

VinoStatusIcon *
vino_status_icon_new (VinoServer *server,
                      GdkScreen  *screen)
{
  g_return_val_if_fail (VINO_IS_SERVER (server), NULL);
  g_return_val_if_fail (GDK_IS_SCREEN  (screen), NULL);
  
  return g_object_new (VINO_TYPE_STATUS_ICON,
                       "icon-name", "preferences-desktop-remote-desktop",
                       "server",    server,
                       "screen",    screen,
                       NULL);
}

VinoServer *
vino_status_icon_get_server (VinoStatusIcon *icon)
{
  g_return_val_if_fail (VINO_IS_STATUS_ICON (icon), NULL);

  return icon->priv->server;
}

static void
vino_status_icon_preferences (VinoStatusIcon *icon)
{
  GdkScreen *screen;
  GError *error = NULL;

  screen = gtk_status_icon_get_screen (GTK_STATUS_ICON (icon));
  if (!gdk_spawn_command_line_on_screen (screen, "vino-preferences", &error))
    {
      vino_util_show_error (_("Error displaying preferences"),
			    error->message,
			    NULL);
      g_error_free (error);
    }
}

static void
vino_status_icon_help (VinoStatusIcon *icon)
{
  GdkScreen *screen;
  GError    *error = NULL;

  screen = gtk_status_icon_get_screen (GTK_STATUS_ICON (icon));
  if (!gtk_show_uri (screen,
		     "ghelp:user-guide?goscustdesk-90",
		     GDK_CURRENT_TIME,
		     &error))
    {
      vino_util_show_error (_("Error displaying help"),
			    error->message,
			    NULL);
      g_error_free (error);
    }
}

static void
vino_status_icon_about (VinoStatusIcon *icon)
{

  g_return_if_fail (VINO_IS_STATUS_ICON (icon));

  const char *authors[] = {
    "Mark McLoughlin <mark@skynet.ie>",
    "Calum Benson <calum.benson@sun.com>",
    "Federico Mena Quintero <federico@ximian.com>",
    "Sebastien Estienne <sebastien.estienne@gmail.com>",
    "Shaya Potter <spotter@cs.columbia.edu>",
    "Steven Zhang <steven.zhang@sun.com>",
    "Srirama Sharma <srirama.sharma@wipro.com>",
    "Jonh Wendell <wendell@bani.com.br>",
    NULL
  };
  char *license;
  char *translators;

  license = _("Licensed under the GNU General Public License Version 2\n\n"
              "Vino is free software; you can redistribute it and/or\n"
              "modify it under the terms of the GNU General Public License\n"
              "as published by the Free Software Foundation; either version 2\n"
              "of the License, or (at your option) any later version.\n\n"
              "Vino is distributed in the hope that it will be useful,\n"
              "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
              "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
              "GNU General Public License for more details.\n\n"
              "You should have received a copy of the GNU General Public License\n"
              "along with this program; if not, write to the Free Software\n"
              "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA\n"
              "02110-1301, USA.\n");

  /* Translators comment: put your own name here to appear in the about dialog. */
  translators = _("translator-credits");

  if (!strcmp (translators, "translator-credits"))
    translators = NULL;

  gtk_show_about_dialog (NULL,
                         "comments",           _("Share your desktop with other users"),
                         "version",            VERSION,
                         "license",            license,
                         "authors",            authors,
                         "translator-credits", translators,
                         "logo-icon-name",     "preferences-desktop-remote-desktop",
                         NULL);
}

static void
vino_status_icon_disconnect_client (VinoStatusIconNotify *a, gint response)
{

  GSList *l;
  GSList *next;

  VinoStatusIcon *icon    = a->icon;
  VinoClient     *client  = a->client;

  gtk_widget_destroy (icon->priv->disconnect_dialog);
  icon->priv->disconnect_dialog = NULL;

  if (response != GTK_RESPONSE_OK)
  {
    g_free (a);
    return;
  }

  if (client)
  {
    if (g_slist_find (icon->priv->clients, client))
      vino_client_disconnect (client);
  }
  else
    for (l = icon->priv->clients; l; l = next)
      {
        VinoClient *client = l->data;

        next = l->next;

        vino_client_disconnect (client);
      }

  g_free (a);
}

static void
vino_status_icon_disconnect_confirm (VinoStatusIconNotify *a)
{
  char      *primary_msg;
  char      *secondary_msg;

  VinoStatusIcon *icon    = a->icon;
  VinoClient     *client  = a->client;

  if (icon->priv->disconnect_dialog)
  {
    gtk_window_present (GTK_WINDOW(icon->priv->disconnect_dialog));
    return;
  }

  if (client != NULL)
    {
      /* Translators: %s is a hostname */
      primary_msg   = g_strdup_printf (_("Are you sure you want to disconnect '%s'?"),
                                       vino_client_get_hostname (client));
      secondary_msg = g_strdup_printf (_("The remote user from '%s' will be disconnected. Are you sure?"),
                                       vino_client_get_hostname (client));
    }
  else
    {
      primary_msg   = g_strdup (_("Are you sure you want to disconnect all clients?"));
      secondary_msg = g_strdup (_("All remote users will be disconnected. Are you sure?"));
    }

  icon->priv->disconnect_dialog = gtk_message_dialog_new (NULL,
                                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                                          GTK_MESSAGE_QUESTION,
                                                          GTK_BUTTONS_CANCEL,
                                                          "%s",
                                                          primary_msg);

  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (icon->priv->disconnect_dialog), FALSE);

  gtk_dialog_add_button (GTK_DIALOG (icon->priv->disconnect_dialog), _("Disconnect"), GTK_RESPONSE_OK);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (icon->priv->disconnect_dialog),
                                            "%s", secondary_msg);

  g_signal_connect_swapped (icon->priv->disconnect_dialog, "response",
                            G_CALLBACK (vino_status_icon_disconnect_client), (gpointer) a);  
  gtk_widget_show_all (GTK_WIDGET(icon->priv->disconnect_dialog));

  g_free (primary_msg);
  g_free (secondary_msg);
}

static void
vino_status_icon_popup_menu (GtkStatusIcon *status_icon,
			     guint          button,
			     guint32        timestamp)
{
  VinoStatusIcon *icon = VINO_STATUS_ICON (status_icon);
  GtkWidget      *item;
  GSList         *l;
  VinoStatusIconNotify *a;
  guint          n_clients;

  icon->priv->menu = (GtkMenu*) gtk_menu_new ();

  item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 gtk_image_new_from_stock (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU));
  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (vino_status_icon_preferences), icon);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  n_clients = g_slist_length (icon->priv->clients);
  if (n_clients > 1)
    {
      item  = gtk_image_menu_item_new_with_label (_("Disconnect all"));
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                     gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU));

      a = g_new (VinoStatusIconNotify, 1);
      a->icon   = icon;
      a->client = NULL;

      g_signal_connect_swapped (item, "activate",
                                G_CALLBACK (vino_status_icon_disconnect_confirm), (gpointer) a);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);
    }

  for (l = icon->priv->clients; l; l = l->next)
    {
      VinoClient *client = l->data;
      char       *str;
      
      a = g_new (VinoStatusIconNotify, 1);
      a->icon   = icon;
      a->client = client;

      /* Translators: %s is a hostname */
      str = g_strdup_printf (_("Disconnect %s"), vino_client_get_hostname (client));

      item  = gtk_image_menu_item_new_with_label (str);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                     gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU));
      g_signal_connect_swapped (item, "activate",
                                G_CALLBACK (vino_status_icon_disconnect_confirm), (gpointer) a);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);
      
      g_free (str);
    }

  if (n_clients)
    {
      item = gtk_separator_menu_item_new ();
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);
    }

  item = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 gtk_image_new_from_stock (GTK_STOCK_HELP, GTK_ICON_SIZE_MENU));
  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (vino_status_icon_help), icon);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  item = gtk_image_menu_item_new_with_mnemonic (_("_About"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 gtk_image_new_from_stock (GTK_STOCK_ABOUT, GTK_ICON_SIZE_MENU));
  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (vino_status_icon_about), icon);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  gtk_menu_popup (GTK_MENU (icon->priv->menu), NULL, NULL,
		  gtk_status_icon_position_menu, icon,
		  button, timestamp);
  if (button == 0)
    gtk_menu_shell_select_first (GTK_MENU_SHELL (icon->priv->menu), FALSE);
}

static void
vino_status_icon_activate (GtkStatusIcon *icon)
{
  vino_status_icon_preferences (VINO_STATUS_ICON (icon));
}

void
vino_status_icon_add_client (VinoStatusIcon *icon,
                             VinoClient     *client)
{
  g_return_if_fail (VINO_IS_STATUS_ICON (icon));
  g_return_if_fail (client != NULL);

  icon->priv->clients = g_slist_append (icon->priv->clients, client);

  vino_status_icon_update_state (icon);

  if (gtk_status_icon_get_visible (GTK_STATUS_ICON (icon)))
    {
      VinoStatusIconNotify *a;

      a = g_new (VinoStatusIconNotify, 1);
      a->icon   = icon;
      a->client = client;
      g_timeout_add_seconds (1, 
                             vino_status_icon_show_new_client_notification,
                             (gpointer) a);
    }
}

gboolean
vino_status_icon_remove_client (VinoStatusIcon *icon,
                                VinoClient     *client)
{
  g_return_val_if_fail (VINO_IS_STATUS_ICON (icon), TRUE);
  g_return_val_if_fail (client != NULL, TRUE);

  if (!icon->priv->clients)
    return FALSE;

  icon->priv->clients = g_slist_remove (icon->priv->clients, client);

  vino_status_icon_update_state (icon);

  return icon->priv->clients == NULL;
}

static void
vino_status_icon_class_init (VinoStatusIconClass *klass)
{
  GObjectClass       *gobject_class     = G_OBJECT_CLASS (klass);
  GtkStatusIconClass *status_icon_class = GTK_STATUS_ICON_CLASS (klass);

  gobject_class->finalize     = vino_status_icon_finalize;
  gobject_class->set_property = vino_status_icon_set_property;
  gobject_class->get_property = vino_status_icon_get_property;

  status_icon_class->activate   = vino_status_icon_activate;
  status_icon_class->popup_menu = vino_status_icon_popup_menu;

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
						      VINO_TYPE_STATUS_ICON_VISIBILITY,
						      VINO_STATUS_ICON_VISIBILITY_CLIENT,
						      G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT |
						      G_PARAM_STATIC_NAME |
						      G_PARAM_STATIC_NICK |
						      G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (VinoStatusIconPrivate));
}

#ifdef VINO_ENABLE_LIBNOTIFY
static void
vino_status_handle_new_client_notification_closed (VinoStatusIcon *icon)
{
  g_object_unref (icon->priv->new_client_notification);
  icon->priv->new_client_notification = NULL;
}
#endif /* VINO_ENABLE_LIBNOTIFY */

static gboolean
vino_status_icon_show_new_client_notification (gpointer user_data)
{
#ifdef VINO_ENABLE_LIBNOTIFY
#define NOTIFICATION_TIMEOUT 5

  GError     *error;
  const char *summary;
  char       *body;

  VinoStatusIconNotify *a = (VinoStatusIconNotify *)user_data;
  VinoStatusIcon *icon    = a->icon;
  VinoClient     *client  = a->client;

  if (vino_server_get_prompt_enabled (icon->priv->server))
  {
    g_free (user_data);
    return FALSE;
  }

  if (!notify_is_initted () &&  !notify_init (g_get_application_name ()))
    {
      g_printerr (_("Error initializing libnotify\n"));
      g_free (user_data);
      return FALSE;
    }

  if (g_slist_index (icon->priv->clients, client) == -1)
    {
      g_free (user_data);
      return FALSE;
    }

  if (icon->priv->new_client_notification)
    {
      notify_notification_close (icon->priv->new_client_notification, NULL);
      g_object_unref (icon->priv->new_client_notification);
      icon->priv->new_client_notification = NULL;
    }

  if (vino_server_get_view_only (icon->priv->server))
    {
      summary = _("Another user is viewing your desktop");
      body = g_strdup_printf (_("A user on the computer '%s' is remotely viewing your desktop."),
                              vino_client_get_hostname (client));
    }
  else
    {
      summary = _("Another user is controlling your desktop");
      body = g_strdup_printf (_("A user on the computer '%s' is remotely controlling your desktop."),
                              vino_client_get_hostname (client));
    }

  icon->priv->new_client_notification =
    notify_notification_new_with_status_icon (summary,
                                              body,
                                              "preferences-desktop-remote-desktop",
                                              GTK_STATUS_ICON (icon));

  g_free (body);

  g_signal_connect_swapped (icon->priv->new_client_notification, "closed",
                            G_CALLBACK (vino_status_handle_new_client_notification_closed),
                            icon);

  notify_notification_set_timeout (icon->priv->new_client_notification,
                                   NOTIFICATION_TIMEOUT * 1000);

  error = NULL;
  if (!notify_notification_show (icon->priv->new_client_notification, &error))
    {
      g_printerr (_("Error while displaying notification bubble: %s\n"),
                  error->message);
      g_error_free (error);
    }

  g_free (user_data);

#undef NOTIFICATION_TIMEOUT
#endif /* VINO_ENABLE_LIBNOTIFY */

  return FALSE;
}

void
vino_status_icon_set_visibility (VinoStatusIcon *icon,
				 VinoStatusIconVisibility  visibility)
{
  g_return_if_fail (VINO_IS_STATUS_ICON (icon));
  g_return_if_fail (visibility != VINO_STATUS_ICON_VISIBILITY_INVALID);

  if (visibility != icon->priv->visibility)
    {
      icon->priv->visibility = visibility;
      vino_status_icon_update_state (icon);
    }
}

VinoStatusIconVisibility
vino_status_icon_get_visibility (VinoStatusIcon *icon)
{
  g_return_val_if_fail (VINO_IS_STATUS_ICON (icon), VINO_STATUS_ICON_VISIBILITY_INVALID);

  return icon->priv->visibility;
}
