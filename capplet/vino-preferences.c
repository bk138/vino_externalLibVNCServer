/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *      Mark McLoughlin <mark@skynet.ie>
 *      Jonh Wendell <wendell@bani.com.br>
 */

#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <dbus/dbus-glib.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include "vino-message-box.h"
#include "vino-url-webservice.h"

#ifdef VINO_ENABLE_KEYRING
#include <gnome-keyring.h>
#endif

#ifdef VINO_ENABLE_LIBUNIQUE
#include <unique/unique.h>
#endif

#define VINO_PREFS_DIR                    "/desktop/gnome/remote_access"
#define VINO_PREFS_ENABLED                VINO_PREFS_DIR "/enabled"
#define VINO_PREFS_PROMPT_ENABLED         VINO_PREFS_DIR "/prompt_enabled"
#define VINO_PREFS_VIEW_ONLY              VINO_PREFS_DIR "/view_only"
#define VINO_PREFS_AUTHENTICATION_METHODS VINO_PREFS_DIR "/authentication_methods"
#define VINO_PREFS_VNC_PASSWORD           VINO_PREFS_DIR "/vnc_password"
#define VINO_PREFS_ICON_VISIBILITY        VINO_PREFS_DIR "/icon_visibility"
#define VINO_PREFS_USE_UPNP               VINO_PREFS_DIR "/use_upnp"

#define N_LISTENERS                       7

#define VINO_DBUS_BUS_NAME  "org.gnome.Vino"
#define VINO_DBUS_INTERFACE "org.gnome.VinoScreen"

typedef struct {
  GladeXML    *xml;
  GConfClient *client;

  GtkWidget   *dialog;
  GtkWidget   *writability_warning;
  GtkWidget   *allowed_toggle;
  GtkWidget   *view_only_toggle;
  GtkWidget   *message;
  GtkWidget   *prompt_enabled_toggle;
  GtkWidget   *password_toggle;
  GtkWidget   *password_entry;
  GtkWidget   *icon_always_radio;
  GtkWidget   *icon_client_radio;
  GtkWidget   *icon_never_radio;
  GtkWidget   *use_upnp_toggle;
#ifdef VINO_ENABLE_LIBUNIQUE
  UniqueApp   *app;
#endif

  DBusGConnection *connection;
  DBusGProxy      *proxy;
  DBusGProxyCall  *call_id;

  SoupSession     *session;
  SoupMessage     *msg;
  gint            port;

  guint        listeners [N_LISTENERS];
  int          n_listeners;
  int          expected_listeners;

  guint        use_password : 1;
  guint        retrieving_info : 1;
} VinoPreferencesDialog;

static void vino_preferences_dialog_update_message_box (VinoPreferencesDialog *dialog);


static char *
vino_preferences_dialog_get_password_from_keyring (VinoPreferencesDialog *dialog)
{
#ifdef VINO_ENABLE_KEYRING
  GnomeKeyringNetworkPasswordData *found_item;
  GnomeKeyringResult               result;
  GList                           *matches;
  char                            *password;
  
  matches = NULL;

  result = gnome_keyring_find_network_password_sync (
                NULL,           /* user     */
		NULL,           /* domain   */
		"vino.local",   /* server   */
		NULL,           /* object   */
		"rfb",          /* protocol */
		"vnc-password", /* authtype */
		5900,           /* port     */
		&matches);

  if (result != GNOME_KEYRING_RESULT_OK || matches == NULL || matches->data == NULL)
    return NULL;


  found_item = (GnomeKeyringNetworkPasswordData *) matches->data;

  password = g_strdup (found_item->password);

  gnome_keyring_network_password_list_free (matches);

  return password;
#else
  return NULL;
#endif
}

static gboolean
vino_preferences_dialog_set_password_in_keyring (VinoPreferencesDialog *dialog,
                                                 const char            *password)
{
#ifdef VINO_ENABLE_KEYRING
  GnomeKeyringResult result;
  guint32            item_id;

  result = gnome_keyring_set_network_password_sync (
                NULL,           /* default keyring */
                NULL,           /* user            */
                NULL,           /* domain          */
                "vino.local",   /* server          */
                NULL,           /* object          */
                "rfb",          /* protocol        */
                "vnc-password", /* authtype        */
                5900,           /* port            */
                password,       /* password        */
                &item_id);

  return result == GNOME_KEYRING_RESULT_OK;
#else
  return FALSE;
#endif
}

