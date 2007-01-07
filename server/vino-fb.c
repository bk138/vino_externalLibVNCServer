/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Red Hat, Inc.
 * Copyright (C) 2004 Novell, Inc.
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
 *      Federico Mena Quintero <federico@ximian.com>
 *
 *
 *   The screen polling code is based on XUpdateScanner from
 *   KRFB (krfb/xupdatescanner.cc) by Tim Jansen <tim@tjansen.de>:
 *
 *     Copyright (C) 2000 heXoNet Support GmbH, D-66424 Homburg.
 *
 */

#include <config.h>

#include "vino-fb.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif
#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

#include "vino-util.h"

#define TILE_WIDTH  32
#define TILE_HEIGHT 32

#ifndef HAVE_XSHM
typedef struct { int dummy; } XShmSegmentInfo;
#endif

struct _VinoFBPrivate
{
  Display         *xdisplay;
  GdkScreen       *screen;
  GdkWindow       *root_window;

  XImage          *fb_image;
  XShmSegmentInfo  fb_image_x_shm_info;
  Pixmap           fb_pixmap;

  XImage          *scanline;
  XShmSegmentInfo  scanline_x_shm_info;
  int              n_scanline;
  
  XImage          *tile;
  XShmSegmentInfo  tile_x_shm_info;

  GdkRegion       *damage_region;

  guint            update_timeout;

#ifdef HAVE_XDAMAGE
  GdkRegion       *pending_damage;
  guint            damage_idle_handler;

  Damage           xdamage;
  int              xdamage_notify_event;
  XserverRegion    xdamage_region;
  GC               xdamage_copy_gc;
#endif

  guint            use_x_shm : 1;
  guint            use_xdamage : 1;

  guint            fb_image_is_x_shm_segment : 1;
  guint            scanline_is_x_shm_segment : 1;
  guint            tile_is_x_shm_segment : 1;
};


enum
{
  PROP_0,
  PROP_SCREEN
};

enum
{
  DAMAGE_NOTIFY,
  SIZE_CHANGED,

  LAST_SIGNAL
};

static void vino_fb_init_from_screen (VinoFB    *vfb,
				      GdkScreen *screen);
static void vino_fb_screen_size_changed (VinoFB    *vfb,
					 GdkScreen *screen);

#ifdef HAVE_XDAMAGE
static GdkFilterReturn vino_fb_xdamage_event_filter (GdkXEvent *xevent,
						     GdkEvent  *event,
						     VinoFB    *vfb);
#endif

static gpointer parent_class;
static guint    signals [LAST_SIGNAL] = { 0 };


static void
emit_damage_notify (VinoFB *vfb)
{
  g_signal_emit (vfb, signals [DAMAGE_NOTIFY], 0);
}

static void
emit_size_changed (VinoFB *vfb)
{
  g_signal_emit (vfb, signals [SIZE_CHANGED], 0);
}

static gboolean
vino_fb_get_image (VinoFB          *vfb,
		   GdkDrawable     *drawable,
		   XImage          *image,
		   XShmSegmentInfo *x_shm_info,
		   gboolean         is_x_shm_segment,
		   int              x,
		   int              y,
		   int              width,
		   int              height)
{
  Drawable xdrawable;
  int      error;

  g_assert (vfb != NULL && drawable != NULL && image != NULL && x_shm_info != NULL);

  xdrawable = GDK_DRAWABLE_XID (drawable);

  gdk_error_trap_push ();

#ifdef HAVE_XSHM  
  if (is_x_shm_segment && image->width == width && image->height == height)
    {
      XShmGetImage (vfb->priv->xdisplay,
		    xdrawable,
		    image,
		    x, y,
		    AllPlanes);
    }
  else
#endif /* HAVE_XSHM */
    {
      XGetSubImage (vfb->priv->xdisplay,
		    xdrawable,
		    x, y, width, height,
		    AllPlanes, ZPixmap,
		    image, 0, 0);
    }

  if ((error = gdk_error_trap_pop ()))
    {
#ifdef G_ENABLE_DEBUG
      char error_text [64];

      XGetErrorText (vfb->priv->xdisplay, error, error_text, 63);

      g_warning ("Received a '%s' X Window System error while copying a tile",
		 error_text);
      g_warning ("Failed image = %d, %d %dx%d - screen = %dx%d",
		 x, y, width, height,
		 gdk_screen_get_width (vfb->priv->screen),
		 gdk_screen_get_height (vfb->priv->screen));
#endif

      return FALSE;
    }

  return TRUE;
}

