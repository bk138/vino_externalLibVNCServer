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

#ifndef __VINO_HTTP_H__
#define __VINO_HTTP_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define VINO_TYPE_HTTP         (vino_http_get_type ())
#define VINO_HTTP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), VINO_TYPE_HTTP, VinoHTTP))
#define VINO_HTTP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VINO_TYPE_HTTP, VinoHTTPClass))
#define VINO_IS_HTTP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), VINO_TYPE_HTTP))
#define VINO_IS_HTTP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), VINO_TYPE_HTTP))
#define VINO_HTTP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), VINO_TYPE_HTTP, VinoHTTPClass))

typedef struct _VinoHTTP        VinoHTTP;
typedef struct _VinoHTTPClass   VinoHTTPClass;
typedef struct _VinoHTTPPrivate VinoHTTPPrivate;

struct _VinoHTTP
{
  GObject        base;

  VinoHTTPPrivate *priv;
};

struct _VinoHTTPClass
{
  GObjectClass base_class;
};

GType     vino_http_get_type        (void) G_GNUC_CONST;

VinoHTTP *vino_http_get             (int       rfb_port);

void      vino_http_add_rfb_port    (VinoHTTP *http,
				     int       rfb_port);
void      vino_http_remove_rfb_port (VinoHTTP *http,
				     int       rfb_port);
int       vino_get_http_server_port ();

G_END_DECLS

#endif /* __VINO_HTTP_H__ */