static void
vino_preferences_dialog_update_for_allowed (VinoPreferencesDialog *dialog,
					    gboolean               allowed)
{
  gtk_widget_set_sensitive (dialog->view_only_toggle,         allowed);
  gtk_widget_set_sensitive (dialog->prompt_enabled_toggle,    allowed);
  gtk_widget_set_sensitive (dialog->password_toggle,          allowed);
  gtk_widget_set_sensitive (dialog->password_entry,           allowed ? dialog->use_password : FALSE);
  gtk_widget_set_sensitive (dialog->use_upnp_toggle,          allowed);
  gtk_widget_set_sensitive (dialog->icon_always_radio,        allowed);
  gtk_widget_set_sensitive (dialog->icon_client_radio,        allowed);
  gtk_widget_set_sensitive (dialog->icon_never_radio,         allowed);
}

static gboolean
delay_update_message (VinoPreferencesDialog *dialog)
{
  vino_preferences_dialog_update_message_box (dialog);
  return FALSE;
}

static void
vino_preferences_dialog_allowed_toggled (GtkToggleButton       *toggle,
					 VinoPreferencesDialog *dialog)
{
  gboolean allowed;

  allowed = gtk_toggle_button_get_active (toggle);

  gconf_client_set_bool (dialog->client, VINO_PREFS_ENABLED, allowed, NULL);

  vino_preferences_dialog_update_for_allowed (dialog, allowed);
  
  /* ugly: here I delay the GetInfo for 3 seconds, time necessary to server starts */
  if (allowed)
    {
      vino_message_box_show_image (VINO_MESSAGE_BOX (dialog->message));
      vino_message_box_set_label (VINO_MESSAGE_BOX (dialog->message),
                                  _("Checking the connectivity of this machine..."));
      g_timeout_add_seconds (3, (GSourceFunc) delay_update_message, dialog);
    }
  else
    {
      if (dialog->retrieving_info)
	{
	  dialog->retrieving_info = FALSE;
	  soup_session_cancel_message (dialog->session, dialog->msg, 408);
	}
      vino_preferences_dialog_update_message_box (dialog);
    }
}

static void
vino_preferences_dialog_allowed_notify (GConfClient           *client,
					guint                  cnx_id,
					GConfEntry            *entry,
					VinoPreferencesDialog *dialog)
{
  gboolean allowed;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  allowed = gconf_value_get_bool (entry->value) != FALSE;

  if (allowed != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->allowed_toggle)))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->allowed_toggle), allowed);
    }
}

static gboolean
vino_preferences_dialog_setup_allowed_toggle (VinoPreferencesDialog *dialog)
{
  gboolean allowed;

  dialog->allowed_toggle = glade_xml_get_widget (dialog->xml, "allowed_toggle");
  g_assert (dialog->allowed_toggle != NULL);

  allowed = gconf_client_get_bool (dialog->client, VINO_PREFS_ENABLED, NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->allowed_toggle), allowed);

  g_signal_connect (dialog->allowed_toggle, "toggled",
		    G_CALLBACK (vino_preferences_dialog_allowed_toggled), dialog);

  if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_ENABLED, NULL))
    {
      gtk_widget_set_sensitive (dialog->allowed_toggle, FALSE);
      gtk_widget_show (dialog->writability_warning);
    }

  dialog->listeners [dialog->n_listeners] = 
    gconf_client_notify_add (dialog->client,
			     VINO_PREFS_ENABLED,
			     (GConfClientNotifyFunc) vino_preferences_dialog_allowed_notify,
			     dialog, NULL, NULL);
  dialog->n_listeners++;

  return allowed;
}

static void
vino_preferences_dialog_prompt_enabled_toggled (GtkToggleButton       *toggle,
						VinoPreferencesDialog *dialog)
{
  gconf_client_set_bool (dialog->client,
			 VINO_PREFS_PROMPT_ENABLED,
			 gtk_toggle_button_get_active (toggle),
			 NULL);
}

static void
vino_preferences_dialog_prompt_enabled_notify (GConfClient           *client,
					       guint                  cnx_id,
					       GConfEntry            *entry,
					       VinoPreferencesDialog *dialog)
{
  gboolean prompt_enabled;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  prompt_enabled = gconf_value_get_bool (entry->value) != FALSE;

  if (prompt_enabled != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->prompt_enabled_toggle)))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->prompt_enabled_toggle), prompt_enabled);
    }
}

