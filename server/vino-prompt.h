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

#ifndef __VINO_PROMPT_H__
#define __VINO_PROMPT_H__

#include <glib-object.h>
#include <gdk/gdk.h>
#include <rfb/rfb.h>

G_BEGIN_DECLS

#define VINO_TYPE_PROMPT         (vino_prompt_get_type ())
#define VINO_PROMPT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), VINO_TYPE_PROMPT, VinoPrompt))
#define VINO_PROMPT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VINO_TYPE_PROMPT, VinoPromptClass))
#define VINO_IS_PROMPT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), VINO_TYPE_PROMPT))
#define VINO_IS_PROMPT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), VINO_TYPE_PROMPT))
#define VINO_PROMPT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), VINO_TYPE_PROMPT, VinoPromptClass))

typedef struct _VinoPrompt        VinoPrompt;
typedef struct _VinoPromptClass   VinoPromptClass;
typedef struct _VinoPromptPrivate VinoPromptPrivate;

typedef enum
{
  VINO_RESPONSE_INVALID = 0,
  VINO_RESPONSE_ACCEPT  = 1,
  VINO_RESPONSE_REJECT  = 2
} VinoPromptResponse;

struct _VinoPrompt
{
  GObject            base;

  VinoPromptPrivate *priv;
};

struct _VinoPromptClass
{
  GObjectClass base_class;

  void (* response) (VinoPrompt         *prompt,
		     rfbClientPtr        rfb_client,
		     VinoPromptResponse  response);
};

GType       vino_prompt_get_type      (void) G_GNUC_CONST;

VinoPrompt *vino_prompt_new           (GdkScreen    *screen);

void        vino_prompt_add_client    (VinoPrompt   *prompt,
				       rfbClientPtr  rfb_client);
void        vino_prompt_remove_client (VinoPrompt   *prompt,
				       rfbClientPtr  rfb_client);

void        vino_prompt_set_screen    (VinoPrompt   *prompt,
				       GdkScreen    *screen);
GdkScreen  *vino_prompt_get_screen    (VinoPrompt   *prompt);

G_END_DECLS

#endif /* __VINO_PROMPT_H__ */