static void
vino_fb_destroy_image (VinoFB          *vfb,
		       XImage          *image,
		       XShmSegmentInfo *x_shm_info,
		       gboolean         is_x_shm_segment,
		       gboolean         is_attached)
{
#ifdef HAVE_XSHM
  if (is_x_shm_segment)
    {
      if (is_attached)
	XShmDetach (vfb->priv->xdisplay, x_shm_info);

      if (x_shm_info->shmaddr != (char *)-1)
	shmdt (x_shm_info->shmaddr);
      x_shm_info->shmaddr = (char *)-1;
      x_shm_info->shmid = -1;
    }
#endif /* HAVE_XSHM */
  
  if (image)
    XDestroyImage (image);
}

static gboolean
vino_fb_create_image (VinoFB           *vfb,
		      XImage          **image,
		      XShmSegmentInfo  *x_shm_info,
		      gboolean          must_use_x_shm,
		      int               width,
		      int               height,
		      int               depth)
{
  int n_screen;

  n_screen = gdk_screen_get_number (vfb->priv->screen);

#ifdef HAVE_XSHM
  if (vfb->priv->use_x_shm)
    {
      *image = XShmCreateImage (vfb->priv->xdisplay,
			       DefaultVisual (vfb->priv->xdisplay, n_screen),
			       depth,
			       ZPixmap,
			       NULL,
			       x_shm_info,
			       width,
			       height);
      if (!*image)
	goto x_shm_error;

      x_shm_info->shmid = shmget (IPC_PRIVATE,
				  (*image)->bytes_per_line * (*image)->height,
				  IPC_CREAT | 0600);
      if (x_shm_info->shmid == -1)
	goto x_shm_error;

      x_shm_info->readOnly = False;
      x_shm_info->shmaddr = shmat (x_shm_info->shmid, 0, 0);
      (*image)->data = x_shm_info->shmaddr;

      if (x_shm_info->shmaddr == (char*) -1)
	goto x_shm_error;

      gdk_error_trap_push ();

      XShmAttach (vfb->priv->xdisplay, x_shm_info);
      XSync (vfb->priv->xdisplay, False);
      
      if (gdk_error_trap_pop ())
	goto x_shm_error;
      
      shmctl (x_shm_info->shmid, IPC_RMID, 0);

      return TRUE;

    x_shm_error:
      vfb->priv->use_x_shm = FALSE;

      vino_fb_destroy_image (vfb, *image, x_shm_info, TRUE, FALSE);
      *image = NULL;
      
      return vino_fb_create_image (vfb, image, x_shm_info, FALSE,
				   width, height, depth);
    }
#endif /* HAVE_XSHM */

  if (!must_use_x_shm)
    {
      int   rowstride = width * (depth / 8);
      char *data;

      data = malloc (rowstride * height);
      if (!data)
	return FALSE;

      *image = XCreateImage (vfb->priv->xdisplay,
			     DefaultVisual (vfb->priv->xdisplay, 0),
			     depth,
			     ZPixmap,
			     0,
			     data,
			     width,
			     height,
			     8,
			     rowstride);
      if (!*image)
	free (data);

      return FALSE;
    }

  return FALSE;
}

static gboolean
vino_fb_copy_tile (VinoFB       *vfb,
		   GdkRectangle *rect)
{
  XImage *fb_image;
  char   *src;
  char   *dest;
  int     bytes_per_pixel;
  int     src_bytes_per_line;
  int     dest_bytes_per_line;
  int     i;

  if (!vino_fb_get_image (vfb,
			  vfb->priv->root_window,
			  vfb->priv->tile,
			  &vfb->priv->tile_x_shm_info,
			  vfb->priv->tile_is_x_shm_segment,
			  rect->x, 
			  rect->y,
			  rect->width,
			  rect->height))
    return FALSE;

  fb_image = vfb->priv->fb_image;

  src_bytes_per_line  = vfb->priv->tile->bytes_per_line;
  dest_bytes_per_line = fb_image->bytes_per_line;
  bytes_per_pixel     = fb_image->bits_per_pixel >> 3;

  src  = vfb->priv->tile->data;
  dest = fb_image->data + rect->y * dest_bytes_per_line + rect->x * bytes_per_pixel;

  for (i = 0; i < rect->height; i++)
    {
      memcpy (dest, src, rect->width * bytes_per_pixel);

      src  += src_bytes_per_line;
      dest += dest_bytes_per_line;
    }

  return TRUE;
}

