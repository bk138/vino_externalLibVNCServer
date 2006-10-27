/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 */

#include <config.h>

#include <string.h>
#include <libintl.h>
#include <unistd.h>
#include <netdb.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include "vino-url.h"

#ifdef VINO_ENABLE_KEYRING
#include <gnome-keyring.h>
#endif

#define VINO_PREFS_DIR                    "/desktop/gnome/remote_access"
#define VINO_PREFS_ENABLED                VINO_PREFS_DIR "/enabled"
#define VINO_PREFS_PROMPT_ENABLED         VINO_PREFS_DIR "/prompt_enabled"
#define VINO_PREFS_VIEW_ONLY              VINO_PREFS_DIR "/view_only"
#define VINO_PREFS_AUTHENTICATION_METHODS VINO_PREFS_DIR "/authentication_methods"
#define VINO_PREFS_VNC_PASSWORD           VINO_PREFS_DIR "/vnc_password"
#define VINO_PREFS_MAILTO                 VINO_PREFS_DIR "/mailto"

#define N_LISTENERS 6

typedef struct {
  GladeXML    *xml;
  GConfClient *client;
  char        *mailto;

  GtkWidget   *dialog;
  GtkWidget   *writability_warning;
  GtkWidget   *sharing_icon;
  GtkWidget   *security_icon;
  GtkWidget   *url_labels_box;
  GtkWidget   *url_label;
  GtkWidget   *allowed_toggle;
  GtkWidget   *prompt_enabled_toggle;
  GtkWidget   *view_only_toggle;
  GtkWidget   *password_toggle;
  GtkWidget   *password_box;
  GtkWidget   *password_entry;

  guint        listeners [N_LISTENERS];
  int          n_listeners;
  int          expected_listeners;

  guint        use_password : 1;
} VinoPreferencesDialog;

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

  if (result != GNOME_KEYRING_RESULT_OK)
    return NULL;

  g_assert (matches != NULL && matches->data != NULL);

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
  gtk_widget_set_sensitive (dialog->prompt_enabled_toggle, allowed);
  gtk_widget_set_sensitive (dialog->view_only_toggle,      allowed);
  gtk_widget_set_sensitive (dialog->url_labels_box,        allowed);
  gtk_widget_set_sensitive (dialog->password_toggle,       allowed);
  gtk_widget_set_sensitive (dialog->password_box,          allowed ? dialog->use_password : FALSE);
}

