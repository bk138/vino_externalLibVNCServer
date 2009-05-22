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

#include "vino-prompt.h"

#include <gtk/gtk.h>
#include "vino-util.h"
#include "vino-enums.h"
#include "vino-marshal.h"

struct _VinoPromptPrivate
{
  GdkScreen     *screen;
  GtkWidget     *dialog;
  GtkWidget     *sharing_icon;
  GtkWidget     *host_label;
  rfbClientPtr   current_client;
  GSList        *pending_clients;
};

enum
{
  PROP_0,
  PROP_SCREEN
};

enum
{
  RESPONSE,
  LAST_SIGNAL
};

static gboolean vino_prompt_display (VinoPrompt   *prompt,
				     rfbClientPtr  rfb_client);

static guint prompt_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (VinoPrompt, vino_prompt, G_TYPE_OBJECT);

static void
vino_prompt_finalize (GObject *object)
{
  VinoPrompt *prompt = VINO_PROMPT (object);

  g_slist_free (prompt->priv->pending_clients);
  prompt->priv->pending_clients = NULL;

  if (prompt->priv->dialog)
    gtk_widget_destroy (prompt->priv->dialog);
  prompt->priv->dialog = NULL;
  prompt->priv->sharing_icon = NULL;
  prompt->priv->host_label = NULL;

  g_free (prompt->priv);
  prompt->priv = NULL;

  if (G_OBJECT_CLASS (vino_prompt_parent_class)->finalize)
    G_OBJECT_CLASS (vino_prompt_parent_class)->finalize (object);
}

static void
vino_prompt_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  VinoPrompt *prompt = VINO_PROMPT (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      vino_prompt_set_screen (prompt, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_prompt_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  VinoPrompt *prompt = VINO_PROMPT (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, prompt->priv->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_prompt_init (VinoPrompt *prompt)
{
  prompt->priv = g_new0 (VinoPromptPrivate, 1);
}

static void
vino_prompt_class_init (VinoPromptClass *klass)
{
  GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
  VinoPromptClass *prompt_class  = VINO_PROMPT_CLASS (klass);
  
  gobject_class->finalize     = vino_prompt_finalize;
  gobject_class->set_property = vino_prompt_set_property;
  gobject_class->get_property = vino_prompt_get_property;

  prompt_class->response = NULL;
  
  g_object_class_install_property (gobject_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
							_("Screen"),
							_("The screen on which to display the prompt"),
							GDK_TYPE_SCREEN,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  prompt_signals [RESPONSE] =
    g_signal_new ("response",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VinoPromptClass, response),
                  NULL, NULL,
                  vino_marshal_VOID__POINTER_ENUM,
                  G_TYPE_NONE,
		  2,
		  G_TYPE_POINTER,
		  VINO_TYPE_PROMPT_RESPONSE);

  vino_init_stock_items ();
}

VinoPrompt *
vino_prompt_new (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return g_object_new (VINO_TYPE_PROMPT,
		       "screen", screen,
		       NULL);
}

GdkScreen *
vino_prompt_get_screen (VinoPrompt *prompt)
{
  g_return_val_if_fail (VINO_IS_PROMPT (prompt), NULL);

  return prompt->priv->screen;
}

void
vino_prompt_set_screen (VinoPrompt *prompt,
			GdkScreen  *screen)
{
  g_return_if_fail (VINO_IS_PROMPT (prompt));

  if (prompt->priv->screen != screen)
    {
      prompt->priv->screen = screen;

      g_object_notify (G_OBJECT (prompt), "screen");
    }
}

static void
vino_prompt_process_pending_clients (VinoPrompt *prompt)
{
  if (prompt->priv->pending_clients)
    {
      rfbClientPtr rfb_client = (rfbClientPtr) prompt->priv->pending_clients->data;

      prompt->priv->pending_clients =
	g_slist_delete_link (prompt->priv->pending_clients,
			     prompt->priv->pending_clients);

      vino_prompt_display (prompt, rfb_client);
    }
}

static void
emit_response_signal (VinoPrompt   *prompt,
		      rfbClientPtr  rfb_client,
		      int           response)
{
  dprintf (PROMPT, "Emiting response signal for %p: %s\n",
	   rfb_client,
	   response == VINO_RESPONSE_ACCEPT ? "accept" : "reject");

  g_signal_emit (prompt,
		 prompt_signals [RESPONSE],
		 0,
		 rfb_client,
		 response);
}

static void
vino_prompt_handle_dialog_response (VinoPrompt *prompt,
				    int         response,
				    GtkDialog  *dialog)
{
  rfbClientPtr rfb_client;
  int          prompt_response = VINO_RESPONSE_INVALID;

  dprintf (PROMPT, "Got a response for client %p: %s\n",
	   prompt->priv->current_client,
	   response == GTK_RESPONSE_ACCEPT ? "accept" :
	   response == GTK_RESPONSE_REJECT ? "reject" : "unknown");

  switch (response)
    {
    case GTK_RESPONSE_ACCEPT:
      prompt_response = VINO_RESPONSE_ACCEPT;
      break;
    case GTK_RESPONSE_REJECT:
    default:
      prompt_response = VINO_RESPONSE_REJECT;
      break;
    }

  rfb_client = prompt->priv->current_client;
  prompt->priv->current_client = NULL;

  prompt->priv->dialog = NULL;
  prompt->priv->sharing_icon = NULL;
  prompt->priv->host_label = NULL;
  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (rfb_client != NULL)
    {
      emit_response_signal (prompt, rfb_client, prompt_response);
    }

  vino_prompt_process_pending_clients (prompt);
}

static void
vino_prompt_setup_icons (VinoPrompt *prompt,
			 GtkBuilder *builder)
{
#define ICON_SIZE_STANDARD 48

  prompt->priv->sharing_icon = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                   "sharing_icon"));
  g_assert (prompt->priv->sharing_icon != NULL);

  gtk_window_set_icon_name (GTK_WINDOW (prompt->priv->dialog),
			    "preferences-desktop-remote-desktop");
  gtk_image_set_from_icon_name (GTK_IMAGE (prompt->priv->sharing_icon),
				"preferences-desktop-remote-desktop", GTK_ICON_SIZE_DIALOG);

#undef ICON_SIZE_STANDARD
}

static gboolean
vino_prompt_setup_dialog (VinoPrompt *prompt)
{
#define VINO_UI_FILE "vino-prompt.ui"

  GtkBuilder *builder;
  const char *ui_file;
  GtkWidget  *help_button;
  GError     *error = NULL;

  if (g_file_test (VINO_UI_FILE, G_FILE_TEST_EXISTS))
    ui_file = VINO_UI_FILE;
  else
    ui_file = VINO_UIDIR "/" VINO_UI_FILE;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, ui_file, &error))
  {
    g_warning ("Unable to locate ui file '%s'", ui_file);
    g_error_free (error);
    return FALSE;
  }

  prompt->priv->dialog = GTK_WIDGET (gtk_builder_get_object (builder, "vino_dialog"));
  g_assert (prompt->priv->dialog != NULL);

  g_signal_connect_swapped (prompt->priv->dialog, "response",
			    G_CALLBACK (vino_prompt_handle_dialog_response), prompt);

  vino_prompt_setup_icons (prompt, builder);

  prompt->priv->host_label = GTK_WIDGET (gtk_builder_get_object (builder, "host_label"));
  g_assert (prompt->priv->host_label != NULL);

  help_button = GTK_WIDGET (gtk_builder_get_object (builder, "help_button"));
  g_assert (help_button != NULL);
  gtk_widget_hide (help_button);

  g_object_unref (builder);

  return TRUE;

