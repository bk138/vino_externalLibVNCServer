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

#include "vino-shell.h"

#include <string.h>
#include <gdk/gdkdisplay.h>
#include <gtk/gtkmain.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include "GNOME_RemoteDesktop.h"
#include "vino-util.h"

#define VINO_SERVER_IID "OAFIID:GNOME_RemoteDesktopServer"

static GType vino_shell_get_type (void) G_GNUC_CONST;

gboolean
vino_shell_register (int   *argc,
		     char **argv)
{
  GObject                   *shell;
  Bonobo_RegistrationResult  result;
  GSList                    *reg_env = NULL;
  char                      *display_name, *p;

  bonobo_init (argc, argv);

  shell = g_object_new (vino_shell_get_type (), NULL);

  display_name = g_strdup (gdk_display_get_name (gdk_display_get_default ()));
  
  /* Strip off the screen portion of the display */
  p = strrchr (display_name, ':');
  if (p)
    {
      p = strchr (p, '.');
      if (p)
	p [0] = '\0';
    }

  reg_env = bonobo_activation_registration_env_set (reg_env, "DISPLAY", display_name);
  result = bonobo_activation_register_active_server (VINO_SERVER_IID,
						     BONOBO_OBJREF (shell),
						     reg_env);

  bonobo_activation_registration_env_free (reg_env);
  g_free (display_name);

  switch (result)
    {
    case Bonobo_ACTIVATION_REG_SUCCESS:
      break;
    case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
      g_warning (_("Remote Desktop server already running; exiting ...\n"));
      goto registration_failed;
    default:
      g_warning (_("Problem registering the remote desktop server with bonobo-activation; exiting ...\n"));
      goto registration_failed;
    }

  return TRUE;

 registration_failed:
  bonobo_object_unref (BONOBO_OBJECT (shell));

  return FALSE;
}

typedef BonoboObject VinoShell;

typedef struct
{
  BonoboObjectClass                  parent_class;

  POA_GNOME_RemoteDesktop_Shell__epv epv;
} VinoShellClass;

static GObjectClass *vino_shell_parent_class = NULL;

static gboolean
vino_shell_idle_quit (void)
{
  if (gtk_main_level ())
    gtk_main_quit ();

  return FALSE;
}

static void
vino_shell_destroy (BonoboObject *bobject)
{
  g_idle_add ((GSourceFunc) vino_shell_idle_quit, NULL);
  
  BONOBO_OBJECT_CLASS (vino_shell_parent_class)->destroy (bobject);
}

static void
vino_shell_class_init (VinoShellClass *klass)
{
  BonoboObjectClass *bobject_class = (BonoboObjectClass *) klass;

  bobject_class->destroy = vino_shell_destroy;

  vino_shell_parent_class = g_type_class_peek_parent (klass);
}

static void
vino_shell_init (VinoShell *shell)
{
}

BONOBO_TYPE_FUNC_FULL (VinoShell,
                       GNOME_RemoteDesktop_Shell,
                       BONOBO_TYPE_OBJECT,
                       vino_shell);
