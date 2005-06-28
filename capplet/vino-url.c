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

#include <config.h>

#include "vino-url.h"

#include <libintl.h>
#include <gtk/gtktooltips.h>
#include <libgnomeui/gnome-url.h>

#define _(x) dgettext (GETTEXT_PACKAGE, x)

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_TOOLTIP
};

struct _VinoURLPrivate
{
  char          *address;
  char          *tooltip;

  PangoAttrList *attributes;
  GtkTooltips   *tooltips;
  GdkWindow     *event_window;

  guint          button_down : 1;
  guint          foreground_modified : 1;
  guint          underline_modified : 1;
};

static void vino_url_class_init    (VinoURLClass *klass);
static void vino_url_instance_init (VinoURL      *url);

static void vino_url_finalize     (GObject      *object);
static void vino_url_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec);
static void vino_url_get_property (GObject      *object,
				   guint         prop_id,
				   GValue       *value,
				   GParamSpec   *pspec);

static void     vino_url_realize        (GtkWidget        *widget);
static void     vino_url_unrealize      (GtkWidget        *widget);
static void     vino_url_size_allocate  (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static void     vino_url_size_request   (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void     vino_url_map            (GtkWidget        *widget);
static void     vino_url_unmap          (GtkWidget        *widget);
static gboolean vino_url_expose         (GtkWidget        *widget,
					 GdkEventExpose   *event);
static gboolean vino_url_button_press   (GtkWidget        *widget,
					 GdkEventButton   *event);
static gboolean vino_url_button_release (GtkWidget        *widget,
					 GdkEventButton   *event);
static gboolean vino_url_focus          (GtkWidget        *widget,
					 GtkDirectionType  direction);
static void     vino_url_state_changed  (GtkWidget        *widget,
					 GtkStateType      previous_state);

static void vino_url_activate (VinoURL *url);

static void vino_url_change_attribute_internal (VinoURL        *url,
						PangoAttribute *attribute,
						gboolean        internal);
static void vino_url_set_use_underline         (VinoURL        *url,
						gboolean        use_underline);
static void vino_url_set_use_url_color         (VinoURL        *url,
						gboolean        use_underline);

static GtkLabelClass *parent_class = NULL;

GType
vino_url_get_type (void)
{
  static GType url_type = 0;
  
  if (!url_type)
    {
      static const GTypeInfo url_info =
      {
	sizeof (VinoURLClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) vino_url_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (VinoURL),
	0,		/* n_preallocs */
	(GInstanceInitFunc) vino_url_instance_init,
      };
      
      url_type = g_type_register_static (GTK_TYPE_LABEL,
					 "VinoURL",
					 &url_info, 0);
    }
  
  return url_type;
}

static void
vino_url_class_init (VinoURLClass *klass)
{
  GObjectClass  *gobject_class;
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = (GObjectClass *)   klass;
  widget_class  = (GtkWidgetClass *) klass;

  gobject_class->finalize     = vino_url_finalize;
  gobject_class->set_property = vino_url_set_property;
  gobject_class->get_property = vino_url_get_property;

  widget_class->realize              = vino_url_realize;
  widget_class->unrealize            = vino_url_unrealize;
  widget_class->size_request         = vino_url_size_request;
  widget_class->size_allocate        = vino_url_size_allocate;
  widget_class->map                  = vino_url_map;
  widget_class->unmap                = vino_url_unmap;
  widget_class->expose_event         = vino_url_expose;
  widget_class->button_press_event   = vino_url_button_press;
  widget_class->button_release_event = vino_url_button_release;
  widget_class->focus                = vino_url_focus;
  widget_class->state_changed        = vino_url_state_changed;

  klass->activate = vino_url_activate;

  g_object_class_install_property (gobject_class,
				   PROP_ADDRESS,
				   g_param_spec_string ("address",
							_("Address"),
							_("The address pointed to by the widget"),
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_TOOLTIP,
				   g_param_spec_string ("tooltip",
							_("Tooltip"),
							_("A tooltip for this URL"),
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("url-color",
							       _("URL color"),
							       _("The color of the URL's label"),
							       GDK_TYPE_COLOR,
							       G_PARAM_READWRITE));

  widget_class->activate_signal =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (VinoURLClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
vino_url_instance_init (VinoURL *url)
{
  url->priv = g_new0 (VinoURLPrivate, 1);

  vino_url_set_use_url_color (url, TRUE);
  vino_url_set_use_underline (url, TRUE);

  url->priv->tooltips = gtk_tooltips_new ();
  g_object_ref (url->priv->tooltips);
  gtk_object_sink (GTK_OBJECT (url->priv->tooltips));

  /* Chain up to the label's focus handling code
   * which is meant for selection, even though
   * we're not selectable.
   */
  GTK_WIDGET_SET_FLAGS (url, GTK_CAN_FOCUS);
}

static void
vino_url_finalize (GObject *object)
{
  VinoURL *url = VINO_URL (object);

  if (url->priv->address)
    g_free (url->priv->address);
  url->priv->address = NULL;

  if (url->priv->attributes)
    pango_attr_list_unref (url->priv->attributes);
  url->priv->attributes = NULL;

  if (url->priv->tooltips)
    g_object_unref (url->priv->tooltips);
  url->priv->tooltips = NULL;

  if (url->priv->tooltip)
    g_free (url->priv->tooltip);
  url->priv->tooltip = NULL;

  g_free (url->priv);
  url->priv = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
vino_url_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  VinoURL *url = VINO_URL (object);
                                                                                                             
  switch (prop_id)
    {
    case PROP_ADDRESS:
      vino_url_set_address (url, g_value_get_string (value));
      break;
    case PROP_TOOLTIP:
      vino_url_set_tooltip (url, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_url_get_property (GObject    *object,
		       guint       prop_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
  VinoURL *url = VINO_URL (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_string (value, vino_url_get_address (url));
      break;
    case PROP_TOOLTIP:
      g_value_set_string (value, vino_url_get_tooltip (url));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
vino_url_realize (GtkWidget *widget)
{
  VinoURL       *url = VINO_URL (widget);
  GdkWindowAttr  attributes;
  GdkCursor     *cursor = NULL;
  gint           attributes_mask;

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x           = widget->allocation.x;
  attributes.y           = widget->allocation.y;
  attributes.width       = widget->allocation.width;
  attributes.height      = widget->allocation.height;
  attributes.wclass      = GDK_INPUT_ONLY;
  attributes.event_mask  = gtk_widget_get_events (widget) |
				GDK_BUTTON_PRESS_MASK     |
				GDK_BUTTON_RELEASE_MASK   |
				GDK_ENTER_NOTIFY_MASK     |
				GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  if (GTK_WIDGET_IS_SENSITIVE (widget))
    {
      attributes.cursor = cursor =
	gdk_cursor_new_for_display (gtk_widget_get_display (widget),
				    GDK_HAND2);
      attributes_mask |= GDK_WA_CURSOR;
    }

  url->priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					    &attributes, attributes_mask);
  gdk_window_set_user_data (url->priv->event_window, widget);

  if (cursor)
    gdk_cursor_unref (cursor);
}

static void
vino_url_unrealize (GtkWidget *widget)
{
  VinoURL *url = VINO_URL (widget);

  if (url->priv->event_window)
    {
      gdk_window_set_user_data (url->priv->event_window, NULL);
      gdk_window_destroy (url->priv->event_window);
      url->priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
vino_url_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  int focus_width;
  int focus_pad;

  gtk_widget_style_get (widget,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        NULL);

  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  requisition->width  += 2 * (focus_width + focus_pad);
  requisition->height += 2 * (focus_width + focus_pad);
}

static void
vino_url_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  VinoURL *url = VINO_URL (widget);
  
  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (url->priv->event_window,
			      allocation->x,
			      allocation->y,
			      allocation->width,
			      allocation->height);
    }
}

static void
vino_url_map (GtkWidget *widget)
{
  VinoURL *url = VINO_URL (widget);

  GTK_WIDGET_CLASS (parent_class)->map (widget);

  if (url->priv->event_window)
    gdk_window_show (url->priv->event_window);
}

static void
vino_url_unmap (GtkWidget *widget)
{
  VinoURL *url = VINO_URL (widget);

  if (url->priv->event_window)
    gdk_window_hide (url->priv->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static gboolean
vino_url_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  GtkAllocation  real_allocation;
  PangoLayout   *layout;
  int            width, height;
  int            focus_width;
  int            focus_pad;

  gtk_widget_style_get (widget,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        NULL);

  /* We need to fool GtkLabel into drawing the label 
   * in the right place.
   */
  real_allocation = widget->allocation;

  widget->allocation.x      += focus_width + focus_pad;
  widget->allocation.y      += focus_width + focus_pad;
  widget->allocation.width  -= 2 * (focus_width + focus_pad);
  widget->allocation.height -= 2 * (focus_width + focus_pad);

  GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

  layout = gtk_label_get_layout (GTK_LABEL (widget));

  width = height = 0;
  pango_layout_get_pixel_size (layout, &width, &height);

  width  += 2 * (focus_pad + focus_width);
  height += 2 * (focus_pad + focus_width);

  widget->allocation = real_allocation;

  if (GTK_WIDGET_HAS_FOCUS (widget))
    gtk_paint_focus (widget->style,
		     widget->window,
		     GTK_WIDGET_STATE (widget),
		     &event->area,
		     widget,
		     "label",
		     widget->allocation.x,
		     widget->allocation.y,
		     width,
		     height);

  return FALSE;
}

static gboolean
vino_url_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  VinoURL *url = VINO_URL (widget);

  if (event->button == 1 && event->window == url->priv->event_window)
    {
      url->priv->button_down = TRUE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
vino_url_button_release (GtkWidget      *widget,
			 GdkEventButton *event)
{
  VinoURL *url = VINO_URL (widget);

  if (event->button == 1 && url->priv->button_down)
    {
      gtk_widget_activate (widget);
      url->priv->button_down = FALSE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
vino_url_focus (GtkWidget        *widget,
		GtkDirectionType  direction)
{
  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }

  return FALSE;
}

static void
vino_url_state_changed (GtkWidget    *widget,
			GtkStateType  previous_state)
{
  if (GTK_WIDGET_REALIZED (widget))
    {
      if (GTK_WIDGET_IS_SENSITIVE (widget))
	{
	  GdkCursor *cursor;

	  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
					       GDK_HAND2);
	  gdk_window_set_cursor (VINO_URL (widget)->priv->event_window, cursor);
	  gdk_cursor_unref (cursor);
	}
      else
	{
	  gdk_window_set_cursor (VINO_URL (widget)->priv->event_window, NULL);
	}
    }

  vino_url_set_use_url_color (VINO_URL (widget), GTK_WIDGET_IS_SENSITIVE (widget));
}

static void
vino_url_activate (VinoURL *url)
{
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (url));
  GError    *error = NULL;

  if (!url->priv->address)
    return;

  if (!gnome_url_show_on_screen (url->priv->address, screen, &error))
    {
      /* FIXME better error handling!
       *       What best to do? For the specific case
       *       in this preferences dialog we want to be
       *       able to pop up a dialog with the error
       *       but also the vino URL as a selectable
       *       label.
       *
       *       Maybe chain this up to the caller?
       */

      g_warning ("Failed to show URL '%s': %s\n",
		 url->priv->address, error->message);
      g_error_free (error);
    }
}

GtkWidget *
vino_url_new (const char *address,
	      const char *label,
	      const char *tooltip)
{
  g_return_val_if_fail (address != NULL, NULL);

  return g_object_new (VINO_TYPE_URL,
		       "address", address,
		       "label",   label,
		       "tooltip", tooltip,
		       NULL);
}

static void
vino_url_set_use_underline (VinoURL  *url,
			    gboolean  use_underline)
{
  if (!url->priv->underline_modified)
    {
      if (use_underline)
	{
	  vino_url_change_attribute_internal (url,
					      pango_attr_underline_new (PANGO_UNDERLINE_SINGLE),
					      TRUE);
	}
      else
	{
	  vino_url_unset_attribute_type (url, PANGO_ATTR_UNDERLINE);
	}
    }
}

static void
vino_url_set_use_url_color (VinoURL  *url,
			    gboolean  use_url_color)
{
  if (!url->priv->foreground_modified)
    {
      if (use_url_color)
	{
	  GdkColor        blue = { 0, 0x0000, 0x0000, 0xffff };
	  GdkColor       *url_color;
	  PangoAttribute *foreground;

	  gtk_widget_style_get (GTK_WIDGET (url),
				"url-color", &url_color,
				NULL);
	  if (!url_color)
	    url_color = &blue;

	  foreground = pango_attr_foreground_new (url_color->red,
						  url_color->green,
						  url_color->blue);

	  vino_url_change_attribute_internal (url, foreground, TRUE);

	  if (url_color != &blue)
	    gdk_color_free (url_color);
	}
      else
	{
	  vino_url_unset_attribute_type (url, PANGO_ATTR_FOREGROUND);
	}
    }
}

void
vino_url_set_address (VinoURL    *url,
		      const char *address)
{
  g_return_if_fail (VINO_IS_URL (url));

  g_free (url->priv->address);
  url->priv->address = g_strdup (address);

  g_object_notify (G_OBJECT (url), "address");
}

G_CONST_RETURN char *
vino_url_get_address (VinoURL *url)
{
  g_return_val_if_fail (VINO_IS_URL (url), NULL);

  return url->priv->address;
}

void
vino_url_set_tooltip (VinoURL    *url,
		      const char *tooltip)
{
  g_return_if_fail (VINO_IS_URL (url));

  g_free (url->priv->tooltip);
  url->priv->tooltip = g_strdup (tooltip);

  gtk_tooltips_set_tip (url->priv->tooltips,
			GTK_WIDGET (url),
			url->priv->tooltip,
			NULL);

  g_object_notify (G_OBJECT (url), "tooltip");
}

G_CONST_RETURN char *
vino_url_get_tooltip (VinoURL *url)
{
  g_return_val_if_fail (VINO_IS_URL (url), NULL);

  return url->priv->tooltip;
}

/* Debugging; There should probably be a nicer API
 * for fiddling with the attributes on a label;
 * Either that or I'm being dumb again;
 */
#define SANITY_CHECK_ATTRIBUTES

static void
sanity_check_attributes_notify (VinoURL *url)
{
  PangoAttrList *attrs;

  attrs = gtk_label_get_attributes (GTK_LABEL (url));
  if (attrs != url->priv->attributes)
    {
      g_warning ("Label attributes changed, resetting");
      gtk_label_set_attributes (GTK_LABEL (url),
				url->priv->attributes);
    }
}

static void
sanity_check_attributes (VinoURL *url)
{
#ifdef SANITY_CHECK_ATTRIBUTES
  g_signal_connect (url, "notify::attributes",
		    G_CALLBACK (sanity_check_attributes_notify), NULL);
#endif /* SANITY_CHECK_ATTRIBUTES */
}

static void
vino_url_change_attribute_internal (VinoURL        *url,
				    PangoAttribute *attribute,
				    gboolean        internal)
{
  if (!url->priv->attributes)
    {
      url->priv->attributes = pango_attr_list_new ();
      gtk_label_set_attributes (GTK_LABEL (url), url->priv->attributes);
      sanity_check_attributes (url);
    }

  attribute->start_index = 0;
  attribute->end_index = G_MAXINT;

  if (!internal)
    {
      if (attribute->klass->type == PANGO_ATTR_FOREGROUND)
	url->priv->foreground_modified = TRUE;

      if (attribute->klass->type == PANGO_ATTR_UNDERLINE)
	url->priv->underline_modified = TRUE;
    }

  pango_attr_list_change (url->priv->attributes, attribute);
}

void
vino_url_change_attribute (VinoURL        *url,
			   PangoAttribute *attribute)
{
  g_return_if_fail (VINO_IS_URL (url));

  vino_url_change_attribute_internal (url, attribute, FALSE);
}

static gboolean
filter_out_attr_type (PangoAttribute *attribute,
		      gpointer        data)
{
  PangoAttrType attr_type = GPOINTER_TO_INT (data);

  return attribute->klass->type == attr_type;
}

void
vino_url_unset_attribute_type (VinoURL       *url,
			       PangoAttrType  attr_type)
{
  g_return_if_fail (VINO_IS_URL (url));

  if (url->priv->attributes)
    {
      PangoAttrList *filtered;

      filtered =
	pango_attr_list_filter (url->priv->attributes,
				(PangoAttrFilterFunc) filter_out_attr_type,
				GINT_TO_POINTER (attr_type));
      if (filtered)
	pango_attr_list_unref (filtered);
    }
}
