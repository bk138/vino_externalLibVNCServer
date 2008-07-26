/*
 * Copyright (C) 2006 Jonh Wendell.
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
 *      Jonh Wendell <wendell@bani.com.br>
 */

#ifndef __VINO_STATUS_ICON_H__
#define __VINO_STATUS_ICON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  VINO_STATUS_ICON_VISIBILITY_INVALID = 0,
  VINO_STATUS_ICON_VISIBILITY_ALWAYS,
  VINO_STATUS_ICON_VISIBILITY_CLIENT,
  VINO_STATUS_ICON_VISIBILITY_NEVER
} VinoStatusIconVisibility;

#define VINO_TYPE_STATUS_ICON         (vino_status_icon_get_type ())
#define VINO_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), VINO_TYPE_STATUS_ICON, VinoStatusIcon))
#define VINO_STATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VINO_TYPE_STATUS_ICON, VinoStatusIconClass))
#define VINO_IS_STATUS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), VINO_TYPE_STATUS_ICON))
#define VINO_IS_STATUS_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), VINO_TYPE_STATUS_ICON))
#define VINO_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), VINO_TYPE_STATUS_ICON, VinoStatusIconClass))

typedef struct _VinoStatusIcon        VinoStatusIcon;
typedef struct _VinoStatusIconClass   VinoStatusIconClass;
typedef struct _VinoStatusIconPrivate VinoStatusIconPrivate;

struct _VinoStatusIcon
{
  GtkStatusIcon          base;

  VinoStatusIconPrivate *priv;
};

struct _VinoStatusIconClass
{
  GtkStatusIconClass base_class;
};

#include "vino-server.h"

GType           vino_status_icon_get_type      (void) G_GNUC_CONST;

VinoStatusIcon *vino_status_icon_new           (VinoServer      *server,
                                                GdkScreen       *screen);

VinoServer     *vino_status_icon_get_server    (VinoStatusIcon  *icon);

void            vino_status_icon_add_client    (VinoStatusIcon  *icon,
                                                VinoClient      *client);
gboolean        vino_status_icon_remove_client (VinoStatusIcon  *icon,
                                                VinoClient      *client);

void		vino_status_icon_update_state	(VinoStatusIcon *icon);
void		vino_status_icon_set_visibility	(VinoStatusIcon           *icon,
						 VinoStatusIconVisibility visibility);
VinoStatusIconVisibility vino_status_icon_get_visibility (VinoStatusIcon *icon);

G_END_DECLS

#endif /* __VINO_STATUS_ICON_H__ */
