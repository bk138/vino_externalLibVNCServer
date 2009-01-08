/*
 * Copyright (c) 2008 Sun Microsystems, Inc.. All rights reserved.
 * Use is subject to license terms
 *
 *  Author: James Carlson, James.D.Carlson@Sun.COM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __IFADDRS_H
#define __IFADDRS_H

#include <sys/types.h>

#undef ifa_broadaddr
#undef ifa_dstaddr
struct ifaddrs {
	struct ifaddrs	*ifa_next;	/* Pointer to next struct */
	char		*ifa_name;	/* Interface name */
	uint64_t	ifa_flags;	/* Interface flags */
	struct sockaddr	*ifa_addr;	/* Interface address */
	struct sockaddr	*ifa_netmask;	/* Interface netmask */
	struct sockaddr	*ifa_dstaddr;	/* P2P interface destination */
};
#define	ifa_broadaddr	ifa_dstaddr

extern int getifaddrs(struct ifaddrs **);
extern void freeifaddrs(struct ifaddrs *);
#endif
