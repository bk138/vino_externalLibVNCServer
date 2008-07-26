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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libbonobo.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <gdk/gdkdisplay.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-ui-init.h>

#define REMOTE_DESKTOP_DIR "/desktop/gnome/remote_access"
#define REMOTE_DESKTOP_KEY REMOTE_DESKTOP_DIR "/enabled"
#define REMOTE_DESKTOP_IID "OAFIID:GNOME_RemoteDesktopServer"

#ifdef G_ENABLE_DEBUG
static gboolean verbose_debug = FALSE;

#ifdef G_HAVE_ISO_VARARGS
#  define dprintf(...) G_STMT_START {          \
        if (verbose_debug)                     \
                fprintf (stderr, __VA_ARGS__); \
        } G_STMT_END
#elif defined(G_HAVE_GNUC_VARARGS)
#  define dprintf(args...) G_STMT_START {      \
        if (verbose_debug)                     \
                fprintf (stderr, args);        \
        } G_STMT_END
#endif

#else /* if !defined (G_ENABLE_DEBUG) */

#ifdef G_HAVE_ISO_VARARGS
#  define dprintf(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#  define dprintf(args...)
#endif
#endif /* G_ENABLE_DEBUG */

typedef struct
{
  Bonobo_Unknown obj;
  gboolean       activating;
  time_t         start_time;
  guint          attempts;
  guint          shutdown_timeout;

  GConfClient   *client;
  gboolean       enabled;
  guint          listener;
} RemoteDesktopData;

static void remote_desktop_restart (RemoteDesktopData *data);

static gboolean
remote_desktop_shutdown_timeout (RemoteDesktopData *data)
{
  if (data->activating)
    {
      dprintf ("Remote desktop server activating, delaying shutdown\n");
      return TRUE;
    }

  dprintf ("Shutting down remote desktop server\n");

  if (data->obj != CORBA_OBJECT_NIL)
    bonobo_object_release_unref (data->obj, NULL);
  data->obj = CORBA_OBJECT_NIL;
    
  data->shutdown_timeout = 0;

  return FALSE;
}

static void
remote_desktop_shutdown (RemoteDesktopData *data)
{
  if (!data->activating && data->obj == CORBA_OBJECT_NIL)
    return;

  if (data->shutdown_timeout)
    return;

  dprintf ("Shutting down remote desktop server in 30 seconds\n");

  data->shutdown_timeout = g_timeout_add (30 * 1000,
					  (GSourceFunc) remote_desktop_shutdown_timeout,
					  data);
}

static void
remote_desktop_cnx_broken (ORBitConnection   *cnx,
			   RemoteDesktopData *data)
{
  g_return_if_fail (data->obj != CORBA_OBJECT_NIL);
  g_return_if_fail (data->activating == FALSE);

  data->obj = CORBA_OBJECT_NIL;

  if (data->shutdown_timeout)
    {
      g_source_remove (data->shutdown_timeout);
      data->shutdown_timeout = 0;
      return;
    }

  if (data->enabled)
    {
      g_warning (_("Remote desktop server died, restarting\n"));

      remote_desktop_restart (data);
    }
}

static void
remote_desktop_obj_activated (Bonobo_Unknown     object,
			      CORBA_Environment *ev,
			      RemoteDesktopData *data)
{
  g_return_if_fail (data->obj == CORBA_OBJECT_NIL);
  g_return_if_fail (data->activating == TRUE);

  data->activating = FALSE;
  data->obj        = object;

  if (object == CORBA_OBJECT_NIL)
    {
      if (BONOBO_EX (ev))
	{
	  g_warning (_("Activation of %s failed: %s\n"),
		     REMOTE_DESKTOP_IID,
		     bonobo_exception_general_error_get (ev));
	}
      else
	{
	  g_warning (_("Activation of %s failed: Unknown Error\n"),
		     REMOTE_DESKTOP_IID);
	}

      if (data->enabled)
	remote_desktop_restart (data);
      return;
    }

  dprintf ("Activated %s; object: %p\n", REMOTE_DESKTOP_IID, object);

  ORBit_small_listen_for_broken (object,
				 G_CALLBACK (remote_desktop_cnx_broken),
				 data);
}

/* We don't want bonobo-activation's behaviour of using
 * the value of $DISPLAY since this may contain the
 * screen number.
 *
 * FIXME: should this be the default behaviour for
 *        bonobo-activation too ?
 */
