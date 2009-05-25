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
 
#ifndef __VINO_DBUS_ERROR_H__
#define __VINO_DBUS_ERROR_H__

#include <dbus/dbus-glib.h>

typedef enum
{
  VINO_DBUS_ERROR_NOT_IMPLEMENTED,
  VINO_DBUS_ERROR_FAILED
} VinoDBusError;

GQuark vino_dbus_error_quark (void);

#endif /* __VINO_DBUS_ERROR_H__ */