static void
vino_preferences_dialog_allowed_toggled (GtkToggleButton       *toggle,
					 VinoPreferencesDialog *dialog)
{
  gboolean allowed;

  allowed = gtk_toggle_button_get_active (toggle);

  gconf_client_set_bool (dialog->client, VINO_PREFS_ENABLED, allowed, NULL);

  vino_preferences_dialog_update_for_allowed (dialog, allowed);
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

  gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (dialog->allowed_toggle)->child), TRUE);

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

  gtk_widget_set_sensitive (dialog->password_box, dialog->use_password);
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

  password_b64 = gconf_value_get_string (entry->value);

  blob_len = 0;
  blob = g_base64_decode (password_b64, &blob_len);

  password = g_strndup ((char *) blob, blob_len);

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
  g_free (blob);
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
  
  dialog->password_box = glade_xml_get_widget (dialog->xml, "password_box");
  g_assert (dialog->password_box != NULL);

  password_in_keyring = TRUE;

  if (!(password = vino_preferences_dialog_get_password_from_keyring (dialog)))
    {
      guchar *blob;
      gsize   blob_len;
      char   *password_b64;

      password_b64 = gconf_client_get_string (dialog->client, VINO_PREFS_VNC_PASSWORD, NULL);

      blob_len = 0;
      blob = g_base64_decode (password_b64, &blob_len);

      password = g_strndup ((char *) blob, blob_len);

      g_free (blob);
      g_free (password_b64);

      password_in_keyring = FALSE;
    }

  if (password)
    {
      gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), password);
    }

  g_free (password);

  g_signal_connect (dialog->password_entry, "changed",
		    G_CALLBACK (vino_preferences_dialog_password_changed), dialog);

  gtk_widget_set_sensitive (dialog->password_box, dialog->use_password);

  if (!password_in_keyring)
    {
      if (!gconf_client_key_is_writable (dialog->client, VINO_PREFS_VNC_PASSWORD, NULL))
        {
          gtk_widget_set_sensitive (dialog->password_box, FALSE);
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
vino_preferences_dialog_setup_icons (VinoPreferencesDialog *dialog)
{
  gtk_window_set_icon_name (GTK_WINDOW (dialog->dialog), "gnome-remote-desktop");

  dialog->sharing_icon = glade_xml_get_widget (dialog->xml, "sharing_icon");
  g_assert (dialog->sharing_icon != NULL);
  
  gtk_image_set_from_icon_name (GTK_IMAGE (dialog->sharing_icon),
                                "gnome-remote-desktop",
                                GTK_ICON_SIZE_DIALOG);

  dialog->security_icon = glade_xml_get_widget (dialog->xml, "security_icon");
  g_assert (dialog->security_icon != NULL);

  gtk_image_set_from_icon_name (GTK_IMAGE (dialog->security_icon),
                                "gnome-lockscreen",
                                GTK_ICON_SIZE_DIALOG);
}

static char *
vino_preferences_get_local_hostname (void)
{
  static char      local_host [NI_MAXHOST] = { 0, };
  struct addrinfo  hints;
  struct addrinfo *results;
  char            *retval;

  if (gethostname (local_host, NI_MAXHOST) == -1)
    return NULL;

  memset (&hints, 0, sizeof (hints));
  hints.ai_flags = AI_CANONNAME;

  results = NULL;
  if (getaddrinfo (local_host,  NULL, &hints, &results) != 0)
    return NULL;

  retval = g_strdup (results ? results->ai_canonname : local_host);

  if (results)
    freeaddrinfo (results);

  return retval;
}

static char *
vino_preferences_dialog_get_server_command (VinoPreferencesDialog *dialog)
{
  char *local_host;
  char *server_url;

  local_host = vino_preferences_get_local_hostname ();
  if (!local_host)
    {
      return g_strdup ("vncviewer localhost:0");
    }

  /* FIXME: get the actual port number for the server on this screen */
  server_url = g_strdup_printf ("vncviewer %s:0", local_host);

  g_free (local_host);

  return server_url;
}

static char *
vino_preferences_dialog_construct_mailto (VinoPreferencesDialog *dialog,
					  const char            *url)
{
  GString *mailto;

  mailto = g_string_new ("mailto:");
  if (dialog->mailto)
    mailto = g_string_append (mailto, dialog->mailto);

  mailto = g_string_append_c (mailto, '?');
  g_string_append_printf (mailto, "Body=%s", url);

  return g_string_free (mailto, FALSE);
}

static void
vino_preferences_dialog_update_url_label (VinoPreferencesDialog *dialog)
{
  char *command;
  char *mailto;

  command = vino_preferences_dialog_get_server_command (dialog);
  mailto = vino_preferences_dialog_construct_mailto (dialog, command);

  gtk_label_set_text (GTK_LABEL (dialog->url_label), command);
  vino_url_set_address (VINO_URL (dialog->url_label), mailto);
  
  g_free (command);
  g_free (mailto);
}

static void
vino_preferences_dialog_mailto_notify (GConfClient           *client,
				       guint                  cnx_id,
				       GConfEntry            *entry,
				       VinoPreferencesDialog *dialog)
{
  const char *mailto;

  if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
    return;

  mailto = gconf_value_get_string (entry->value);

  if (!mailto || mailto [0] == '\0' || mailto [0] == ' ')
    {
      g_free (dialog->mailto);
      dialog->mailto = NULL;
      vino_preferences_dialog_update_url_label (dialog);
    }
  else if (!dialog->mailto || (dialog->mailto && strcmp (dialog->mailto, mailto)))
    {
      g_free (dialog->mailto);
      dialog->mailto = g_strdup (mailto);
      vino_preferences_dialog_update_url_label (dialog);
    }
}

static void
vino_preferences_dialog_setup_url_labels (VinoPreferencesDialog *dialog)
{
  char *command;
  char *mailto;

  dialog->url_labels_box = glade_xml_get_widget (dialog->xml, "url_labels_box");
  g_assert (dialog->url_labels_box);

  dialog->listeners [dialog->n_listeners] = 
    gconf_client_notify_add (dialog->client,
			     VINO_PREFS_MAILTO,
			     (GConfClientNotifyFunc) vino_preferences_dialog_mailto_notify,
			     dialog, NULL, NULL);
  dialog->n_listeners++;
  
  dialog->mailto = gconf_client_get_string (dialog->client, VINO_PREFS_MAILTO, NULL);
  if (!dialog->mailto || dialog->mailto [0] == '\0'|| dialog->mailto [0] == ' ')
    {
      g_free (dialog->mailto);
      dialog->mailto = NULL;
    }

  command = vino_preferences_dialog_get_server_command (dialog);
  mailto = vino_preferences_dialog_construct_mailto (dialog, command);
  
  dialog->url_label = vino_url_new (mailto, command,
				    _("Send this command by email"));
  g_free (command);
  g_free (mailto);

  gtk_misc_set_alignment (GTK_MISC (dialog->url_label), 0.0, 0.0);

  gtk_box_pack_start (GTK_BOX (dialog->url_labels_box),
		      dialog->url_label,
		      FALSE, FALSE, 0);
  gtk_widget_show (dialog->url_label);
}

static void
vino_preferences_dialog_response (GtkWidget             *widget,
				  int                    response,
				  VinoPreferencesDialog *dialog)
{
  GError *error;

  if (response != GTK_RESPONSE_HELP)
    {
      gtk_widget_destroy (widget);
      return;
    }

  error = NULL;
  gnome_help_display_desktop (NULL, "user-guide", "user-guide.xml", "goscustdesk-90", &error);
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

static gboolean
vino_preferences_dialog_init (VinoPreferencesDialog *dialog)
{
#define VINO_GLADE_FILE "vino-preferences.glade"

  const char *glade_file;
  gboolean    allowed;

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

  vino_preferences_dialog_setup_icons (dialog);

  g_signal_connect (dialog->dialog, "response",
		    G_CALLBACK (vino_preferences_dialog_response), dialog);
  g_signal_connect (dialog->dialog, "destroy",
		    G_CALLBACK (vino_preferences_dialog_destroyed), dialog);
  g_signal_connect (dialog->dialog, "delete_event", G_CALLBACK (gtk_true), NULL);

  dialog->client = gconf_client_get_default ();
  gconf_client_add_dir (dialog->client, VINO_PREFS_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  vino_preferences_dialog_setup_url_labels (dialog);

  dialog->writability_warning = glade_xml_get_widget (dialog->xml, "writability_warning");
  g_assert (dialog->writability_warning != NULL);
  gtk_widget_hide (dialog->writability_warning);

  allowed = vino_preferences_dialog_setup_allowed_toggle (dialog);

  vino_preferences_dialog_setup_prompt_enabled_toggle (dialog);
  vino_preferences_dialog_setup_view_only_toggle      (dialog);
  vino_preferences_dialog_setup_password_toggle       (dialog);
  vino_preferences_dialog_setup_password_entry        (dialog);

  g_assert (dialog->n_listeners == dialog->expected_listeners);

  vino_preferences_dialog_update_for_allowed (dialog, allowed);

  gtk_widget_show (dialog->dialog);

  return TRUE;

#undef VINO_GLADE_FILE  
}

static void
vino_preferences_dialog_finalize (VinoPreferencesDialog *dialog)
{
  if (dialog->dialog)
    gtk_widget_destroy (dialog->dialog);
  dialog->dialog = NULL;

  if (dialog->mailto)
    g_free (dialog->mailto);
  dialog->mailto = NULL;

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
}

int
main (int argc, char **argv)
{
  VinoPreferencesDialog dialog = { NULL, };

  bindtextdomain (GETTEXT_PACKAGE, VINO_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
		      argc, argv, NULL);

  if (!vino_preferences_dialog_init (&dialog))
    {
      vino_preferences_dialog_finalize (&dialog);
      return 1;
    }

  gtk_main ();

  vino_preferences_dialog_finalize (&dialog);

  return 0;
}