static inline void
setup_per_display_activation (void)
{
  char *display_name, *p;

  display_name = g_strdup (gdk_display_get_name (gdk_display_get_default ()));

  /* Strip off the screen portion of the display */
  p = strrchr (display_name, ':');
  if (p)
    {
      p = strchr (p, '.');
      if (p)
        p [0] = '\0';
    }

  bonobo_activation_set_activation_env_value ("DISPLAY", display_name);

  g_free (display_name);
}

static void
remote_desktop_restart (RemoteDesktopData *data)
{
  CORBA_Environment ev;
  time_t            now;

  if (data->shutdown_timeout)
    {
      dprintf ("Cancelling remote desktop server shutdown\n");
      g_source_remove (data->shutdown_timeout);
      data->shutdown_timeout = 0;
    }

  if (data->activating || data->obj != CORBA_OBJECT_NIL)
    return;
  
  dprintf ("Activating remote desktop server: %s\n", REMOTE_DESKTOP_IID);

  now = time (NULL);
  if (now > data->start_time + 120)
    {
      data->attempts   = 0;
      data->start_time = now;
    }

  if (data->attempts++ > 10)
    {
      g_warning (_("Failed to activate remote desktop server: tried too many times\n"));
      return;
    }

  data->activating = TRUE;

  CORBA_exception_init (&ev);
  
  setup_per_display_activation ();

  bonobo_get_object_async (REMOTE_DESKTOP_IID,
			   "IDL:Bonobo/Unknown:1.0",
			   &ev,
			   (BonoboMonikerAsyncFn) remote_desktop_obj_activated,
			   data);

  if (BONOBO_EX (&ev))
    {
      g_warning ("Activation of %s failed: %s\n",
		 REMOTE_DESKTOP_IID,
		 bonobo_exception_general_error_get (&ev));
    }

  CORBA_exception_free (&ev);
}

static void
remote_desktop_enabled_notify (GConfClient       *client,
			       guint              cnx_id,
			       GConfEntry        *entry,
			       RemoteDesktopData *data)
{
  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  data->enabled = gconf_value_get_bool (entry->value);

  if (data->enabled)
    remote_desktop_restart (data);
  else
    remote_desktop_shutdown (data);
}

static void
remote_desktop_start (RemoteDesktopData *data)
{
  data->client = gconf_client_get_default ();

  data->enabled = gconf_client_get_bool (data->client,
					 REMOTE_DESKTOP_KEY,
					 NULL);

  gconf_client_add_dir (data->client,
			REMOTE_DESKTOP_DIR,
			GCONF_CLIENT_PRELOAD_NONE,
			NULL);

  data->listener = gconf_client_notify_add (data->client,
					    REMOTE_DESKTOP_KEY,
					    (GConfClientNotifyFunc) remote_desktop_enabled_notify,
					    data, NULL, NULL);

  if (data->enabled)
    {
      g_message (_("Starting remote desktop server"));
      remote_desktop_restart (data);
    }
  else
    {
      g_message (_("Not starting remote desktop server"));
    }
}

static void
remote_desktop_cleanup (RemoteDesktopData *data)
{
  if (data->shutdown_timeout)
    g_source_remove (data->shutdown_timeout);
  data->shutdown_timeout = 0;

  if (data->obj)
    ORBit_small_unlisten_for_broken (data->obj,
				     G_CALLBACK (remote_desktop_cnx_broken));

  remote_desktop_shutdown_timeout (data);

  gconf_client_notify_remove (data->client, data->listener);
  data->listener = 0;

  g_object_unref (data->client);
  data->client = NULL;
}

int
main (int argc, char **argv)
{
  static RemoteDesktopData  remote_desktop_data = { NULL, };
  GnomeClient              *session;
  char                     *restart_argv [] = { *argv, 0 };

#ifdef G_ENABLE_DEBUG
  verbose_debug = g_getenv ("VINO_SESSION_DEBUG") != NULL;
#endif

  bindtextdomain (GETTEXT_PACKAGE, VINO_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("vino-session", VERSION,
                      LIBGNOMEUI_MODULE,
                      argc, argv,
                      NULL);

  session = gnome_master_client ();

  gnome_client_set_restart_command (session, 2, restart_argv);
  gnome_client_set_restart_style   (session, GNOME_RESTART_IMMEDIATELY);
  gnome_client_set_priority        (session, 5);
  g_signal_connect (session, "die",
                    G_CALLBACK (gtk_main_quit), NULL);

  remote_desktop_start (&remote_desktop_data);

  gtk_main ();

  remote_desktop_cleanup (&remote_desktop_data);

  return 0;
}