static void
vino_preferences_dialog_setup_prompt_enabled_toggle (VinoPreferencesDialog *dialog)
{
  gboolean prompt_enabled;

  dialog->prompt_enabled_toggle = glade_xml_get_widget (dialog->xml, "prompt_enabled_toggle");
  g_assert (dialog->prompt_enabled_toggle != NULL);

  prompt_enabled = gconf_client_get_bool (dialog->client, VINO_PREFS_PROMPT_ENABLED, NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->prompt_enabled_toggle), prompt_enabled);

  g_signal_connect (dialog->prompt_enabled_toggle, "toggled",
		    G_CALLBACK (vino_preferences_dialog_prompt_enabled_toggled), dialog);

  if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_PROMPT_ENABLED, NULL))
    {
      gtk_widget_set_sensitive (dialog->prompt_enabled_toggle, FALSE);
      gtk_widget_show (dialog->writability_warning);
    }

  dialog->listeners [dialog->n_listeners] = 
    gconf_client_notify_add (dialog->client,
			     VINO_PREFS_PROMPT_ENABLED,
			     (GConfClientNotifyFunc) vino_preferences_dialog_prompt_enabled_notify,
			     dialog, NULL, NULL);
  dialog->n_listeners++;
}

static void
vino_preferences_dialog_view_only_toggled (GtkToggleButton       *toggle,
					   VinoPreferencesDialog *dialog)
{
  gconf_client_set_bool (dialog->client,
			 VINO_PREFS_VIEW_ONLY,
			 !gtk_toggle_button_get_active (toggle),
			 NULL);
}

static void
vino_preferences_dialog_view_only_notify (GConfClient           *client,
					  guint                  cnx_id,
					  GConfEntry            *entry,
					  VinoPreferencesDialog *dialog)
{
  gboolean view_only;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  view_only = gconf_value_get_bool (entry->value) != FALSE;

  if (view_only != !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->view_only_toggle)))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->view_only_toggle), !view_only);
    }
}

static void
vino_preferences_dialog_setup_view_only_toggle (VinoPreferencesDialog *dialog)
{
  gboolean view_only;

  dialog->view_only_toggle = glade_xml_get_widget (dialog->xml, "view_only_toggle");
  g_assert (dialog->view_only_toggle != NULL);

  view_only = gconf_client_get_bool (dialog->client, VINO_PREFS_VIEW_ONLY, NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->view_only_toggle), !view_only);

  g_signal_connect (dialog->view_only_toggle, "toggled",
		    G_CALLBACK (vino_preferences_dialog_view_only_toggled), dialog);

  if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_VIEW_ONLY, NULL))
    {
      gtk_widget_set_sensitive (dialog->view_only_toggle, FALSE);
      gtk_widget_show (dialog->writability_warning);
    }

  dialog->listeners [dialog->n_listeners] = 
    gconf_client_notify_add (dialog->client,
			     VINO_PREFS_VIEW_ONLY,
			     (GConfClientNotifyFunc) vino_preferences_dialog_view_only_notify,
			     dialog, NULL, NULL);
  dialog->n_listeners++;
}

static void
vino_preferences_dialog_icon_visibility_toggled (GtkToggleButton       *toggle,
						 VinoPreferencesDialog *dialog)
{
  gchar *value = "client";

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->icon_always_radio)))
    value = "always";
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->icon_never_radio)))
    value = "never";
  
  gconf_client_set_string (dialog->client,
			   VINO_PREFS_ICON_VISIBILITY,
			   value,
			   NULL);
}

static void
vino_preferences_dialog_icon_visibility_notify (GConfClient           *client,
						guint                  cnx_id,
						GConfEntry            *entry,
						VinoPreferencesDialog *dialog)
{
  const gchar *value;

  if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
    return;

  value = gconf_value_get_string (entry->value);
  if (!g_ascii_strcasecmp (value, "always"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->icon_always_radio), TRUE);
  else if (!g_ascii_strcasecmp (value, "client"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->icon_client_radio), TRUE);
  else if (!g_ascii_strcasecmp (value, "never"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->icon_never_radio), TRUE);
}


