/*
 * Copyright (C) 2006 Jonh Wendell
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
 */

#include <config.h>

#include "vino-status-icon.h"
#include "vino-util.h"

#include <gtk/gtk.h>
#include <string.h>

struct _VinoStatusIconPrivate
{
  GtkMenu    *menu;
  VinoServer *server;
  GSList     *clients;
};

G_DEFINE_TYPE (VinoStatusIcon, vino_status_icon, GTK_TYPE_STATUS_ICON);

enum
{
  PROP_0,
  PROP_SERVER
};

static void
vino_status_icon_finalize (GObject *object)
{
  VinoStatusIcon *icon = VINO_STATUS_ICON (object);

  if (icon->priv->menu)
    gtk_widget_destroy (GTK_WIDGET(icon->priv->menu));
  icon->priv->menu = NULL;

  if (icon->priv->clients)
    g_slist_free (icon->priv->clients);
  icon->priv->clients = NULL;

  G_OBJECT_CLASS (vino_status_icon_parent_class)->finalize (object);
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
                       "icon-name", "gnome-remote-desktop",
                       "server",    server,
#if GTK_CHECK_VERSION (2, 11, 0)
                       "screen",    screen,
#endif
                       NULL);
}

VinoServer *
vino_status_icon_get_server (VinoStatusIcon *icon)
{
  g_return_val_if_fail (VINO_IS_STATUS_ICON (icon), NULL);

  return icon->priv->server;
}

static void
vino_status_icon_spawn_command (VinoStatusIcon *icon,
                                const char     *command,
                                const char     *error_format)
{
  GdkScreen *screen;
  GError    *error;

  g_return_if_fail (VINO_IS_STATUS_ICON (icon));

#if GTK_CHECK_VERSION (2, 11, 0)
  screen = gtk_status_icon_get_screen (GTK_STATUS_ICON (icon));
#else
  screen = vino_server_get_screen (icon->priv->server);
#endif

  error = NULL;

  if (!gdk_spawn_command_line_on_screen (screen, command, &error))
    {
      GtkWidget *message_dialog;

      message_dialog = gtk_message_dialog_new (NULL,
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_ERROR,
					       GTK_BUTTONS_CLOSE,
                                               error_format,
					       error->message);
      gtk_window_set_resizable (GTK_WINDOW (message_dialog), FALSE);

      g_signal_connect (message_dialog, "response",
			G_CALLBACK (gtk_widget_destroy),
			NULL);

      gtk_widget_show (message_dialog);

      g_error_free (error);
    }
}

static void
vino_status_icon_preferences (VinoStatusIcon *icon)
{
  vino_status_icon_spawn_command (icon,
                                  "vino-preferences",
                                  _("There was an error displaying preferences:\n %s"));
}

static void
vino_status_icon_help (VinoStatusIcon *icon)
{
  vino_status_icon_spawn_command (icon,
                                  "yelp ghelp:user-guide?goscustdesk-90",
                                  _("There was an error displaying help:\n %s"));
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
                         "name",               _("GNOME Remote Desktop"),
                         "comments",           _("Share your desktop with other users"),
                         "version",            VERSION,
                         "license",            license,
                         "authors",            authors,
                         "translator-credits", translators,
                         "logo-icon-name",     "gnome-remote-desktop",
                         NULL);
}

static gboolean
vino_status_icon_disconnect_confirm (VinoClient *client)
{
  GtkWidget *dialog;
  char      *primary_msg;
  char      *secondary_msg;
  gboolean   retval;

  if (client != NULL)
    {
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

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_CANCEL,
                                   "%s",
                                   primary_msg);

  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Disconnect"), GTK_RESPONSE_OK);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary_msg);

  retval = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK;

  gtk_widget_destroy (dialog);

  g_free (primary_msg);
  g_free (secondary_msg);

  return retval;
}

static void
vino_status_icon_disconnect_client (VinoClient *client)
{
  if (vino_status_icon_disconnect_confirm (client))
    vino_client_disconnect (client);
}

static void
vino_status_icon_disconnect_all_clients (VinoStatusIcon *icon)
{
  GSList *l;
  GSList *next;

  if (!vino_status_icon_disconnect_confirm (NULL))
    return;

  for (l = icon->priv->clients; l; l = next)
    {
      VinoClient *client = l->data;

      next = l->next;

      vino_client_disconnect (client);
    }
}

static void
vino_status_icon_popup_menu (GtkStatusIcon *status_icon,
			     guint          button,
			     guint32        timestamp)
{
  VinoStatusIcon *icon = VINO_STATUS_ICON (status_icon);
  GtkWidget      *item;
  GSList         *l;

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

  item  = gtk_image_menu_item_new_with_label (_("Disconnect all"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU));
  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (vino_status_icon_disconnect_all_clients), icon);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  for (l = icon->priv->clients; l; l = l->next)
    {
      VinoClient *client = l->data;
      char       *str;

      str = g_strdup_printf (_("Disconnect %s"), vino_client_get_hostname (client));

      item  = gtk_image_menu_item_new_with_label (str);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                     gtk_image_new_from_stock (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU));
      g_signal_connect_swapped (item, "activate",
                                G_CALLBACK (vino_status_icon_disconnect_client), client);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);
      
      g_free (str);
    }

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

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

static void
vino_status_icon_update_tooltip (VinoStatusIcon *icon)
{
  char *tooltip;

  tooltip = NULL;
  
  if (icon->priv->clients != NULL)
    {
      int n_clients;

      n_clients = g_slist_length (icon->priv->clients);

      tooltip = g_strdup_printf (ngettext ("One person is connected",
                                           "%d people are connected",
                                           n_clients),
                                 n_clients);
    }
  
  gtk_status_icon_set_tooltip (GTK_STATUS_ICON (icon), tooltip);

  g_free (tooltip);
}

void
vino_status_icon_add_client (VinoStatusIcon *icon,
                             VinoClient     *client)
{
  g_return_if_fail (VINO_IS_STATUS_ICON (icon));
  g_return_if_fail (client != NULL);

  icon->priv->clients = g_slist_append (icon->priv->clients, client);

  vino_status_icon_update_tooltip (icon);
}

gboolean
vino_status_icon_remove_client (VinoStatusIcon *icon,
                                VinoClient     *client)
{
  g_return_val_if_fail (VINO_IS_STATUS_ICON (icon), TRUE);
  g_return_val_if_fail (client != NULL, TRUE);

  icon->priv->clients = g_slist_remove (icon->priv->clients, client);

  vino_status_icon_update_tooltip (icon);

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

  g_type_class_add_private (gobject_class, sizeof (VinoStatusIconPrivate));
}
