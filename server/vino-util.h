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

#ifndef __VINO_UTIL_H__
#define __VINO_UTIL_H__

#include <config.h>
#include <libintl.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VINO_STOCK_ALLOW  "vino-allow"
#define VINO_STOCK_REFUSE "vino-refuse"

typedef enum
{
  VINO_DEBUG_NONE    = 0,
  VINO_DEBUG_POLLING = 1 << 0,
  VINO_DEBUG_RFB     = 1 << 1,
  VINO_DEBUG_INPUT   = 1 << 2,
  VINO_DEBUG_PREFS   = 1 << 3,
  VINO_DEBUG_TLS     = 1 << 4,
  VINO_DEBUG_MDNS    = 1 << 5,
  VINO_DEBUG_PROMPT  = 1 << 6,
  VINO_DEBUG_HTTP    = 1 << 7,
  VINO_DEBUG_DBUS    = 1 << 8,
  VINO_DEBUG_UPNP    = 1 << 9,
  VINO_DEBUG_TUBE    = 1 << 10
} VinoDebugFlags;

#ifdef G_ENABLE_DEBUG

#include <stdio.h>

extern VinoDebugFlags _vino_debug_flags;

#ifdef G_HAVE_ISO_VARARGS
#  define dprintf(type, ...) G_STMT_START {         \
        if (_vino_debug_flags & VINO_DEBUG_##type)  \
                fprintf (stderr, __VA_ARGS__);      \
        } G_STMT_END
#elif defined(G_HAVE_GNUC_VARARGS)
#  define dprintf(type, args...) G_STMT_START {     \
        if (_vino_debug_flags & VINO_DEBUG_##type)  \
                fprintf (stderr, args);             \
        } G_STMT_END
#endif

void vino_setup_debug_flags (void);

#else /* if !defined (G_ENABLE_DEBUG) */

#ifdef G_HAVE_ISO_VARARGS
#  define dprintf(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#  define dprintf(args...)
#endif

#define vino_setup_debug_flags()

#endif /* G_ENABLE_DEBUG */

void  vino_init_stock_items	(void);

void  vino_util_show_error	(const gchar *title,
				 const gchar *message,
				 GtkWindow *parent);


G_END_DECLS

#endif /* __VINO_UTIL_H__ */
