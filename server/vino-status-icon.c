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

  g_free (icon->priv);
  icon->priv = NULL;

  if (G_OBJECT_CLASS (vino_status_icon_parent_class)->finalize)
    G_OBJECT_CLASS (vino_status_icon_parent_class)->finalize (object);
}


static void
vino_status_icon_init (VinoStatusIcon *icon)
{
  icon->priv = g_new0 (VinoStatusIconPrivate, 1);
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
                       "server", server,
                       /*"screen", screen,*/ /*FIXME: available in gtk 2.12*/
                       NULL);
}

VinoServer *
vino_status_icon_get_server (VinoStatusIcon *icon)
{
  g_return_val_if_fail (VINO_IS_STATUS_ICON (icon), NULL);

  return icon->priv->server;
}


static void
vino_status_icon_preferences (GtkMenuItem    *item,
                              VinoStatusIcon *icon)
{
  const char *command = "vino-preferences";
  GError *error = NULL;

  g_return_if_fail (VINO_IS_STATUS_ICON (icon));

  if (! gdk_spawn_command_line_on_screen (vino_server_get_screen (icon->priv->server), /*FIXME: In gtk 2.12 we have screen property */
                                          command,
                                          &error))
    {
      GtkWidget *message_dialog;

      message_dialog = gtk_message_dialog_new (NULL,
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_ERROR,
					       GTK_BUTTONS_CLOSE,
					       _("There was an error displaying preferences:\n%s"),
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
vino_status_icon_help (GtkMenuItem    *item,
                       VinoStatusIcon *icon)
{
  const char *command = "yelp ghelp:user-guide?goscustdesk-90";
  GError *error = NULL;

  g_return_if_fail (VINO_IS_STATUS_ICON (icon));

  if (! gdk_spawn_command_line_on_screen (vino_server_get_screen (icon->priv->server), /*FIXME: In gtk 2.12 we have screen property */
                                          command,
                                          &error))

    {
      GtkWidget *message_dialog;

      message_dialog = gtk_message_dialog_new (NULL,
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_ERROR,
					       GTK_BUTTONS_CLOSE,
					       _("There was an error displaying help:\n%s"),
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
vino_status_icon_about (GtkMenuItem    *item,
                        VinoStatusIcon *icon)
{

  g_return_if_fail (VINO_IS_STATUS_ICON (icon));

  const char *authors[] = {
	     "Mark McLoughlin  <mark@skynet.ie>",
	     "Calum Benson <calum.benson@sun.com>",
             "Jonh Wendell <wendell@bani.com.br>",
	     NULL};
	//const char *documenters[] = {
	//	NULL};
	//const char *artists[] = {
	//	NULL};
  const char *license[] = {
		N_("Licensed under the GNU General Public License Version 2"),
		N_("Vino is free software; you can redistribute it and/or\n"
		   "modify it under the terms of the GNU General Public License\n"
		   "as published by the Free Software Foundation; either version 2\n"
		   "of the License, or (at your option) any later version."),
		N_("Vino is distributed in the hope that it will be useful,\n"
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License\n"
		   "along with this program; if not, write to the Free Software\n"
		   "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA\n"
		   "02110-1301, USA.")
	};
  	const char  *translators = _("translator-credits");
	char	    *license_trans;

	/* Translators comment: put your own name here to appear in the about dialog. */
  	if (!strcmp (translators, "translator-credits")) {
		translators = NULL;
	}

	license_trans = g_strconcat (_(license[0]), "\n\n", _(license[1]), "\n\n",
				     _(license[2]), "\n\n", _(license[3]), "\n",  NULL);

	gtk_window_set_default_icon_name ("gnome-remote-desktop");
	gtk_show_about_dialog (NULL,
			       "name", _("GNOME Remote Access"),
			       "version", VERSION,
			       //"copyright", "Copyright \xc2\xa9 2005-2006 Richard Hughes",
			       "license", license_trans,
			       //"website", "www",
			       "comments", _("Shares your desktop with others"),
			       "authors", authors,
			       //"documenters", documenters,
			       //"artists", artists,
			       "translator-credits", translators,
			       "logo-icon-name", "gnome-remote-desktop",
			       NULL);
	g_free (license_trans);
}

static void
vino_status_icon_disconnect_client (GtkMenuItem *item,
                                    VinoClient  *client)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG(gtk_message_dialog_new (NULL,
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_OTHER,
						GTK_BUTTONS_NONE,
						_("Are you sure you want to disconnect the remote machine?")));

  gtk_window_set_icon_name (GTK_WINDOW(dialog), "gnome-remote-desktop");
  gtk_window_set_title (GTK_WINDOW(dialog), _("GNOME Remote Access - Confirmation"));
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW(dialog), FALSE);

  GtkWidget *image = gtk_image_new_from_icon_name ("gnome-remote-desktop", GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);
  gtk_message_dialog_set_image (dialog, image);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                         _("Leave as it is"), GTK_RESPONSE_NO,
                         _("Disconnect"), GTK_RESPONSE_YES,
                         NULL);

  gtk_message_dialog_format_secondary_text (dialog, 
                        _("The remote user from the machine '%s' will be disconnected. Are you sure?"),
                         vino_client_get_hostname(client));

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
    vino_server_disconnect_client (client);

  gtk_widget_destroy (GTK_WIDGET (image));
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
label_expose (GtkWidget *widget)
{
  /* Bad hack to make the label draw normally, instead of insensitive. */
  widget->state = GTK_STATE_NORMAL;

  return FALSE;
}

static void
vino_status_icon_popup_menu (VinoStatusIcon *icon,
			    guint            button,
			    guint            time)
{
  GtkWidget  *item;
  GtkWidget  *label;
  VinoClient *client = NULL;
  GtkWidget  *image;

  GSList *l;
  int number_of_clients = 0;
  GString *client_info;

  icon->priv->menu = (GtkMenu*) gtk_menu_new ();

  /* Preferences */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
  image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  g_signal_connect (G_OBJECT (item), "activate",
		   G_CALLBACK (vino_status_icon_preferences), icon);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  /* Separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  /* Clients */
  number_of_clients = g_slist_length(icon->priv->clients);

  if (number_of_clients > 0)
    {
      /* Information about connected clients */
      label = gtk_label_new (_("Connected machines, click to disconnect"));
      g_signal_connect (G_OBJECT (label), "expose-event", G_CALLBACK (label_expose), NULL);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

      item = gtk_menu_item_new ();
      gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);

      gtk_container_add (GTK_CONTAINER (item), label);
      gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

      /* List of clients */
      for (l = icon->priv->clients; l; l = l->next)
        {
          client = (VinoClient *) l->data;

          client_info = g_string_new("");
          g_string_printf(client_info, "'%s'", vino_client_get_hostname(client));
          if (vino_server_get_view_only(icon->priv->server))
            g_string_append(client_info, _(" is watching your desktop"));
          else
            g_string_append(client_info, _(" is controlling your desktop"));

          item  = gtk_image_menu_item_new_with_label (client_info->str);
          image = gtk_image_new_from_icon_name (GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

          g_signal_connect (G_OBJECT (item), "activate",
	      	            G_CALLBACK (vino_status_icon_disconnect_client), client);
          gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);
      
          g_string_free(client_info, TRUE);
        }

      /* Separator */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);
    
    }


  /* Help */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
  image = gtk_image_new_from_icon_name (GTK_STOCK_HELP, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  g_signal_connect (G_OBJECT (item), "activate",
		  G_CALLBACK (vino_status_icon_help), icon);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  /* About */
  item = gtk_image_menu_item_new_with_mnemonic (_("_About"));
  image = gtk_image_new_from_icon_name (GTK_STOCK_ABOUT, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  g_signal_connect (G_OBJECT (item), "activate",
		  G_CALLBACK (vino_status_icon_about), icon);
  gtk_menu_shell_append (GTK_MENU_SHELL (icon->priv->menu), item);

  gtk_widget_show_all (GTK_WIDGET (icon->priv->menu));
  gtk_menu_popup (GTK_MENU (icon->priv->menu), NULL, NULL,
		gtk_status_icon_position_menu, icon,
		1, gtk_get_current_event_time());

}

static void
vino_status_icon_activate (VinoStatusIcon *icon)
{
  vino_status_icon_popup_menu (icon, 1, 1);
}

static void
vino_status_icon_update_tooltip (VinoStatusIcon *icon)
{
  GString *tooltip;
  int number_of_clients = g_slist_length (icon->priv->clients);
  
  if (number_of_clients > 0)
    {
      /* Set its tooltip, based on number of connected clients */
      tooltip = g_string_new ("");

      if (number_of_clients == 1)
        g_string_printf (tooltip, _("One person is connected"));
      else
        g_string_printf (tooltip, _("%d people are connected"), number_of_clients);

      gtk_status_icon_set_tooltip (GTK_STATUS_ICON(icon), tooltip->str);

      g_string_free (tooltip, TRUE);
    }
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

  return (g_slist_length(icon->priv->clients) <= 0);
}

static void
vino_status_icon_class_init (VinoStatusIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize     = vino_status_icon_finalize;
  gobject_class->set_property = vino_status_icon_set_property;
  gobject_class->get_property = vino_status_icon_get_property;

  GTK_STATUS_ICON_CLASS(klass)->activate   = vino_status_icon_popup_menu;
  GTK_STATUS_ICON_CLASS(klass)->popup_menu = vino_status_icon_activate;

  g_object_class_install_property (gobject_class,
				   PROP_SERVER,
				   g_param_spec_object ("server",
							_("Server"),
							_("The server"),
							VINO_TYPE_SERVER,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}
