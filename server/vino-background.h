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

#ifndef __VINO_BACKGROUND_H__
#define __VINO_BACKGROUND_H__

#include <glib.h>

G_BEGIN_DECLS

void vino_background_handler (int sig);
void vino_background_draw (gboolean option);
gboolean vino_background_get_status (void);

G_END_DECLS

#endif /* __VINO_BACKGROUND_H__ */