#undef VINO_UI_FILE
}

static gboolean
vino_prompt_display (VinoPrompt   *prompt,
		     rfbClientPtr  rfb_client)
{
  char *host_label;

  if (prompt->priv->current_client)
    {
      g_assert (prompt->priv->dialog);
      gtk_window_present (GTK_WINDOW (prompt->priv->dialog));
      return prompt->priv->current_client == rfb_client;
    }

  g_assert (prompt->priv->dialog == NULL);

  if (!vino_prompt_setup_dialog (prompt))
    return FALSE;

  host_label = g_strdup_printf (_("A user on the computer '%s' is trying to remotely view or control your desktop."),
				rfb_client->host);

  gtk_label_set_text (GTK_LABEL (prompt->priv->host_label), host_label);

  g_free (host_label);

  prompt->priv->current_client = rfb_client;

  gtk_widget_show_all (prompt->priv->dialog);

  dprintf (PROMPT, "Prompting for client %p\n", rfb_client);

  return TRUE;
}

void
vino_prompt_add_client (VinoPrompt   *prompt,
			rfbClientPtr  rfb_client)
{
  g_return_if_fail (VINO_IS_PROMPT (prompt));
  g_return_if_fail (rfb_client != NULL);

  if (!vino_prompt_display (prompt, rfb_client))
    {
      dprintf (PROMPT, "Prompt in progress for %p: queueing %p\n",
	       prompt->priv->current_client, rfb_client);
      prompt->priv->pending_clients =
	g_slist_append (prompt->priv->pending_clients, rfb_client);
    }
}

void
vino_prompt_remove_client (VinoPrompt   *prompt,
			   rfbClientPtr  rfb_client)
{
  g_return_if_fail (VINO_IS_PROMPT (prompt));
  g_return_if_fail (rfb_client != NULL);

  if (prompt->priv->current_client == rfb_client)
    {
      g_assert (prompt->priv->dialog != NULL);

      gtk_widget_destroy (prompt->priv->dialog);
      prompt->priv->dialog = NULL;
      prompt->priv->current_client = NULL;
    }
  else
    {
      prompt->priv->pending_clients =
	g_slist_remove (prompt->priv->pending_clients, rfb_client);
    }
}
