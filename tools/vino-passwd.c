/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2008 Jorge Pereira <jorge@jorgepereira.com.br>
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
 *      Jorge Pereira <jorge@jorgepereira.com.br>
 */

#include <config.h>

#define _GNU_SOURCE 1

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <unistd.h>
#include <signal.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#ifdef VINO_ENABLE_KEYRING
#include <gnome-keyring.h>
#endif

#define VINO_PREFS_DIR			"/desktop/gnome/remote_access"
#define VINO_PREFS_VNC_PASSWORD		VINO_PREFS_DIR "/vnc_password"
#define VINO_PASSWORD_MAXLEN		8

static gboolean
vino_passwd_set_password_in_keyring (const char *password)
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
vino_passwd_set_password (GConfClient *conf, 
                          const char *password)
{
  gchar *password_b64;

  if (vino_passwd_set_password_in_keyring (password))
    return;

  password_b64 = g_base64_encode ((guchar *) password, strlen (password));
  gconf_client_set_string (conf, VINO_PREFS_VNC_PASSWORD, password_b64, NULL);
  g_free (password_b64);
}

static void
vino_passwd_read (char *buff, 
                  const size_t len,
                  const char *prompt_msg)
{
  struct termios told, tnew;
  size_t pos = 0;
  gboolean again = FALSE;

  tcgetattr (STDIN_FILENO, &told);
  tnew = told;
  tnew.c_lflag &= ~(ICANON | ECHO | ISIG);
  tnew.c_iflag &= ~(IXON | IXOFF);
  tcsetattr (STDIN_FILENO, TCSAFLUSH, &tnew);

  g_print ("%s", prompt_msg);

  do
    {
      char key;

      if ( read (STDIN_FILENO, &key, 1) < 0 )
	break;

      if (key == 0x03) /* Ctrl + C */
        {
          tcsetattr (STDIN_FILENO, TCSAFLUSH, &told); 
          g_printerr ("\n");
          g_printerr (_("Cancelled"));
          g_printerr ("\n");
          exit (1);
        }
      else if (again && key == '\n')
        {
          g_printerr ("\n");
          g_printerr (ngettext ("ERROR: Maximum length of password is %d character. Please, re-enter the password.",
                                "ERROR: Maximum length of password is %d characters. Please, re-enter the password.",
                                VINO_PASSWORD_MAXLEN),
                      VINO_PASSWORD_MAXLEN);
          g_printerr ("\n");
          g_print ("%s", prompt_msg);

          pos   = 0;
          again = FALSE;
        }
      else if (pos > 0 && key == '\n')
        break;
      else if (pos > 0 && key == 0x7f) /* 0x7f - <BACKSPACE> */
        pos--;
      else if (key == '\n' || key == 0x7f)
        continue;
      else if (pos >= len -1)
          again = TRUE;
      else
        buff[pos++] = key;
    } 
  while (TRUE);
  buff[pos] = '\0';

  tcsetattr (STDIN_FILENO, TCSAFLUSH, &told); 
  fflush (stdout);
}

static void
vino_passwd_change (GConfClient *conf)
{
  gchar password1[VINO_PASSWORD_MAXLEN + 1];
  gchar password2[VINO_PASSWORD_MAXLEN + 1];

  g_print (_("Changing Vino password.\n"));

  vino_passwd_read (password1, sizeof(password1), _("Enter new Vino password: "));
  g_print ("\n");

  vino_passwd_read (password2, sizeof(password2), _("Retype new Vino password: "));
  g_print ("\n");

  if (g_str_equal (password1, password2))
    {
      vino_passwd_set_password (conf, password1);
      g_print (_("vino-passwd: password updated successfully.\n"));
    }
  else
    {
      g_printerr (_("Sorry, passwords do not match.\n"));
      g_printerr (_("vino-passwd: password unchanged.\n"));
    }
}

int 
main(int argc, char *argv[])
{
  gboolean       opt_version = FALSE;
  GError         *error      = NULL;
  GConfClient    *conf       = NULL;
  GOptionContext *context    = NULL;  
  
  const GOptionEntry entries[] = 
    { 
      { 
        "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, _("Show Vino version"), NULL 
      },
      { NULL }
    };

  bindtextdomain (GETTEXT_PACKAGE, VINO_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_option_context_new (_("- Updates Vino password"));    
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_help_enabled (context, TRUE);
  g_option_context_parse (context, &argc, &argv, &error);
  g_option_context_free (context);

  if (error)
    {
      g_print ("%s\n%s\n",
	       error->message,
	       _("Run 'vino-passwd --help' to see a full list of available command line options"));
      g_error_free (error);
      return 1;
    }

  if (opt_version)
   {
     g_print (_("VINO Version %s\n"), VERSION);
     return 0;
   }

  conf = gconf_client_get_default ();

  if (gconf_client_key_is_writable (conf, VINO_PREFS_VNC_PASSWORD, NULL))
    vino_passwd_change (conf);
  else
    g_printerr (_("ERROR: You do not have enough permissions to change Vino password.\n"));

  g_object_unref (conf);

  return 0;
}