static void
vino_preferences_dialog_setup_icon_visibility (VinoPreferencesDialog *dialog)
{
  gchar *value;

  dialog->icon_always_radio = glade_xml_get_widget (dialog->xml, "icon_always_radio");
  g_assert (dialog->icon_always_radio != NULL);
  dialog->icon_client_radio = glade_xml_get_widget (dialog->xml, "icon_client_radio");
  g_assert (dialog->icon_client_radio != NULL);
  dialog->icon_never_radio = glade_xml_get_widget (dialog->xml, "icon_never_radio");
  g_assert (dialog->icon_never_radio != NULL);

  value = gconf_client_get_string (dialog->client, VINO_PREFS_ICON_VISIBILITY, NULL);
  if (!g_ascii_strcasecmp (value, "always"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->icon_always_radio), TRUE);
  else if (!g_ascii_strcasecmp (value, "client"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->icon_client_radio), TRUE);
  else if (!g_ascii_strcasecmp (value, "never"))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->icon_never_radio), TRUE);

  g_signal_connect (dialog->icon_always_radio, "toggled",
		    G_CALLBACK (vino_preferences_dialog_icon_visibility_toggled), dialog);
  g_signal_connect (dialog->icon_client_radio, "toggled",
		    G_CALLBACK (vino_preferences_dialog_icon_visibility_toggled), dialog);
  g_signal_connect (dialog->icon_never_radio, "toggled",
		    G_CALLBACK (vino_preferences_dialog_icon_visibility_toggled), dialog);

  if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_ICON_VISIBILITY, NULL))
    {
      gtk_widget_set_sensitive (dialog->icon_always_radio, FALSE);
      gtk_widget_set_sensitive (dialog->icon_client_radio, FALSE);
      gtk_widget_set_sensitive (dialog->icon_never_radio, FALSE);
      gtk_widget_show (dialog->writability_warning);
    }

  dialog->listeners [dialog->n_listeners] = 
    gconf_client_notify_add (dialog->client,
			     VINO_PREFS_ICON_VISIBILITY,
			     (GConfClientNotifyFunc) vino_preferences_dialog_icon_visibility_notify,
			     dialog, NULL, NULL);
  dialog->n_listeners++;
}

static void
vino_preferences_dialog_use_password_toggled (GtkToggleButton       *toggle,
					      VinoPreferencesDialog *dialog)
{
  GSList *auth_methods = NULL;

  dialog->use_password = gtk_toggle_button_get_active (toggle);

  if (dialog->use_password)
    auth_methods = g_slist_prepend (auth_methods, "vnc");
  else
    auth_methods = g_slist_append (auth_methods, "none");

  gconf_client_set_list (dialog->client,
			 VINO_PREFS_AUTHENTICATION_METHODS,
			 GCONF_VALUE_STRING,
			 auth_methods,
			 NULL);

  g_slist_free (auth_methods);

  gtk_widget_set_sensitive (dialog->password_entry, dialog->use_password);
}

static void
vino_preferences_dialog_use_password_notify (GConfClient           *client,
					     guint                  cnx_id,
					     GConfEntry            *entry,
					     VinoPreferencesDialog *dialog)
{
  GSList   *auth_methods, *l;
  gboolean  use_password;

  if (!entry->value || entry->value->type != GCONF_VALUE_LIST ||
      gconf_value_get_list_type (entry->value) != GCONF_VALUE_STRING)
    return;

  auth_methods = gconf_value_get_list (entry->value);

  use_password = FALSE;
  for (l = auth_methods; l; l = l->next)
    {
      GConfValue *value = l->data;
      const char *method;

      method = gconf_value_get_string (value);

      if (!strcmp (method, "vnc"))
	use_password = TRUE;
    }

  if (use_password != dialog->use_password)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->password_toggle), use_password);
    }
}

static gboolean
vino_preferences_dialog_get_use_password (VinoPreferencesDialog *dialog)
{
  GSList   *auth_methods, *l;
  gboolean  use_password;

  auth_methods = gconf_client_get_list (dialog->client,
					VINO_PREFS_AUTHENTICATION_METHODS,
					GCONF_VALUE_STRING,
					NULL);

  use_password = FALSE;
  for (l = auth_methods; l; l = l->next)
    {
      char *method = l->data;

      if (!strcmp (method, "vnc"))
	use_password = TRUE;

      g_free (method);
    }
  g_slist_free (auth_methods);

  return use_password;
}

static void
vino_preferences_dialog_setup_password_toggle (VinoPreferencesDialog *dialog)
{
  dialog->password_toggle = glade_xml_get_widget (dialog->xml, "password_toggle");
  g_assert (dialog->password_toggle != NULL);

  dialog->use_password = vino_preferences_dialog_get_use_password (dialog);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->password_toggle), dialog->use_password);

  g_signal_connect (dialog->password_toggle, "toggled",
		    G_CALLBACK (vino_preferences_dialog_use_password_toggled), dialog);

  if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_AUTHENTICATION_METHODS, NULL))
    {
      gtk_widget_set_sensitive (dialog->password_toggle, FALSE);
      gtk_widget_show (dialog->writability_warning);
    }

  dialog->listeners [dialog->n_listeners] = 
    gconf_client_notify_add (dialog->client,
			     VINO_PREFS_AUTHENTICATION_METHODS,
			     (GConfClientNotifyFunc) vino_preferences_dialog_use_password_notify,
			     dialog, NULL, NULL);
  dialog->n_listeners++;
}

