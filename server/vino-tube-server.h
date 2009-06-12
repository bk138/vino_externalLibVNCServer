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

#ifndef __VINO_TUBE_SERVER_H__
#define __VINO_TUBE_SERVER_H__

#include <glib-object.h>
#include "vino-server.h"

G_BEGIN_DECLS

#define VINO_TYPE_TUBE_SERVER         (vino_tube_server_get_type ())
#define VINO_TUBE_SERVER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
    VINO_TYPE_TUBE_SERVER, VinoTubeServer))
#define VINO_TUBE_SERVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), \
    VINO_TYPE_TUBE_SERVER, VinoTubeServerClass))
#define VINO_IS_TUBE_SERVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
    VINO_TYPE_TUBE_SERVER))
#define VINO_IS_TUBE_SERVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), \
    VINO_TYPE_TUBE_SERVER))
#define VINO_TUBE_SERVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), \
    VINO_TYPE_TUBE_SERVER, VinoTubeServerClass))

typedef struct _VinoTubeServer VinoTubeServer;
typedef struct _VinoTubeServerClass VinoTubeServerClass;
typedef struct _VinoTubeServerPrivate VinoTubeServerPrivate;

struct _VinoTubeServer
{
  VinoServer base;

  VinoTubeServerPrivate *priv;
};

struct _VinoTubeServerClass
{
  VinoServerClass base_class;

  void (* disconnected) (VinoTubeServer *server);
};

GType vino_tube_server_get_type (void) G_GNUC_CONST;
gboolean vino_tube_server_share_with_tube (VinoTubeServer *server,
     GError **error);
const gchar* vino_tube_server_get_alias (VinoTubeServer *self);
const gchar* vino_tube_server_get_avatar_filename (VinoTubeServer *self);

G_END_DECLS

#endif /* __VINO_TUBE_SERVER_H__ */