static gboolean
vino_fb_poll_scanline (VinoFB *vfb,
		       int     line)
{
  char     *src, *dest;
  int       screen_width, screen_height;
  int       bytes_per_pixel;
  int       x, inc;
  gboolean  retval = FALSE;

  screen_width  = gdk_screen_get_width  (vfb->priv->screen);
  screen_height = gdk_screen_get_height (vfb->priv->screen);

  g_assert (line >= 0 && line < screen_height);

  if (!vino_fb_get_image (vfb,
			  vfb->priv->root_window,
			  vfb->priv->scanline,
			  &vfb->priv->scanline_x_shm_info,
			  vfb->priv->scanline_is_x_shm_segment,
			  0, line,
			  screen_width, 1))
      return FALSE;

  bytes_per_pixel = vfb->priv->fb_image->bits_per_pixel >> 3;

  dest = vfb->priv->fb_image->data + (line * vfb->priv->fb_image->bytes_per_line);
  src  = vfb->priv->scanline->data;
  inc  = TILE_WIDTH * bytes_per_pixel;

  for (x = 0; x < screen_width; x += TILE_WIDTH, dest += inc, src += inc)
    {
      int width = MIN (TILE_WIDTH, screen_width - x);

      if (memcmp (dest, src, width * bytes_per_pixel) != 0)
	{
	  GdkRectangle rect;

	  rect.x = x;
	  rect.y = line - (line % TILE_HEIGHT);
	  rect.width = width;
	  rect.height = MIN (TILE_HEIGHT, screen_height - rect.y);

	  dprintf (POLLING, "damage: (%d, %d) (%d x %d)\n", rect.x, rect.y, rect.width, rect.height);

	  if (vino_fb_copy_tile (vfb, &rect))
	    {
	      if (!vfb->priv->damage_region)
		vfb->priv->damage_region = gdk_region_rectangle (&rect);
	      else
		gdk_region_union_with_rect (vfb->priv->damage_region, &rect);
	    }

	  retval = TRUE;
	}
    }

  return retval;
}

static gboolean
vino_fb_poll_screen (VinoFB *vfb)
{
#define N_SCANLINES 35
  unsigned int scanlines [N_SCANLINES] = {
    0, 16,  8, 24, 33, 4,  20,
   12, 28, 10, 26, 18, 34,  2,
   22,  6, 30, 14,  1, 17, 32,
    9, 25,  7, 23, 15, 31, 19,
    3, 27, 11, 29, 13,  5, 21
  };
  int      screen_height;
  int      line;
  gboolean screen_damaged = FALSE;
  gboolean already_damaged = vfb->priv->damage_region != NULL;

  dprintf (POLLING, "polling screen %d, scanline index %d, every %d scanline from %d\n",
	   gdk_screen_get_number (vfb->priv->screen),
	   vfb->priv->n_scanline,
	   N_SCANLINES,
	   scanlines [vfb->priv->n_scanline]);

  screen_height = gdk_screen_get_height (vfb->priv->screen);

  for (line = scanlines [vfb->priv->n_scanline]; line < screen_height; line += N_SCANLINES)
    {
      if (vino_fb_poll_scanline (vfb, line))
	screen_damaged = TRUE;
    }
  
  vfb->priv->n_scanline = ++vfb->priv->n_scanline % N_SCANLINES;

  if (!already_damaged && screen_damaged)
    emit_damage_notify (vfb);

  return TRUE;
}

