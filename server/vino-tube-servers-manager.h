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

#ifndef __VINAGRE_TUBE_SERVERS_MANAGER_H__
#define __VINAGRE_TUBE_SERVERS_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define VINO_TYPE_TUBE_SERVERS_MANAGER (vino_tube_servers_manager_get_type())
#define VINO_TUBE_SERVERS_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
    VINO_TYPE_TUBE_SERVERS_MANAGER, VinoTubeServersManager))
#define VINO_IS_TUBE_SERVERS_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
    VINO_TYPE_TUBE_SERVERS_MANAGER))
#define VINO_TUBE_SERVERS_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST\
    ((klass), VINO_TYPE_TUBE_SERVERS_MANAGER, VinoTubeServersManagerClass))
#define VINO_IS_TUBE_SERVERS_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE \
    ((klass), VINO_TYPE_TUBE_SERVERS_MANAGER))
#define VINO_TUBE_SERVERS_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS \
    ((obj), VINO_TYPE_TUBE_SERVERS_MANAGER, VinoTubeServersManagerClass))

typedef struct _VinoTubeServersManager VinoTubeServersManager;
typedef struct _VinoTubeServersManagerClass VinoTubeServersManagerClass;
typedef struct _VinoTubeServersManagerPrivate VinoTubeServersManagerPrivate;

struct _VinoTubeServersManager
{
  GObject parent_instance;
  VinoTubeServersManagerPrivate *priv;
};

struct _VinoTubeServersManagerClass
{
  GObjectClass parent_class;
};

GType vino_tube_servers_manager_get_type (void) G_GNUC_CONST;
VinoTubeServersManager* vino_tube_servers_manager_new (void);

G_END_DECLS

#endif
