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

#ifndef __VINO_CURSOR_H__
#define __VINO_CURSOR_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _VinoCursorData VinoCursorData;

VinoCursorData *vino_cursor_init         (GdkScreen       *screen);
void            vino_cursor_finalize     (VinoCursorData  *data);
void            vino_cursor_get_position (VinoCursorData  *data,
					  int             *x,
					  int             *y);
gboolean        vino_cursor_get_x_source (VinoCursorData  *data,
					  int             *width,
					  int             *height,
					  const char     **cursor_source,
					  const char     **cursor_mask);

G_END_DECLS

#endif /* __VINO_CURSOR_H__ */