static void
vino_fb_finalize_xdamage (VinoFB *vfb)
{
#ifdef HAVE_XDAMAGE
  if (vfb->priv->damage_idle_handler)
    g_source_remove (vfb->priv->damage_idle_handler);
  vfb->priv->damage_idle_handler = 0;

  if (vfb->priv->pending_damage)
    gdk_region_destroy (vfb->priv->pending_damage);
  vfb->priv->pending_damage = NULL;

  if (vfb->priv->fb_pixmap)
    XFreePixmap (vfb->priv->xdisplay, vfb->priv->fb_pixmap);
  vfb->priv->fb_pixmap = None;

  gdk_window_remove_filter (vfb->priv->root_window,
			    (GdkFilterFunc) vino_fb_xdamage_event_filter,
			    vfb);

  if (vfb->priv->xdamage_copy_gc != None)
    XFreeGC (vfb->priv->xdisplay, vfb->priv->xdamage_copy_gc);
  vfb->priv->xdamage_copy_gc = None;

  if (vfb->priv->xdamage != None)
    XFixesDestroyRegion (vfb->priv->xdisplay, vfb->priv->xdamage_region);
  vfb->priv->xdamage_region = None;

  if (vfb->priv->xdamage != None)
    XDamageDestroy (vfb->priv->xdisplay, vfb->priv->xdamage);
  vfb->priv->xdamage = None;
#endif
}

/* Frees the scanline and tile data */
static void
vino_fb_finalize_polling (VinoFB *vfb)
{
  if (vfb->priv->update_timeout)
    g_source_remove (vfb->priv->update_timeout);
  vfb->priv->update_timeout = 0;

  if (vfb->priv->scanline)
    vino_fb_destroy_image (vfb,
			   vfb->priv->scanline,
			   &vfb->priv->scanline_x_shm_info,
			   vfb->priv->scanline_is_x_shm_segment,
			   TRUE);
  vfb->priv->scanline = NULL;
  
  if (vfb->priv->tile)
    vino_fb_destroy_image (vfb,
			   vfb->priv->tile,
			   &vfb->priv->tile_x_shm_info,
			   vfb->priv->tile_is_x_shm_segment,
			   TRUE);
  vfb->priv->tile = NULL;
}

static void
vino_fb_finalize_screen_data (VinoFB *vfb)
{
  if (vfb->priv->damage_region)
    gdk_region_destroy (vfb->priv->damage_region);
  vfb->priv->damage_region = NULL;

  if (vfb->priv->use_xdamage)
    vino_fb_finalize_xdamage (vfb);
  else
    vino_fb_finalize_polling (vfb);

  if (vfb->priv->fb_image)
    vino_fb_destroy_image (vfb,
			   vfb->priv->fb_image,
			   &vfb->priv->fb_image_x_shm_info,
			   vfb->priv->fb_image_is_x_shm_segment,
			   TRUE);
  vfb->priv->fb_image = NULL;
  
  g_signal_handlers_disconnect_by_func (vfb->priv->screen,
					G_CALLBACK (vino_fb_screen_size_changed),
					vfb);

}

static void
vino_fb_screen_size_changed (VinoFB    *vfb,
			     GdkScreen *screen)
{
  g_return_if_fail (VINO_IS_FB (vfb));

  vino_fb_finalize_screen_data (vfb);
  vino_fb_init_from_screen (vfb, screen);

  emit_size_changed (vfb);
}

#ifdef HAVE_XDAMAGE

