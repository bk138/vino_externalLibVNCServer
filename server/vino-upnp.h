/*
 * Copyright (C) 2008 Jonh Wendell.
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

#ifndef __VINO_UPNP_H__
#define __VINO_IPNP_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define VINO_TYPE_UPNP         (vino_upnp_get_type ())
#define VINO_UPNP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), VINO_TYPE_UPNP, VinoUpnp))
#define VINO_UPNP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VINO_TYPE_UPNP, VinoUpnpClass))
#define VINO_IS_UPNP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), VINO_TYPE_UPNP))
#define VINO_IS_UPNP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), VINO_TYPE_UPNP))
#define VINO_UPNP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), VINO_TYPE_UPNP, VinoUpnpClass))

typedef struct _VinoUpnp        VinoUpnp;
typedef struct _VinoUpnpClass   VinoUpnpClass;
typedef struct _VinoUpnpPrivate VinoUpnpPrivate;

struct _VinoUpnp
{
  GObject          base;
  VinoUpnpPrivate *priv;
};

struct _VinoUpnpClass
{
  GObjectClass base_class;
};

GType		vino_upnp_get_type		(void) G_GNUC_CONST;

VinoUpnp	*vino_upnp_new			(void);
gchar		*vino_upnp_get_external_ip	(VinoUpnp *upnp);
int		vino_upnp_get_external_port	(VinoUpnp *upnp);
int		vino_upnp_add_port		(VinoUpnp *upnp, int port);
void		vino_upnp_remove_port		(VinoUpnp *upnp);

G_END_DECLS

#endif /* __VINO_UPNP_H__ */
