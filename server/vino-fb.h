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

#ifndef __VINO_FB_H__
#define __VINO_FB_H__

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define VINO_TYPE_FB         (vino_fb_get_type ())
#define VINO_FB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), VINO_TYPE_FB, VinoFB))
#define VINO_FB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VINO_TYPE_FB, VinoFBClass))
#define VINO_IS_FB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), VINO_TYPE_FB))
#define VINO_IS_FB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), VINO_TYPE_FB))
#define VINO_FB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), VINO_TYPE_FB, VinoFBClass))

typedef struct _VinoFB        VinoFB;
typedef struct _VinoFBClass   VinoFBClass;
typedef struct _VinoFBPrivate VinoFBPrivate;

struct _VinoFB
{
  GObject        base;

  VinoFBPrivate *priv;
};

struct _VinoFBClass
{
  GObjectClass base_class;

  void (* damage_notify) (VinoFB *vfb);
  void (* size_changed)  (VinoFB *vfb);
};

GType         vino_fb_get_type           (void) G_GNUC_CONST;

VinoFB       *vino_fb_new                (GdkScreen *screen);

GdkScreen    *vino_fb_get_screen         (VinoFB    *vfb);
char         *vino_fb_get_pixels         (VinoFB    *vfb);
int           vino_fb_get_width          (VinoFB    *vfb);
int           vino_fb_get_height         (VinoFB    *vfb);
int           vino_fb_get_bits_per_pixel (VinoFB    *vfb);
int           vino_fb_get_rowstride      (VinoFB    *vfb);
int           vino_fb_get_depth          (VinoFB    *vfb);
GdkByteOrder  vino_fb_get_byte_order     (VinoFB    *vfb);
void          vino_fb_get_color_masks    (VinoFB    *vfb,
					  gulong    *red_mask,
					  gulong    *green_mask,
					  gulong    *blue_mask);

GdkRectangle *vino_fb_get_damage         (VinoFB    *vfb,
					  int       *n_rects,
					  gboolean   clear_damage);

G_END_DECLS

#endif /* __VINO_FB_H__ */
