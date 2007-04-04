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
 *
 *
 *   The keyboard and pointer handling code is borrowed from
 *   x11vnc.c in libvncserver/contrib which is:
 *
 *     Copyright (c) 2002-2003 Karl J. Runge <runge@karlrunge.com>
 *
 *   x11vnc.c itself is based heavily on:
 *       the originial x11vnc.c in libvncserver (Johannes E. Schindelin)
 *       krfb, the KDE desktopsharing project (Tim Jansen)
 *       x0rfbserver, the original native X vnc server (Jens Wagner)
 */

#include <config.h>

#include "vino-input.h"

#include <string.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif

#include "vino-util.h"

/* See <X11/keysymdef.h> - "Latin 1: Byte 3 and 4 = 0"
 */
#define VINO_IS_LATIN1_KEYSYM(k) ((k) != NoSymbol && ((k) & 0xffffff00) == 0)

#define VINO_IS_MODIFIER_KEYSYM(k) (((k) >= XK_Shift_L && (k) <= XK_Hyper_R) || \
                                     (k) == XK_Mode_switch                   || \
                                     (k) == XK_ISO_Level3_Shift)

typedef enum
{
  VINO_LEFT_SHIFT  = 1 << 0,
  VINO_RIGHT_SHIFT = 1 << 1,
  VINO_ALT_GR      = 1 << 2
} VinoModifierState;

#define VINO_LEFT_OR_RIGHT_SHIFT (VINO_LEFT_SHIFT | VINO_RIGHT_SHIFT)

typedef struct
{
  guint8            button_mask;

  VinoModifierState modifier_state;
  guint8            modifiers [0x100];
  KeyCode           keycodes [0x100];
  KeyCode           left_shift_keycode;
  KeyCode           right_shift_keycode;
  KeyCode           alt_gr_keycode;

  guint             initialized : 1;
  guint             xtest_supported : 1;
} VinoInputData;

/* Data is per-display, but we only handle a single display.
 */
static VinoInputData global_input_data = { 0, };

/* Set up a keysym -> keycode + modifier mapping.
 *
 * RFB transmits the KeySym for a keypress, but we may only inject
 * keycodes using XTest. Thus, we must ensure that the modifier
 * state is such that the keycode we inject maps to the KeySym
 * we received from the client.
 */
#ifdef HAVE_XTEST
static void
vino_input_initialize_keycodes (GdkDisplay *display)
{
  Display *xdisplay;
  int      min_keycodes, max_keycodes;
  int      keysyms_per_keycode;
  int      keycode;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  memset (global_input_data.keycodes,   0, sizeof (global_input_data.keycodes));
  memset (global_input_data.modifiers, -1, sizeof (global_input_data.modifiers));

  XDisplayKeycodes (xdisplay, &min_keycodes, &max_keycodes);

  g_assert (min_keycodes >= 8);
  g_assert (max_keycodes <= 255);

  XGetKeyboardMapping (xdisplay, min_keycodes, 0, &keysyms_per_keycode);

  dprintf (INPUT, "Initializing keysym to keycode/modifier mapping\n");

  for (keycode = min_keycodes; keycode < max_keycodes; keycode++)
    {
      guint8 modifier;

      for (modifier = 0; modifier < keysyms_per_keycode; modifier++)
	{
	  guint32 keysym = XKeycodeToKeysym (xdisplay, keycode, modifier);

	  if (VINO_IS_LATIN1_KEYSYM (keysym))
	    {
	      if (global_input_data.keycodes [keysym] != 0)
		continue;

	      global_input_data.keycodes  [keysym] = keycode;
	      global_input_data.modifiers [keysym] = modifier;

	      dprintf (INPUT, "\t0x%.2x -> %d %d\n", keysym, keycode, modifier);
	    }
	}
    }

  global_input_data.left_shift_keycode  = XKeysymToKeycode (xdisplay, XK_Shift_L);
  global_input_data.right_shift_keycode = XKeysymToKeycode (xdisplay, XK_Shift_R);
  global_input_data.alt_gr_keycode      = XKeysymToKeycode (xdisplay, XK_Mode_switch);
}
#endif /* HAVE_XTEST */

gboolean
vino_input_init (GdkDisplay *display)
{
#ifdef HAVE_XTEST
  Display *xdisplay;
  int      ignore, *i = &ignore;

  g_assert (global_input_data.initialized != TRUE);

  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  if (XTestQueryExtension (xdisplay, i, i, i, i))
    {
      XTestGrabControl (xdisplay, True);

      global_input_data.xtest_supported = TRUE;
    }

  vino_input_initialize_keycodes (display);

  global_input_data.initialized = TRUE;

  return global_input_data.xtest_supported;
#else
  return global_input_data.xtest_supported = FALSE;
#endif /* HAVE_XSHM */
}

void
vino_input_handle_pointer_event (GdkScreen *screen,
				 guint8     button_mask,
				 guint16    x,
				 guint16    y)
{
#ifdef HAVE_XTEST
  Display *xdisplay;
  guint8   prev_mask = global_input_data.button_mask;
  int      i;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

  XTestFakeMotionEvent (xdisplay,
			gdk_screen_get_number (screen),
			x, y,
			CurrentTime);
  
  dprintf (INPUT, "Injected motion event: %d, %d\n", x, y);

  for (i = 0; i < 5; i++)
    {
      gboolean button_down      = (button_mask & (1 << i)) != FALSE;
      gboolean prev_button_down = (prev_mask   & (1 << i)) != FALSE;

      if (button_down != prev_button_down)
	{
	  XTestFakeButtonEvent (xdisplay, i + 1, button_down, CurrentTime);
	  
	  dprintf (INPUT, "Injected button %d %s\n",
		   i + 1, button_down ? "press" : "release");
	}
    }

  global_input_data.button_mask = button_mask;
#endif /* HAVE_XTEST */
}