static gboolean
vino_fb_xdamage_idle_handler (VinoFB *vfb)
{
  GdkRectangle *damage = NULL;
  XRectangle    xdamage;
  int           n_rects;
  int           error;

  g_assert (!gdk_region_empty (vfb->priv->pending_damage));

  gdk_region_get_rectangles (vfb->priv->pending_damage, &damage, &n_rects);

  xdamage.x      = damage->x;
  xdamage.y      = damage->y;
  xdamage.width  = damage->width;
  xdamage.height = damage->height;

  dprintf (POLLING, "Updating damaged region in idle: %d %d %dx%d\n",
	   damage->x, damage->y, damage->width, damage->height);

  /* subtract damage from server */
  XFixesSetRegion (vfb->priv->xdisplay, vfb->priv->xdamage_region, &xdamage, 1);
  XDamageSubtract (vfb->priv->xdisplay,
		   vfb->priv->xdamage,
		   vfb->priv->xdamage_region,
		   None);

  gdk_error_trap_push ();

  /* Copy the damaged pixels from the server */
  if (vfb->priv->use_x_shm)
    {
      XCopyArea (vfb->priv->xdisplay,
		 GDK_WINDOW_XWINDOW (vfb->priv->root_window),
		 vfb->priv->fb_pixmap,
		 vfb->priv->xdamage_copy_gc,
		 damage->x,
		 damage->y,
		 damage->width,
		 damage->height,
		 damage->x,
		 damage->y);
      XSync (vfb->priv->xdisplay, False);
    }
  else
    {
      XGetSubImage (vfb->priv->xdisplay,
		    GDK_WINDOW_XWINDOW (vfb->priv->root_window),
		    damage->x,
		    damage->y,
		    damage->width,
		    damage->height,
		    AllPlanes,
		    ZPixmap,
		    vfb->priv->fb_image,
		    damage->x,
		    damage->y);
    }

  if ((error = gdk_error_trap_pop ()))
    {
#ifdef G_ENABLE_DEBUG
      char error_text [64];

      XGetErrorText (vfb->priv->xdisplay, error, error_text, 63);

      g_warning ("Received a '%s' X Window System error while copying damaged pixels",
		 error_text);
      g_warning ("Failed image = %d, %d %dx%d - screen = %dx%d",
		 damage->x,
		 damage->y,
		 damage->width,
		 damage->height,
		 gdk_screen_get_width (vfb->priv->screen),
		 gdk_screen_get_height (vfb->priv->screen));
#endif
      goto out;
    }

  /* add damage to our region */
  if (vfb->priv->damage_region)
    gdk_region_union_with_rect (vfb->priv->damage_region, damage);
  else
    vfb->priv->damage_region = gdk_region_rectangle (damage);

  emit_damage_notify (vfb);

 out:
  {
    GdkRegion *tmp;

    tmp = gdk_region_rectangle (damage);
    gdk_region_subtract (vfb->priv->pending_damage, tmp);
    gdk_region_destroy (tmp);
  }

  g_free (damage);

  if (gdk_region_empty (vfb->priv->pending_damage))
    {
      vfb->priv->damage_idle_handler = 0;
      return FALSE;
    }

  return TRUE;
}

static GdkFilterReturn
vino_fb_xdamage_event_filter (GdkXEvent *xevent,
			      GdkEvent  *event,
			      VinoFB    *vfb)
{
  XEvent             *xev = (XEvent *) xevent;
  XDamageNotifyEvent *notify;
  GdkRectangle        damage;

  if (xev->type != vfb->priv->xdamage_notify_event)
    return GDK_FILTER_CONTINUE;

  notify = (XDamageNotifyEvent *) xev;

  damage.x      = notify->area.x;
  damage.y      = notify->area.y;
  damage.width  = notify->area.width;
  damage.height = notify->area.height;

  dprintf (POLLING, "Got DamageNotify event: %d %d %dx%d, more = %s, level = %d\n",
	   damage.x, damage.y, damage.width, damage.height,
	   notify->more ? "(true)" : "(false)", notify->level);

  gdk_region_union_with_rect (vfb->priv->pending_damage, &damage);

  if (!vfb->priv->damage_idle_handler)
    vfb->priv->damage_idle_handler =
      g_idle_add ((GSourceFunc) vino_fb_xdamage_idle_handler, vfb);

  return GDK_FILTER_REMOVE;
}
#endif /* HAVE_XDAMAGE */

static void
vino_fb_init_xdamage (VinoFB *vfb)
{
#ifdef HAVE_XDAMAGE
  int       event_base, error_base;
  int       major, minor;
  XGCValues values;

  if (!XDamageQueryExtension (vfb->priv->xdisplay, &event_base, &error_base))
    return;

  if (!XDamageQueryVersion (vfb->priv->xdisplay, &major, &minor) || major != 1)
    return;

  vfb->priv->xdamage_notify_event = event_base + XDamageNotify;

  vfb->priv->xdamage = XDamageCreate (vfb->priv->xdisplay,
				      GDK_WINDOW_XWINDOW (vfb->priv->root_window),
				      XDamageReportDeltaRectangles);
  if (vfb->priv->xdamage == None)
    return;

  vfb->priv->xdamage_region = XFixesCreateRegion (vfb->priv->xdisplay, NULL, 0);
  if (vfb->priv->xdamage_region == None)
    {
      XDamageDestroy (vfb->priv->xdisplay, vfb->priv->xdamage);
      vfb->priv->xdamage = None;
      return;
    }

  values.subwindow_mode = IncludeInferiors;
  vfb->priv->xdamage_copy_gc = XCreateGC (vfb->priv->xdisplay,
					  GDK_WINDOW_XWINDOW (vfb->priv->root_window),
					  GCSubwindowMode,
					  &values);

  gdk_x11_register_standard_event_type (gdk_screen_get_display (vfb->priv->screen),
					event_base,
					XDamageNumberEvents);
  gdk_window_add_filter (vfb->priv->root_window,
			 (GdkFilterFunc) vino_fb_xdamage_event_filter,
			 vfb);

  vfb->priv->pending_damage = gdk_region_new ();

  vfb->priv->use_xdamage = TRUE;
#endif
}

