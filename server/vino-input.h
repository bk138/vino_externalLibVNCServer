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

#ifndef __VINO_INPUT_H__
#define __VINO_INPUT_H__

#include <gdk/gdk.h>
#include "vino-server.h"

G_BEGIN_DECLS

gboolean vino_input_init                   (GdkDisplay *display);
void     vino_input_handle_pointer_event   (GdkScreen  *screen,
					    guint8      button_mask,
					    guint16     x,
					    guint16     y);
void     vino_input_handle_key_event       (GdkScreen  *screen,
					    guint32     keysym,
					    gboolean    key_press);
void     vino_input_handle_clipboard_event (GdkScreen  *screen,
					    char       *text,
					    int         len,
					    VinoServer *server);

G_END_DECLS

#endif /* __VINO_INPUT_H__ */
