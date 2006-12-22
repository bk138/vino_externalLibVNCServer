/*
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2006 Jonh Wendell <wendell@bani.com.br>
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
 *      William Jon McCann <mccann@jhu.edu>
 *      Jonh Wendell <wendell@bani.com.br>
 *
 * Code taken from gnome-screensaver/src/gs-listener-dbus.h
 */

#ifndef __VINO_DBUS_LISTENER_H__
#define __VINO_DBUS_LISTENER_H__

#include "vino-server.h"

G_BEGIN_DECLS

#define VINO_TYPE_DBUS_LISTENER         (vino_dbus_listener_get_type ())
#define VINO_DBUS_LISTENER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), VINO_TYPE_DBUS_LISTENER, VinoDBusListener))
#define VINO_DBUS_LISTENER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VINO_TYPE_DBUS_LISTENER, VinoDBusListenerClass))
#define VINO_IS_DBUS_LISTENER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), VINO_TYPE_DBUS_LISTENER))
#define VINO_IS_DBUS_LISTENER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), VINO_TYPE_DBUS_LISTENER))
#define VINO_DBUS_LISTENER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), VINO_TYPE_DBUS_LISTENER, VinoDBusListenerClass))

typedef struct _VinoDBusListener        VinoDBusListener;
typedef struct _VinoDBusListenerClass   VinoDBusListenerClass;
typedef struct _VinoDBusListenerPrivate VinoDBusListenerPrivate;

struct _VinoDBusListener
{
  GObject                   base;

  VinoDBusListenerPrivate  *priv;
};

struct _VinoDBusListenerClass
{
  GObjectClass  base_class;
};

typedef enum
{
  VINO_DBUS_LISTENER_ERROR_SERVICE_UNAVAILABLE,
  VINO_DBUS_LISTENER_ERROR_ACQUISITION_FAILURE,
  VINO_DBUS_LISTENER_ERROR_ACTIVATION_FAILURE
} VinoDBusListenerError;

GType              vino_dbus_listener_get_type                (void) G_GNUC_CONST;

VinoDBusListener  *vino_dbus_listener_new                     (VinoServer   *server);

VinoServer        *vino_dbus_listener_get_server              (VinoDBusListener *listener);

G_END_DECLS

#endif /* __VINO_DBUS_LISTENER_H__ */