static void
vino_fb_init_polling (VinoFB *vfb)
{
  g_assert (!vfb->priv->use_xdamage);

  vfb->priv->scanline_is_x_shm_segment =
    vino_fb_create_image (vfb,
			  &vfb->priv->scanline,
			  &vfb->priv->scanline_x_shm_info, FALSE,
			  gdk_screen_get_width (vfb->priv->screen), 1,
			  vfb->priv->fb_image->depth);
  if (!vfb->priv->scanline)
    {
      g_warning (G_STRLOC ": failed to initialize scanline XImage\n");
      XDestroyImage (vfb->priv->fb_image);
      vfb->priv->fb_image = NULL;
      return;
    }

  dprintf (POLLING, "Initialized scanline XImage (%p): is_x_shm_segment = %s\n",
	   vfb->priv->scanline,
	   vfb->priv->scanline_is_x_shm_segment ? "(true)" : "(false)");
  
  vfb->priv->tile_is_x_shm_segment =
    vino_fb_create_image (vfb,
			  &vfb->priv->tile,
			  &vfb->priv->tile_x_shm_info, FALSE,
			  TILE_WIDTH, TILE_HEIGHT,
			  vfb->priv->fb_image->depth);
  if (!vfb->priv->tile)
    {
      g_warning (G_STRLOC ": failed to initialize tile XImage\n");
      vino_fb_destroy_image (vfb,
			     vfb->priv->scanline,
			     &vfb->priv->scanline_x_shm_info,
			     vfb->priv->scanline_is_x_shm_segment,
			     TRUE);
      vfb->priv->scanline = NULL;
      XDestroyImage (vfb->priv->fb_image);
      vfb->priv->fb_image = NULL;
      return;
    }

  dprintf (POLLING, "Initialized scanline XImage (%p): is_x_shm_segment = %s\n",
	   vfb->priv->scanline,
	   vfb->priv->scanline_is_x_shm_segment ? "(true)" : "(false)");

  vfb->priv->update_timeout =
    g_timeout_add (20, (GSourceFunc) vino_fb_poll_screen, vfb);
}

static void
vino_fb_init_fb_image (VinoFB *vfb)
{
  if (vfb->priv->use_xdamage)
      {
	vfb->priv->fb_image_is_x_shm_segment =
	  vino_fb_create_image (vfb,
				&vfb->priv->fb_image,
				&vfb->priv->fb_image_x_shm_info,
				TRUE,
				gdk_screen_get_width (vfb->priv->screen),
				gdk_screen_get_height (vfb->priv->screen),
				DefaultDepthOfScreen (GDK_SCREEN_XSCREEN (vfb->priv->screen)));
      }

  if (vfb->priv->fb_image)
    {
#ifdef HAVE_XSHM
      if (vfb->priv->use_x_shm)
	{
	  vfb->priv->fb_pixmap = XShmCreatePixmap (vfb->priv->xdisplay,
						   GDK_WINDOW_XWINDOW (vfb->priv->root_window),
						   vfb->priv->fb_image->data,
						   &vfb->priv->fb_image_x_shm_info,
						   vfb->priv->fb_image->width,
						   vfb->priv->fb_image->height,
						   vfb->priv->fb_image->depth);
	}
#endif
      if (vfb->priv->fb_pixmap == None)
	{
	  vino_fb_destroy_image (vfb,
				 vfb->priv->fb_image,
				 &vfb->priv->fb_image_x_shm_info,
				 vfb->priv->fb_image_is_x_shm_segment,
				 TRUE);
	  vfb->priv->fb_image = NULL;
	  vfb->priv->fb_image_is_x_shm_segment = FALSE;
	}
    }

  if (!vfb->priv->fb_image)
    {
      vfb->priv->fb_image =
	XGetImage (vfb->priv->xdisplay,
		   GDK_WINDOW_XWINDOW (vfb->priv->root_window),
		   0, 0,
		   gdk_screen_get_width  (vfb->priv->screen),
		   gdk_screen_get_height (vfb->priv->screen),
		   AllPlanes,
		   ZPixmap);
    }
}