static void
vino_preferences_vnc_password_notify (GConfClient           *client,
				      guint                  cnx_id,
				      GConfEntry            *entry,
				      VinoPreferencesDialog *dialog)
{
  const char *password_b64;
  guchar     *blob;
  gsize       blob_len;
  char       *password;

  if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
    return;

  password = NULL;

  password_b64 = gconf_value_get_string (entry->value);

  if (password_b64 && *password_b64)
    {
      blob_len = 0;
      blob = g_base64_decode (password_b64, &blob_len);
    
      password = g_strndup ((char *) blob, blob_len);

      g_free (blob);
    }

  if (!password || !password [0])
    {
      gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), "");
    }
  else
    {
      const char *old_password;

      old_password = gtk_entry_get_text (GTK_ENTRY (dialog->password_entry));

      if (!old_password || (old_password && strcmp (old_password, password)))
	{
	  gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), password);
	}
    }

  g_free (password);
}

static void
vino_preferences_dialog_password_changed (GtkEntry              *entry,
					  VinoPreferencesDialog *dialog)
{
  const char *password;

  password = gtk_entry_get_text (entry);

  if (vino_preferences_dialog_set_password_in_keyring (dialog, password))
    return;

  if (!password || !password [0])
    {
      gconf_client_unset (dialog->client, VINO_PREFS_VNC_PASSWORD, NULL);
    }
  else
    {
      char *password_b64;

      password_b64 = g_base64_encode ((guchar *) password, strlen (password));

      gconf_client_set_string (dialog->client, VINO_PREFS_VNC_PASSWORD, password_b64, NULL);

      g_free (password_b64);
    }
}

static void
vino_preferences_dialog_setup_password_entry (VinoPreferencesDialog *dialog)
{
  char     *password;
  gboolean  password_in_keyring;

  dialog->password_entry = glade_xml_get_widget (dialog->xml, "password_entry");
  g_assert (dialog->password_entry != NULL);
  
  password_in_keyring = TRUE;

  if (!(password = vino_preferences_dialog_get_password_from_keyring (dialog)))
    {
      guchar *blob;
      gsize   blob_len;
      char   *password_b64;

      password_b64 = gconf_client_get_string (dialog->client, VINO_PREFS_VNC_PASSWORD, NULL);

      if (password_b64 && *password_b64)
        {
           blob_len = 0;
           blob = g_base64_decode (password_b64, &blob_len);

           password = g_strndup ((char *) blob, blob_len);
        
           g_free (blob);
           g_free (password_b64);
        }

      password_in_keyring = FALSE;
    }

  if (password)
    {
      gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), password);
    }

  g_free (password);

  g_signal_connect (dialog->password_entry, "changed",
		    G_CALLBACK (vino_preferences_dialog_password_changed), dialog);

  gtk_widget_set_sensitive (dialog->password_entry, dialog->use_password);

  if (!password_in_keyring)
    {
      if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_VNC_PASSWORD, NULL))
        {
          gtk_widget_set_sensitive (dialog->password_entry, FALSE);
          gtk_widget_show (dialog->writability_warning);
        }

      dialog->listeners [dialog->n_listeners] = 
        gconf_client_notify_add (dialog->client,
                                 VINO_PREFS_VNC_PASSWORD,
                                 (GConfClientNotifyFunc) vino_preferences_vnc_password_notify,
                                 dialog, NULL, NULL);
      dialog->n_listeners++;
    }
  else
    {
      dialog->expected_listeners--;
    }
}

static void
vino_preferences_dialog_use_upnp_toggled (GtkToggleButton       *toggle,
					  VinoPreferencesDialog *dialog)
{
  gconf_client_set_bool (dialog->client,
			 VINO_PREFS_USE_UPNP,
			 gtk_toggle_button_get_active (toggle),
			 NULL);

  g_timeout_add_seconds (1, (GSourceFunc) delay_update_message, dialog);
}

static void
vino_preferences_dialog_use_upnp_notify (GConfClient           *client,
					 guint                  cnx_id,
					 GConfEntry            *entry,
					 VinoPreferencesDialog *dialog)
{
  gboolean use_upnp;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  use_upnp = gconf_value_get_bool (entry->value) != FALSE;

  if (use_upnp != gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_upnp_toggle)))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->use_upnp_toggle), use_upnp);
    }
}

static void
vino_preferences_dialog_setup_use_upnp_toggle (VinoPreferencesDialog *dialog)
{
  gboolean use_upnp;

  dialog->use_upnp_toggle = glade_xml_get_widget (dialog->xml, "use_upnp_toggle");
  g_assert (dialog->use_upnp_toggle != NULL);

  use_upnp = gconf_client_get_bool (dialog->client, VINO_PREFS_USE_UPNP, NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->use_upnp_toggle), use_upnp);

  g_signal_connect (dialog->use_upnp_toggle, "toggled",
		    G_CALLBACK (vino_preferences_dialog_use_upnp_toggled), dialog);

  if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_USE_UPNP, NULL))
    {
      gtk_widget_set_sensitive (dialog->use_upnp_toggle, FALSE);
      gtk_widget_show (dialog->writability_warning);
    }

  dialog->listeners [dialog->n_listeners] = 
    gconf_client_notify_add (dialog->client,
			     VINO_PREFS_USE_UPNP,
			     (GConfClientNotifyFunc) vino_preferences_dialog_use_upnp_notify,
			     dialog, NULL, NULL);
  dialog->n_listeners++;
}