#ifdef HAVE_XTEST
static inline void
vino_input_update_modifier_state (VinoInputData    *input_data,
				  VinoModifierState state,
				  guint32           check_keysym,
				  guint32           keysym,
				  gboolean          key_press)
{
  if (keysym == check_keysym)
    {
      if (key_press)
	input_data->modifier_state |= state;
      else
	input_data->modifier_state &= ~state;
    }
}

static void
vino_input_fake_modifier (GdkScreen         *screen,
			  VinoInputData     *input_data,
			  guint8             modifier,
			  gboolean           key_press)
{
  Display           *xdisplay;
  VinoModifierState  modifier_state = input_data->modifier_state;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

  if ((modifier_state & VINO_LEFT_OR_RIGHT_SHIFT) && modifier != 1)
    {
      dprintf (INPUT, "Shift is down, but we don't want it to be\n");

      if (modifier_state & VINO_LEFT_SHIFT)
	XTestFakeKeyEvent (xdisplay,
			   input_data->left_shift_keycode,
			   !key_press,
			   CurrentTime);
      
      if (modifier_state & VINO_RIGHT_SHIFT)
	XTestFakeKeyEvent (xdisplay,
			   input_data->right_shift_keycode,
			   !key_press,
			   CurrentTime);
    }

  if (!(modifier_state & VINO_LEFT_OR_RIGHT_SHIFT) && modifier == 1)
    {
      dprintf (INPUT, "Shift isn't down, but we want it to be\n");

      XTestFakeKeyEvent (xdisplay,
			 input_data->left_shift_keycode,
			 key_press,
			 CurrentTime);
    }
      
  if ((modifier_state & VINO_ALT_GR) && modifier != 2)
    {
      dprintf (INPUT, "Alt is down, but we don't want it to be\n");

      XTestFakeKeyEvent (xdisplay,
			 input_data->alt_gr_keycode,
			 !key_press,
			 CurrentTime);
    }
      
  if (!(modifier_state & VINO_ALT_GR) && modifier == 2)
    {
      dprintf (INPUT, "Alt isn't down, but we want it to be\n");

      XTestFakeKeyEvent (xdisplay,
			 input_data->alt_gr_keycode,
			 key_press,
			 CurrentTime);
    }
}
#endif /* HAVE_XTEST */

void
vino_input_handle_key_event (GdkScreen *screen,
			     guint32    keysym,
			     gboolean   key_press)
{
#ifdef HAVE_XTEST
  Display *xdisplay;

  /* 
   * We inject a key press/release pair for all key presses 
   * and ignore key releases. The exception is modifiers.
   */

  if (!key_press && !VINO_IS_MODIFIER_KEYSYM (keysym))
    return;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));
  
  vino_input_update_modifier_state (&global_input_data,
				    VINO_LEFT_SHIFT, XK_Shift_L,
				    keysym, key_press);

  vino_input_update_modifier_state (&global_input_data,
				    VINO_RIGHT_SHIFT, XK_Shift_R,
				    keysym, key_press);

  vino_input_update_modifier_state (&global_input_data,
				    VINO_ALT_GR, XK_Mode_switch,
				    keysym, key_press);

  if (VINO_IS_LATIN1_KEYSYM (keysym))
    {
      KeyCode keycode  = global_input_data.keycodes [keysym];
      guint8  modifier = global_input_data.modifiers [keysym];

      if (keycode != NoSymbol)
	{
	  g_assert (key_press != FALSE);

	  vino_input_fake_modifier (screen, &global_input_data, modifier, TRUE);

	  dprintf (INPUT, "Injecting keysym 0x%.2x %s (keycode %d, modifier %d)\n",
		   keysym, key_press ? "press" : "release", keycode, modifier);

	  XTestFakeKeyEvent (xdisplay, keycode, TRUE,  CurrentTime);
	  XTestFakeKeyEvent (xdisplay, keycode, FALSE, CurrentTime);

	  vino_input_fake_modifier (screen, &global_input_data, modifier, FALSE);
	}
    }
  else if (keysym != XK_Caps_Lock)
    {
      KeyCode keycode;

      if ((keycode = XKeysymToKeycode (xdisplay, keysym)) != NoSymbol)
	{
	  dprintf (INPUT, "Injecting keysym 0x%.2x %s (keycode %d)\n",
		   keysym, key_press ? "press" : "release", keycode);

	  XTestFakeKeyEvent (xdisplay, keycode, key_press, CurrentTime);

	  if (key_press && !VINO_IS_MODIFIER_KEYSYM (keysym))
	    {
	      XTestFakeKeyEvent (xdisplay, keycode, FALSE, CurrentTime);
	    }
	}
    }

  XFlush (xdisplay);

#endif /* HAVE_XTEST */
}

void
vino_input_handle_clipboard_event (GdkScreen *screen,
				   char      *text,
				   int        len)
{
}