static void
vino_fb_init_from_screen (VinoFB    *vfb,
			  GdkScreen *screen)
{
  g_return_if_fail (screen != NULL);

  vfb->priv->screen      = screen;
  vfb->priv->xdisplay    = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));
  vfb->priv->root_window = gdk_screen_get_root_window (screen);

#ifdef HAVE_XSHM
  vfb->priv->use_x_shm = XShmQueryExtension (vfb->priv->xdisplay) != False;
  if (vfb->priv->use_x_shm)
    {
      int major, minor;
      Bool shared_pixmaps;

      XShmQueryVersion (vfb->priv->xdisplay, &major, &minor, &shared_pixmaps);
      if (!shared_pixmaps)
	vfb->priv->use_x_shm = FALSE;
    }
#endif

  g_signal_connect_swapped (vfb->priv->screen, "size-changed",
			    G_CALLBACK (vino_fb_screen_size_changed),
			    vfb);

  vino_fb_init_xdamage (vfb);
  vino_fb_init_fb_image (vfb);

  if (!vfb->priv->fb_image)
    {
      g_warning (G_STRLOC ": failed to initialize frame buffer XImage");
      return;
    }

  dprintf (POLLING, "Initialized framebuffer contents (%p) for screen %d: %dx%d %dbpp\n",
	   vfb->priv->fb_image,
	   gdk_screen_get_number (vfb->priv->screen),
	   vfb->priv->fb_image->width,
	   vfb->priv->fb_image->height,
	   vfb->priv->fb_image->depth);

  if (!vfb->priv->use_xdamage)
    vino_fb_init_polling (vfb);
}


static void
vino_fb_finalize (GObject *object)
{
  VinoFB *vfb = VINO_FB (object);
  
  vino_fb_finalize_screen_data (vfb);
 
  g_free (vfb->priv);
  vfb->priv = NULL;

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
vino_fb_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
  VinoFB *vfb = VINO_FB (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      vino_fb_init_from_screen (vfb, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_fb_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
  VinoFB *vfb = VINO_FB (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, vfb->priv->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_fb_instance_init (VinoFB *vfb)
{
  vfb->priv = g_new0 (VinoFBPrivate, 1);

#ifdef HAVE_XSHM
  vfb->priv->fb_image_x_shm_info.shmid   = -1;
  vfb->priv->fb_image_x_shm_info.shmaddr = (char *) -1;

  vfb->priv->scanline_x_shm_info.shmid   = -1;
  vfb->priv->scanline_x_shm_info.shmaddr = (char *) -1;
  
  vfb->priv->tile_x_shm_info.shmid   = -1;
  vfb->priv->tile_x_shm_info.shmaddr = (char *) -1;
#endif /* HAVE_XSHM */
}

static void
vino_fb_class_init (VinoFBClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  gobject_class->finalize     = vino_fb_finalize;
  gobject_class->set_property = vino_fb_set_property;
  gobject_class->get_property = vino_fb_get_property;

  klass->damage_notify = NULL;
  klass->size_changed  = NULL;

  g_object_class_install_property (gobject_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
							"Screen",
							"The screen to be monitored",
							GDK_TYPE_SCREEN,
							G_PARAM_READWRITE      |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME    |
                                                        G_PARAM_STATIC_NICK    |
                                                        G_PARAM_STATIC_BLURB));

  signals [DAMAGE_NOTIFY] =
    g_signal_new ("damage-notify",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VinoFBClass, damage_notify),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  
  signals [SIZE_CHANGED] =
    g_signal_new ("size-changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (VinoFBClass, size_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

GType
vino_fb_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (VinoFBClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) vino_fb_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (VinoFB),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) vino_fb_instance_init,
	};
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "VinoFB",
                                            &object_info, 0);
    }

  return object_type;
}