static void
vino_preferences_dialog_response (GtkWidget             *widget,
				  int                    response,
				  VinoPreferencesDialog *dialog)
{
  GError    *error;
  GdkScreen *screen;

  if (response != GTK_RESPONSE_HELP)
    {
      if (dialog->session)
        soup_session_abort (dialog->session);
      gtk_widget_destroy (widget);
      return;
    }

  screen = gtk_widget_get_screen (widget);
  error = NULL;

  gtk_show_uri (screen, "ghelp:user-guide?goscustdesk-90", GDK_CURRENT_TIME, &error);
  if (error)
    {
      GtkWidget *message_dialog;

      message_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog->dialog),
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
vino_preferences_dialog_destroyed (GtkWidget             *widget,
				   VinoPreferencesDialog *dialog)
{
  dialog->dialog = NULL;

  gtk_main_quit ();
}

static void
error_message (VinoPreferencesDialog *dialog)
{
  gchar   *host, *avahi_host;
  GString *message, *url;

  url = g_string_new (NULL);
  message = g_string_new (_("Your desktop is only reachable over the local network."));

  if (!dbus_g_proxy_call (dialog->proxy,
                          "GetInternalData",
                          NULL,
                          G_TYPE_INVALID,
                          G_TYPE_STRING, &host,
                          G_TYPE_STRING, &avahi_host,
                          G_TYPE_INT, &dialog->port,
                          G_TYPE_INVALID))
    {
      dialog->port = 5900;
      host = g_strdup ("localhost");
      avahi_host = NULL;
    }

  
  g_string_append_printf (url, "<a href=\"vnc://%s::%d\">%s</a>", host, dialog->port, host);

  if (avahi_host && avahi_host[0])
    g_string_append_printf (url, " , <a href=\"vnc://%s::%d\">%s</a>", avahi_host, dialog->port, avahi_host);

  g_string_append_c (message, ' ');
  g_string_append_printf (message, _("Others can access your computer using the address %s."), url->str);
  vino_message_box_hide_image (VINO_MESSAGE_BOX (dialog->message));
  vino_message_box_set_label (VINO_MESSAGE_BOX (dialog->message), message->str);

  g_string_free (message, TRUE);
  g_string_free (url, TRUE);
  g_free (host);
  g_free (avahi_host);
}

static void
got_status (SoupSession *session, SoupMessage *msg, VinoPreferencesDialog *dialog)
{
  gboolean       status;
  GError         *error;
  GHashTable     *hash;
  GHashTableIter iter;
  gpointer       key, value;
  gchar          *ip;

  error = NULL;
  ip = NULL;
  status = FALSE;

  if (soup_xmlrpc_extract_method_response (msg->response_body->data,
					   msg->response_body->length,
					   &error,
					   G_TYPE_HASH_TABLE, &hash,
					   G_TYPE_INVALID))
    {
      g_hash_table_iter_init (&iter, hash);
      while (g_hash_table_iter_next (&iter, &key, &value))
	{
	  if (!strcmp (key, "status"))
	    {
	      status = g_value_get_boolean (value);
	      continue;
	    }
	  if (!strcmp (key, "ip"))
	    {
	      ip = g_strdup (g_value_get_string (value));
	      continue;
	    }
	}
      g_hash_table_destroy (hash);

      if (status)
	{
	  gchar   *avahi_host, *host, *message;
	  gint     port;
	  GString *url;

	  if (!dbus_g_proxy_call (dialog->proxy,
                             "GetInternalData",
                             NULL,
                             G_TYPE_INVALID,
                             G_TYPE_STRING, &host,
                             G_TYPE_STRING, &avahi_host,
                             G_TYPE_INT, &port,
                             G_TYPE_INVALID))
	    {
	      host = NULL;
	      avahi_host = NULL;
	    }

	  url = g_string_new (NULL);
	  g_string_append_printf (url, "<a href=\"vnc://%s::%d\">%s</a>", ip, dialog->port, ip);

	  if (avahi_host && avahi_host[0])
	    g_string_append_printf (url, " , <a href=\"vnc://%s::%d\">%s</a>", avahi_host, port, avahi_host);

	  message = g_strdup_printf (_("Others can access your computer using the address %s."), url->str);
	  vino_message_box_hide_image (VINO_MESSAGE_BOX (dialog->message));
	  vino_message_box_set_label (VINO_MESSAGE_BOX (dialog->message), message);

	  g_free (message);
	  g_string_free (url, TRUE);
	  g_free (host);
	  g_free (avahi_host);
	}
      else
	error_message (dialog);

      g_free (ip);
    }
  else
    {
      if (error)
	{
	  g_warning ("%s", error->message);
	  g_error_free (error);
	}
      error_message (dialog);
    }

  dialog->retrieving_info = FALSE;
}

static gboolean
request_timeout_cb (VinoPreferencesDialog *dialog)
{
  if (!dialog->retrieving_info)
    return FALSE;

  soup_session_cancel_message (dialog->session, dialog->msg, 408);
  return FALSE;
}

#define TIMEOUT 6
static void
vino_preferences_dialog_update_message_box (VinoPreferencesDialog *dialog)
{
  gboolean allowed;
  gchar *url;

  if (dialog->retrieving_info)
    return;
  dialog->retrieving_info = TRUE;

  allowed = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->allowed_toggle));
  if (!allowed)
    {
      vino_message_box_hide_image (VINO_MESSAGE_BOX (dialog->message));
      vino_message_box_set_label (VINO_MESSAGE_BOX (dialog->message),
				  _("Nobody can access your desktop."));
      dialog->retrieving_info = FALSE;
      return;
    }

  url = vino_url_webservice_get_random ();
  if (!url)
    {
      error_message (dialog);
      return;
    }

  vino_message_box_show_image (VINO_MESSAGE_BOX (dialog->message));
  vino_message_box_set_label (VINO_MESSAGE_BOX (dialog->message),
			      _("Checking the connectivity of this machine..."));

  dbus_g_proxy_call (dialog->proxy,
                     "GetExternalPort",
                     NULL,
                     G_TYPE_INVALID,
                     G_TYPE_INT, &dialog->port,
                     G_TYPE_INVALID);

  if (!dialog->session)
    dialog->session = soup_session_async_new ();

  dialog->msg = soup_xmlrpc_request_new (url,
					 "vino.check",
					 G_TYPE_INT, dialog->port,
					 G_TYPE_INT, TIMEOUT,
					 G_TYPE_INVALID);
  soup_session_queue_message (dialog->session,
			      dialog->msg,
			      (SoupSessionCallback)got_status,
			      dialog);

  g_timeout_add_seconds (TIMEOUT+1,
			 (GSourceFunc) request_timeout_cb,
			 dialog);
  g_free (url);
}

