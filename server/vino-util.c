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

#include "vino-util.h"
#include <gtk/gtk.h>
#include <string.h>

#ifdef G_ENABLE_DEBUG
VinoDebugFlags _vino_debug_flags = VINO_DEBUG_NONE;

void
vino_setup_debug_flags (void)
{
  const char       *env_str;
  static GDebugKey  debug_keys [] =
    {
      { "polling", VINO_DEBUG_POLLING },
      { "rfb",     VINO_DEBUG_RFB },
      { "input",   VINO_DEBUG_INPUT },
      { "prefs",   VINO_DEBUG_PREFS },
      { "tls",     VINO_DEBUG_TLS },
      { "prompt",  VINO_DEBUG_PROMPT },
      { "http",    VINO_DEBUG_HTTP}
    };
  
  env_str = g_getenv ("VINO_SERVER_DEBUG");
  
  if (env_str)
    _vino_debug_flags |= g_parse_debug_string (env_str,
					       debug_keys,
					       G_N_ELEMENTS (debug_keys));
}
#endif /* G_ENABLE_DEBUG */

static struct VinoStockItem
{
  char *stock_id;
  char *stock_icon_id;
  char *label;
} vino_stock_items [] = {
  { VINO_STOCK_ALLOW,  GTK_STOCK_OK,     N_("_Allow") },
  { VINO_STOCK_REFUSE, GTK_STOCK_CANCEL, N_("_Refuse") },
};

void
vino_init_stock_items (void)
{
  static gboolean  initialized = FALSE;
  GtkIconFactory  *factory;
  GtkStockItem    *items;
  int              i;

  if (initialized)
    return;

  factory = gtk_icon_factory_new ();
  gtk_icon_factory_add_default (factory);

  items = g_new (GtkStockItem, G_N_ELEMENTS (vino_stock_items));

  for (i = 0; i < G_N_ELEMENTS (vino_stock_items); i++)
    {
      GtkIconSet *icon_set;

      items [i].stock_id           = g_strdup (vino_stock_items [i].stock_id);
      items [i].label              = g_strdup (vino_stock_items [i].label);
      items [i].modifier           = 0;
      items [i].keyval             = 0;
      items [i].translation_domain = g_strdup (GETTEXT_PACKAGE);

      /* FIXME: does this take into account the theme? */
      icon_set = gtk_icon_factory_lookup_default (vino_stock_items [i].stock_icon_id);
      gtk_icon_factory_add (factory, vino_stock_items [i].stock_id, icon_set);
    }
  
  gtk_stock_add_static (items, G_N_ELEMENTS (vino_stock_items));

  g_object_unref (factory);

  initialized = TRUE;
}

char *
vino_base64_unencode (const char *data)
{
  static const char *to_base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const        char *p;
  char              *retval;
  int                i, length, count, rem;
  int                qw, tw;

  if (!data || (length = strlen (data)) <= 0)
    return NULL;

  p = data;
  count = rem = 0;
  while (length > 0)
    {
      int skip, vrfy, i;

      skip = strspn (p, to_base64);

      count  += skip;
      p      += skip;
      length -= skip;

      if (length <= 0)
	break;

      vrfy = strcspn (p, to_base64);
      
      for (i = 0; i < vrfy; i++)
	{
	  if (g_ascii_isspace (p [i]))
	    continue;

	  if (p [i] != '=')
	    return NULL;
	  
	  /* rem must be either 2 or 3, otherwise
	   * no '=' should be here
	   */
	  if ((rem = count % 4) < 2)
	    return NULL;

	  /* end-of-message */
	  break;
	}

      length -= vrfy;
      p      += vrfy;
    }

  retval = g_new0 (char, (count / 4) * 3 + (rem ? rem - 1 : 0) + 1);

  if (count <= 0)
    return retval;

  qw = tw = 0;
  for (i = 0; data [i]; i++)
    {
      char c = data [i];
      char bits;

      if (g_ascii_isspace (c))
	continue;

      bits = 0;
      if ((c >= 'A') && (c <= 'Z'))
	{
	  bits = c - 'A';
	}
      else if ((c >= 'a') && (c <= 'z'))
	{
	  bits = c - 'a' + 26;
	}
      else if ((c >= '0') && (c <= '9'))
	{
	  bits = c - '0' + 52;
	}
      else if (c == '=')
	{
	  break;
	}

      switch (qw++)
	{
	case 0:
	  retval [tw + 0] = (bits << 2) & 0xfc;
	  break;
	case 1:
	  retval [tw + 0] |= (bits >> 4) & 0x03;
	  retval [tw + 1]  = (bits << 4) & 0xf0;
	  break;
	case 2:
	  retval [tw + 1] |= (bits >> 2) & 0x0f;
	  retval [tw + 2]  = (bits << 6) & 0xc0;
	  break;
	case 3:
	  retval [tw + 2] |= bits & 0x3f;
	  qw = 0;
	  tw +=3;
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
    }

  return retval;
}
