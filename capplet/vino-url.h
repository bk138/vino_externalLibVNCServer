/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __VINO_URL_H__
#define __VINO_URL_H__

#include <gtk/gtklabel.h>

G_BEGIN_DECLS

#define VINO_TYPE_URL         (vino_url_get_type ())
#define VINO_URL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), VINO_TYPE_URL, VinoURL))
#define VINO_URL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), VINO_TYPE_URL, VinoURLClass))
#define VINO_IS_URL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), VINO_TYPE_URL))
#define VINO_IS_URL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), VINO_TYPE_URL))
#define VINO_URL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), VINO_TYPE_URL, VinoURLClass))

typedef struct _VinoURL        VinoURL;
typedef struct _VinoURLClass   VinoURLClass;
typedef struct _VinoURLPrivate VinoURLPrivate;

struct _VinoURL
{
  GtkLabel        label;

  VinoURLPrivate *priv;
};

struct _VinoURLClass
{
  GtkLabelClass  label_class;

  /* Key binding signal; don't connect to this.
   */
  void         (*activate) (VinoURL *url);
};

GType                vino_url_get_type             (void);
GtkWidget           *vino_url_new                  (const char     *address,
						    const char     *label,
						    const char     *tooltip);
void                 vino_url_set_address          (VinoURL        *url,
						    const char     *address);
G_CONST_RETURN char *vino_url_get_address          (VinoURL        *url);
void                 vino_url_set_tooltip          (VinoURL        *url,
						    const char     *tooltip);
G_CONST_RETURN char *vino_url_get_tooltip          (VinoURL        *url);
void                 vino_url_change_attribute     (VinoURL        *url,
						    PangoAttribute *attribute);
void                 vino_url_unset_attribute_type (VinoURL        *url,
						    PangoAttrType   attr_type);

G_END_DECLS

#endif /* __VINO_URL_H__ */
