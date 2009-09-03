/*
 * Copyright (C) 2005 Ethium, Inc.
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
 *      Sebastien Estienne <sebastien.estienne@gmail.com>
 *
 */

#ifndef __VINO_MDNS_H__
#define __VINO_MDNS_H__

#include <glib.h>

G_BEGIN_DECLS

void vino_mdns_add_service (const char *type,
                            int         port);

void vino_mdns_start (const char *iface);
void vino_mdns_stop  (void);
void vino_mdns_shutdown (void);

const char *vino_mdns_get_hostname (void);

G_END_DECLS

#endif /* __VINO_MDNS_H__ */