static void
vino_preferences_dialog_setup_message_box (VinoPreferencesDialog *dialog)
{
  GtkWidget *event_box;

  event_box = glade_xml_get_widget (dialog->xml, "event_box");
  g_assert (event_box != NULL);

  dialog->message = vino_message_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), dialog->message);
  gtk_widget_show (dialog->message);

  vino_preferences_dialog_update_message_box (dialog);
}

static void
handle_server_info_message (DBusGProxy *proxy, VinoPreferencesDialog *dialog)
{
  vino_preferences_dialog_update_message_box (dialog);
}

static void
vino_preferences_start_listening (VinoPreferencesDialog *dialog)
{
  gchar       *obj_path;
  GdkScreen   *screen;

  screen = gtk_window_get_screen (GTK_WINDOW (dialog->dialog));
  obj_path = g_strdup_printf ("/org/gnome/vino/screens/%d",
                              gdk_screen_get_number (screen));
  dialog->proxy = dbus_g_proxy_new_for_name (dialog->connection,
					     VINO_DBUS_BUS_NAME,
					     obj_path,
					     VINO_DBUS_INTERFACE);

  g_free (obj_path);
  dbus_g_proxy_add_signal (dialog->proxy, "ServerInfoChanged", G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (dialog->proxy,
                               "ServerInfoChanged",
                               G_CALLBACK (handle_server_info_message),
                               dialog,
                               NULL);
}

static gboolean
vino_preferences_dialog_init (VinoPreferencesDialog *dialog)
{
#define VINO_GLADE_FILE "vino-preferences.glade"

  const char *glade_file;
  gboolean    allowed;
  GError     *error = NULL;

  dialog->expected_listeners = N_LISTENERS;

  if (g_file_test (VINO_GLADE_FILE, G_FILE_TEST_EXISTS))
    glade_file = VINO_GLADE_FILE;
  else
    glade_file = VINO_GLADEDIR "/" VINO_GLADE_FILE;

  dialog->xml = glade_xml_new (glade_file, "vino_dialog", NULL);
  if (!dialog->xml)
    {
      g_warning ("Unable to locate glade file '%s'", glade_file);
      return FALSE;
    }

  dialog->dialog = glade_xml_get_widget (dialog->xml, "vino_dialog");
  g_assert (dialog->dialog != NULL);

  g_signal_connect (dialog->dialog, "response",
		    G_CALLBACK (vino_preferences_dialog_response), dialog);
  g_signal_connect (dialog->dialog, "destroy",
		    G_CALLBACK (vino_preferences_dialog_destroyed), dialog);
  g_signal_connect (dialog->dialog, "delete_event", G_CALLBACK (gtk_true), NULL);

  dialog->client = gconf_client_get_default ();
  gconf_client_add_dir (dialog->client, VINO_PREFS_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  dialog->writability_warning = glade_xml_get_widget (dialog->xml, "writability_warning");
  g_assert (dialog->writability_warning != NULL);
  gtk_widget_hide (dialog->writability_warning);

  allowed = vino_preferences_dialog_setup_allowed_toggle (dialog);

  vino_preferences_dialog_setup_view_only_toggle            (dialog);
  vino_preferences_dialog_setup_prompt_enabled_toggle       (dialog);
  vino_preferences_dialog_setup_password_toggle             (dialog);
  vino_preferences_dialog_setup_password_entry              (dialog);
  vino_preferences_dialog_setup_use_upnp_toggle             (dialog);
  vino_preferences_dialog_setup_icon_visibility             (dialog);

  g_assert (dialog->n_listeners == dialog->expected_listeners);

  vino_preferences_dialog_update_for_allowed (dialog, allowed);

  dialog->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (!dialog->connection)
    {
      g_printerr (_("Failed to open connection to bus: %s\n"),
                  error->message);
      g_error_free (error);
      return FALSE;
    }
  vino_preferences_start_listening (dialog);

  vino_preferences_dialog_setup_message_box (dialog);

  gtk_widget_show (dialog->dialog);

#ifdef VINO_ENABLE_LIBUNIQUE
  unique_app_watch_window (dialog->app, GTK_WINDOW (dialog->dialog));
#endif

  return TRUE;

#undef VINO_GLADE_FILE  
}

static void
vino_preferences_dialog_finalize (VinoPreferencesDialog *dialog)
{
  if (dialog->dialog)
    gtk_widget_destroy (dialog->dialog);
  dialog->dialog = NULL;

  if (dialog->client)
    {
      int i;

      for (i = 0; i < dialog->n_listeners; i++)
	{
	  if (dialog->listeners [i])
	    gconf_client_notify_remove (dialog->client, dialog->listeners [i]);
	  dialog->listeners [i] = 0;
	}
      dialog->n_listeners = 0;

      gconf_client_remove_dir (dialog->client, VINO_PREFS_DIR, NULL);

      g_object_unref (dialog->client);
      dialog->client = NULL;
    }

  if (dialog->xml)
    g_object_unref (dialog->xml);
  dialog->xml = NULL;

#ifdef VINO_ENABLE_LIBUNIQUE
  if (dialog->app)
    g_object_unref (dialog->app);
  dialog->app = NULL;
#endif

  if (dialog->session)
    g_object_unref (dialog->session);
  dialog->session = NULL;

  if (dialog->proxy)
    g_object_unref (dialog->proxy);
  dialog->proxy = NULL;

  if (dialog->connection)
    dbus_g_connection_unref (dialog->connection);
  dialog->connection = NULL;
}

static gboolean
vino_preferences_is_running (VinoPreferencesDialog *dialog)
{
#ifdef VINO_ENABLE_LIBUNIQUE
  dialog->app = unique_app_new ("org.gnome.Vino.Preferences", NULL);

  if (unique_app_is_running (dialog->app))
    {
      UniqueResponse response;

      response = unique_app_send_message (dialog->app, UNIQUE_ACTIVATE, NULL);
        
      g_object_unref (dialog->app);
      dialog->app = NULL;
       
      return response == UNIQUE_RESPONSE_OK;
    }
  else
    return FALSE;
#else
  return FALSE;
#endif
}

int
main (int argc, char **argv)
{
  VinoPreferencesDialog dialog = { NULL, };

  bindtextdomain (GETTEXT_PACKAGE, VINO_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  if (vino_preferences_is_running (&dialog))
    return 0;
  
  if (!vino_preferences_dialog_init (&dialog))
    {
      vino_preferences_dialog_finalize (&dialog);
      return 1;
    }

  gtk_main ();

  vino_preferences_dialog_finalize (&dialog);

  return 0;
}
