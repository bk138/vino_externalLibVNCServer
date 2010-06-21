/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * vino-message-box.c
 * Copyright (C) Jonh Wendell 2009 <wendell@bani.com.br>
 * 
 * vino-message-box.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * vino-message-box.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VINO_MESSAGE_BOX_H_
#define _VINO_MESSAGE_BOX_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VINO_TYPE_MESSAGE_BOX             (vino_message_box_get_type ())
#define VINO_MESSAGE_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VINO_TYPE_MESSAGE_BOX, VinoMessageBox))
#define VINO_MESSAGE_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VINO_TYPE_MESSAGE_BOX, VinoMessageBoxClass))
#define VINO_IS_MESSAGE_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VINO_TYPE_MESSAGE_BOX))
#define VINO_IS_MESSAGE_BOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VINO_TYPE_MESSAGE_BOX))
#define VINO_MESSAGE_BOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VINO_TYPE_MESSAGE_BOX, VinoMessageBoxClass))

typedef struct _VinoMessageBoxClass VinoMessageBoxClass;
typedef struct _VinoMessageBox VinoMessageBox;
typedef struct _VinoMessageBoxPrivate VinoMessageBoxPrivate;

struct _VinoMessageBoxClass
{
  GtkInfoBarClass parent_class;
};

struct _VinoMessageBox
{
  GtkInfoBar parent_instance;
  VinoMessageBoxPrivate *priv;
};

GType vino_message_box_get_type (void) G_GNUC_CONST;

GtkWidget	*vino_message_box_new (void);

void		vino_message_box_set_label  (VinoMessageBox *box, const gchar *label);
void		vino_message_box_show_image (VinoMessageBox *box);
void		vino_message_box_hide_image (VinoMessageBox *box);

G_END_DECLS

#endif /* _VINO_MESSAGE_BOX_H_ */
