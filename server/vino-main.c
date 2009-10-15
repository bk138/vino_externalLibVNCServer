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
 *      Jonh Wendell <wendell@bani.com.br>
 */

#include <config.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h>

#include "vino-input.h"
#include "vino-mdns.h"
#include "vino-server.h"
#include "vino-prefs.h"
#include "vino-util.h"
#include "vino-dbus-listener.h"
#include "eggsmclient.h"

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>

#ifdef G_ENABLE_DEBUG
static void
vino_debug_gnutls (int         level,
		   const char *str)
{
  fputs (str, stderr);
}
#endif
#endif /* HAVE_GNUTLS */

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GdkDisplay     *display;
  gboolean        view_only;
  int             i, n_screens;
  GError         *error = NULL;
  EggSMClient    *client;

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, VINO_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_option_context_new (_("- VNC Server for GNOME"));
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  g_option_context_add_group (context, egg_sm_client_get_option_group ());
  g_option_context_parse (context, &argc, &argv, &error);
  if (error)
    {
      g_print ("%s\n%s\n",
	       error->message,
	       _("Run 'vino-server --help' to see a full list of available command line options"));
      g_error_free (error);
      return 1;
    }

  client = egg_sm_client_get ();
  egg_sm_client_set_mode (EGG_SM_CLIENT_MODE_NO_RESTART);
  g_signal_connect (client, "quit",
		    G_CALLBACK (gtk_main_quit), NULL);

  vino_setup_debug_flags ();

#ifdef HAVE_GNUTLS
#ifdef G_ENABLE_DEBUG
  if (_vino_debug_flags & VINO_DEBUG_TLS)
    {
      gnutls_global_set_log_level (10);
      gnutls_global_set_log_function (vino_debug_gnutls);
    }
#endif
#endif /* HAVE_GNUTLS */

  gtk_window_set_default_icon_name ("preferences-desktop-remote-desktop");
  g_set_application_name (_("GNOME Remote Desktop"));

  if (!vino_dbus_request_name ())
    return 1;

  display = gdk_display_get_default ();

  view_only = FALSE;
  if (!vino_input_init (display))
    {
      g_warning (_("Your XServer does not support the XTest extension - "
		   "remote desktop access will be view-only\n"));
      view_only = TRUE;
    }

  vino_prefs_init (view_only);

  n_screens = gdk_display_get_n_screens (display);
  for (i = 0; i < n_screens; i++)
    vino_prefs_create_server (gdk_display_get_screen (display, i));

  gtk_main ();

  vino_mdns_shutdown ();

  vino_prefs_shutdown ();

  return 0;
}