VinoFB *
vino_fb_new (GdkScreen *screen)
{
  VinoFB *vfb;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  vfb = g_object_new (VINO_TYPE_FB,
		      "screen", screen,
		      NULL);
  if (vfb && !vfb->priv->fb_image)
    {
      g_object_unref (vfb);
      return NULL;
    }

  return vfb;
}

GdkScreen *
vino_fb_get_screen (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), NULL);

  return vfb->priv->screen;
}

char *
vino_fb_get_pixels (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), NULL);
  
  g_assert (vfb->priv->fb_image != NULL);

  return vfb->priv->fb_image->data;
}

int
vino_fb_get_width (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), -1);
  
  g_assert (vfb->priv->fb_image != NULL);

  return vfb->priv->fb_image->width;
}

int
vino_fb_get_height (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), -1);
  
  g_assert (vfb->priv->fb_image != NULL);

  return vfb->priv->fb_image->height;
}

int
vino_fb_get_bits_per_pixel (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), -1);
  
  g_assert (vfb->priv->fb_image != NULL);

  return vfb->priv->fb_image->bits_per_pixel;
}
int
vino_fb_get_rowstride (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), -1);
  
  g_assert (vfb->priv->fb_image != NULL);

  return vfb->priv->fb_image->bytes_per_line;
}

int
vino_fb_get_depth (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), -1);
  
  g_assert (vfb->priv->fb_image != NULL);

  return vfb->priv->fb_image->depth;
}

GdkByteOrder
vino_fb_get_byte_order (VinoFB *vfb)
{
  g_return_val_if_fail (VINO_IS_FB (vfb), -1);
  
  g_assert (vfb->priv->fb_image != NULL);

  return vfb->priv->fb_image->bitmap_bit_order == MSBFirst ? GDK_MSB_FIRST : GDK_LSB_FIRST;
}

void
vino_fb_get_color_masks (VinoFB *vfb,
			 gulong *red_mask,
			 gulong *green_mask,
			 gulong *blue_mask)
{
  g_return_if_fail (VINO_IS_FB (vfb));

  g_assert (vfb->priv->fb_image != NULL);

  if (red_mask)
    *red_mask = vfb->priv->fb_image->red_mask;
  if (green_mask)
    *green_mask = vfb->priv->fb_image->green_mask;
  if (blue_mask)
    *blue_mask = vfb->priv->fb_image->blue_mask;
}

static inline void
vino_fb_debug_dump_damage (VinoFB       *vfb,
			   GdkRectangle *rects,
			   int           n_rects)
{
#ifdef G_ENABLE_DEBUG
  if (_vino_debug_flags & VINO_DEBUG_POLLING)
    {
      GdkRectangle clipbox;
      int          area;
      int          i;

      gdk_region_get_clipbox (vfb->priv->damage_region, &clipbox);

      fprintf (stderr, "Dump of damage region: clipbox (%d, %d) (%d x %d)\n",
	       clipbox.x, clipbox.y, clipbox.width, clipbox.height);

      area = 0;
      for (i = 0; i < n_rects; i++)
	{
	  fprintf (stderr, "\t(%d, %d) (%d x %d)\n",
		   rects [i].x, rects [i].y, rects [i].width, rects [i].height);
	  area += rects [i].width * rects [i].height;
	}

      fprintf (stderr, "Bounding area %d, damaged area %d ... (%d%%)\n",
	       clipbox.width * clipbox.height, area,
	       (area * 100) / (clipbox.width * clipbox.height));
    }
#endif
}

GdkRectangle *
vino_fb_get_damage (VinoFB   *vfb,
		    int      *n_rects,
		    gboolean  clear_damage)

{
  GdkRectangle *retval;

  g_return_val_if_fail (VINO_IS_FB (vfb), NULL);
  g_return_val_if_fail (n_rects != NULL, NULL);

  if (!vfb->priv->damage_region)
    {
      *n_rects = 0;
      return NULL;
    }

  retval = NULL;
  *n_rects = 0;
  gdk_region_get_rectangles (vfb->priv->damage_region, &retval, n_rects);

  vino_fb_debug_dump_damage (vfb, retval, *n_rects);
 
  if (clear_damage)
    {
      gdk_region_destroy (vfb->priv->damage_region);
      vfb->priv->damage_region = NULL;
    }

  return retval;
}
