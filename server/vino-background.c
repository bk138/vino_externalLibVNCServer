/*
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

#include <stdlib.h>
#include <gconf/gconf-client.h>
#include "vino-background.h"

#define VINO_PREFS_DISABLE_BACKGROUND   "/desktop/gnome/remote_access/disable_background"
#define GNOME_BACKGROUND_DRAW           "/desktop/gnome/background/draw_background"

void 
vino_background_handler (int sig)
{
  GConfClient *client = gconf_client_get_default ();
  
  if (gconf_client_get_bool (client, VINO_PREFS_DISABLE_BACKGROUND, NULL) &&
      !gconf_client_get_bool (client, GNOME_BACKGROUND_DRAW, NULL))
    gconf_client_set_bool (client, GNOME_BACKGROUND_DRAW, TRUE, NULL);

  g_object_unref (client);
  exit (0);
}

void 
vino_background_draw (gboolean option)
{
  GConfClient *client = gconf_client_get_default ();

  if (option && !gconf_client_get_bool (client, GNOME_BACKGROUND_DRAW, NULL))
    gconf_client_set_bool (client, GNOME_BACKGROUND_DRAW, TRUE, NULL);
  else if (!option && gconf_client_get_bool (client, GNOME_BACKGROUND_DRAW, NULL))
         gconf_client_set_bool (client, GNOME_BACKGROUND_DRAW, FALSE, NULL);

  g_object_unref (client);
}

