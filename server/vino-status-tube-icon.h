/*
 * © 2009, Collabora Ltd
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
 *      Arnaud Maillet <arnaud.maillet@collabora.co.uk>
 */

#ifndef __VINO_STATUS_TUBE_ICON_H__
#define __VINO_STATUS_TUBE_ICON_H__

#include <gdk/gdk.h>

#include "vino-tube-server.h"

G_BEGIN_DECLS

typedef enum
{
  VINO_STATUS_TUBE_ICON_VISIBILITY_INVALID = 0,
  VINO_STATUS_TUBE_ICON_VISIBILITY_ALWAYS,
  VINO_STATUS_TUBE_ICON_VISIBILITY_CLIENT,
  VINO_STATUS_TUBE_ICON_VISIBILITY_NEVER
} VinoStatusTubeIconVisibility;

#define VINO_TYPE_STATUS_TUBE_ICON (vino_status_tube_icon_get_type ())
#define VINO_STATUS_TUBE_ICON(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), \
    VINO_TYPE_STATUS_TUBE_ICON, VinoStatusTubeIcon))
#define VINO_STATUS_TUBE_ICON_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), \
    VINO_TYPE_STATUS_TUBE_ICON, VinoStatusTubeIconClass))
#define VINO_IS_STATUS_TUBE_ICON(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
    VINO_TYPE_STATUS_TUBE_ICON))
#define VINO_IS_STATUS_TUBE_ICON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), \
    VINO_TYPE_STATUS_TUBE_ICON))
#define VINO_STATUS_TUBE_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), \
    VINO_TYPE_STATUS_TUBE_ICON, VinoStatusTubeIconClass))

typedef struct _VinoStatusTubeIcon VinoStatusTubeIcon;
typedef struct _VinoStatusTubeIconClass VinoStatusTubeIconClass;
typedef struct _VinoStatusTubeIconPrivate VinoStatusTubeIconPrivate;

struct _VinoStatusTubeIcon
{
  GtkStatusIcon base;
  VinoStatusTubeIconPrivate *priv;
};

struct _VinoStatusTubeIconClass
{
  GtkStatusIconClass base_class;
};

GType vino_status_tube_icon_get_type (void) G_GNUC_CONST;

VinoStatusTubeIcon* vino_status_tube_icon_new (VinoTubeServer *server,
    GdkScreen *screen);

void vino_status_tube_icon_update_state (VinoStatusTubeIcon *icon);

void vino_status_tube_icon_set_visibility (VinoStatusTubeIcon *icon,
    VinoStatusTubeIconVisibility visibility);

void vino_status_tube_icon_show_notif (VinoStatusTubeIcon *icon,
    const gchar *summary, const gchar *body, gboolean invalidated);
G_END_DECLS

#endif /* __VINO_STATUS_TUBE_ICON_H__ */
