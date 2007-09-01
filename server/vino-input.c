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

/*
 * Theory of keyboard operation
 * 
 * The remote VNC client sends us a series of key press and release
 * events identified as X keysyms. (Even Windows clients use X
 * keysyms; that's just how the protocol works.) We then use our
 * knowledge of the keyboard layout to translate the keysyms into
 * keycodes, and use XTEST to send events for the appropriate
 * keycodes.
 * 
 * 
 * Keyboard layouts
 * 
 * The XKEYBOARD extension describes the keyboard as having up to 4
 * "groups" (layouts), and each key on the keyboard has some number of
 * "levels" corresponding to combinations of modifier keys. Different
 * keys may have different numbers of levels, with different meanings.
 * The "Alt-Gr" key on the keyboard is bound to the keysym
 * ISO_Level3_Shift, which is a modifer that switches to a level
 * called "Level3" on some keys.
 * 
 * The core X protocol (on non-XKB servers) has a simpler model, in
 * which there are normally 2 groups, and each group has 2 levels
 * (unshifted and shifted). The "Alt-Gr" key is bound to the keysym
 * Mode_switch, which is a modifier key that switches to the second
 * group when it is held down.
 * 
 * Our model is a simplification of the XKB model that also fits in
 * well with the core model; there are between 1 and 4 groups
 * (keyboard layouts), and each group has exactly 4 levels (plain,
 * Shift or NumLock, Alt-Gr, and Shift+Alt-Gr).
 * 
 * 
 * Details of key event processing
 * 
 * When a modifier keypress is received, vino sends a KeyPress to the
 * server, and when the corresponding keyrelease event is received, it
 * sends a KeyRelease to the server. But for non-modifier keys, it
 * always sends both a KeyPress and a KeyRelease together when it
 * receives just the keypress event. (This is because we assume that
 * the client side will handle "key repeat", and if we left the key
 * pressed down on the server as well, we'd get double repeating.)
 *
 * Shift and Alt-Gr are treated slightly different from the other
 * modifier keys; they are considered to be just implementation
 * details of how you type a particular keysym. Eg, if you type
 * Control+Shift+? (on a US keyboard), you're typing Control because
 * you want the Control key to be pressed, but you're only typing
 * Shift because it's required to get a "?" rather than a "/".
 * Different keyboard layouts require Shift and Alt-Gr to be used for
 * different keysyms, so if the client and server keyboards are not of
 * the same type, vino will sometimes need to send fake press and
 * release events for Shift and Alt-Gr in order to be able to type the
 * keys the client intended to type. (On keys that have no shifted
 * state, like the arrow and function keys, Shift and Alt-Gr behave
 * as ordinary modifiers, the same way Control and Alt do.)
 * 
 * Two further bits of special behavior are needed for NumLock; first,
 * the second key level, which is normally "Shift", actually
 * corresponds to "NumLock" on keypad keys. Second, since the server's
 * NumLock key could plausibly be locked in either state, we need to
 * take its state into account when typing keypad keys. If the server
 * supports XKB, we use the XkbStateNotify event to track NumLock
 * state changes. If the server doesn't support XKB, we have to
 * explicitly query the modifier state any time the client types a
 * keypad key.
 * 
 * On servers that support XKB, we also have to worry about multiple
 * keyboard layouts. We need to keep track of what keysyms are
 * available in what layouts, and what layout (or "group") the server
 * is currently using. When a keysym arrives, we look for a way to
 * type that keysym in the current group, but if it's only available
 * in a different group, we have to change groups temporarily to type
 * it (in the same way we have to temporarily change modifiers
 * sometimes). Doing this programmatically is a little bit tricky; XKB
 * provides XkbLatchGroup(), but if the layout doesn't use
 * "XkbWrapIntoRange" semantics, then that can only switch to
 * higher-numbered groups. Alternatively, we could find the
 * group-switching keys on the keyboard and use them the way we use
 * the Shift and Alt-Gr keys, but there are several different ways
 * that group switching could be set up, and we'd need to support all
 * of them. Anyway, all of the layouts in the xkeyboard-config package
 * use XkbWrapIntoRange, so XkbLatchGroup() works for us in that case,
 * so we just require that, and don't support multiple groups if the
 * layout uses XkbRedirectIntoRange or XkbClampIntoRange.
 * 
 * Two additional problems show up with Windows-based clients; first,
 * Windows considers Alt-Gr to be equivalent to Control_L + Alt_R, so
 * we have to translate that. Second, Windows clients never send
 * events for dead keys. Instead, if you type a dead acute accent key
 * followed by "e", it sends a single keypress event for XK_eacute.
 * This works fine when the current keyboard layout has a key for that
 * precomposed keysym, but if it doesn't, we need to decompose it back
 * into multiple keypresses. There is no guarantee that the
 * decompositions we use will actually work, but since compositions
 * happen in the library, not the server, there is no way to find out
 * 100% reliably what compositions are available to the window
 * currently being typed into, so this is the best we can do.
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
#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include "vino-util.h"

#define VINO_IS_MODIFIER_KEYSYM(k) (((k) >= XK_Shift_L && (k) <= XK_Hyper_R) || \
                                     (k) == XK_Num_Lock                      || \
                                     (k) == XK_Mode_switch                   || \
                                     (k) == XK_ISO_Level3_Shift)

#define VINO_IS_KEYPAD_KEYSYM(k)    ((k) >= XK_KP_Space && (k) <= XK_KP_Equal)

typedef enum
{
  VINO_LEFT_SHIFT    = 1 << 0,
  VINO_RIGHT_SHIFT   = 1 << 1,
  VINO_LEFT_CONTROL  = 1 << 2,
  VINO_RIGHT_ALT     = 1 << 3,
  VINO_ALT_GR        = 1 << 4,
  VINO_NUM_LOCK      = 1 << 5
} VinoModifierState;

typedef enum
{
  VINO_LEVEL_PLAIN        = 0,
  VINO_LEVEL_SHIFT        = 1,
  VINO_LEVEL_ALTGR        = 2,
  VINO_LEVEL_SHIFT_ALTGR  = 3,
  VINO_LEVEL_NUM_LOCK     = 4
} VinoLevel;

#define VINO_NUM_LEVELS 4  /* NumLock is special and doesn't count */
#define VINO_NUM_GROUPS 4

#define VINO_LEVEL(state) ((((state) & (VINO_LEFT_SHIFT | VINO_RIGHT_SHIFT)) ? VINO_LEVEL_SHIFT : 0) | \
			   (((state) & VINO_ALT_GR) ? VINO_LEVEL_ALTGR : 0) | \
			   (((state) & VINO_NUM_LOCK) ? VINO_LEVEL_NUM_LOCK : 0))

#define VINO_LEVEL_IS_SHIFT(level)    (level & VINO_LEVEL_SHIFT)
#define VINO_LEVEL_IS_ALTGR(level)    (level & VINO_LEVEL_ALTGR)
#define VINO_LEVEL_IS_NUM_LOCK(level) (level & VINO_LEVEL_NUM_LOCK)

typedef struct
{
  KeyCode  keycode;
  guint    level;
  gboolean keypad;
} VinoKeybinding;

typedef struct
{
  guint8             button_mask;
  VinoModifierState  modifier_state;
#ifdef HAVE_XKB
  int                current_group;
#endif

  GHashTable        *keybindings;
  GHashTable        *decompositions;
  KeyCode            left_shift_keycode;
  KeyCode            right_shift_keycode;
  KeyCode            left_control_keycode;
  KeyCode            alt_gr_keycode;
  KeyCode            num_lock_keycode;

  KeySym             alt_gr_keysym;
  guint              num_lock_mod;

#ifdef HAVE_XKB
  int                xkb_num_groups;
  int                xkb_event_type;
#endif

  guint              initialized : 1;
  guint              xtest_supported : 1;
  guint              xkb_supported : 1;
  guint              n_pointer_buttons;
} VinoInputData;

/* Data is per-display, but we only handle a single display.
 */
static VinoInputData global_input_data = { 0, };

#ifdef HAVE_XTEST

static struct {
  guint32 composed;
  guint32 decomposed[3];
} decompositions[] = {
  { XK_Cabovedot, { XK_dead_abovedot, 'C', 0 } },
  { XK_Eabovedot, { XK_dead_abovedot, 'E', 0 } },
  { XK_Gabovedot, { XK_dead_abovedot, 'G', 0 } },
  { XK_Iabovedot, { XK_dead_abovedot, 'I', 0 } },
  { XK_Zabovedot, { XK_dead_abovedot, 'Z', 0 } },
  { XK_cabovedot, { XK_dead_abovedot, 'c', 0 } },
  { XK_eabovedot, { XK_dead_abovedot, 'e', 0 } },
  { XK_gabovedot, { XK_dead_abovedot, 'g', 0 } },
  { XK_idotless,  { XK_dead_abovedot, 'i', 0 } },
  { XK_zabovedot, { XK_dead_abovedot, 'z', 0 } },
  { XK_abovedot,  { XK_dead_abovedot, XK_dead_abovedot, 0 } },

  { XK_Aring,  { XK_dead_abovering, 'A', 0 } },
  { XK_Uring,  { XK_dead_abovering, 'U', 0 } },
  { XK_aring,  { XK_dead_abovering, 'a', 0 } },
  { XK_uring,  { XK_dead_abovering, 'u', 0 } },
  { XK_degree, { XK_dead_abovering, XK_dead_abovering, 0 } },

  { XK_Aacute, { XK_dead_acute, 'A', 0 } },
  { XK_Cacute, { XK_dead_acute, 'C', 0 } },
  { XK_Eacute, { XK_dead_acute, 'E', 0 } },
  { XK_Iacute, { XK_dead_acute, 'I', 0 } },
  { XK_Lacute, { XK_dead_acute, 'L', 0 } },
  { XK_Nacute, { XK_dead_acute, 'N', 0 } },
  { XK_Oacute, { XK_dead_acute, 'O', 0 } },
  { XK_Racute, { XK_dead_acute, 'R', 0 } },
  { XK_Sacute, { XK_dead_acute, 'S', 0 } },
  { XK_Uacute, { XK_dead_acute, 'U', 0 } },
  { XK_Yacute, { XK_dead_acute, 'Y', 0 } },
  { XK_Zacute, { XK_dead_acute, 'Z', 0 } },
  { XK_aacute, { XK_dead_acute, 'a', 0 } },
  { XK_cacute, { XK_dead_acute, 'c', 0 } },
  { XK_eacute, { XK_dead_acute, 'e', 0 } },
  { XK_iacute, { XK_dead_acute, 'i', 0 } },
  { XK_lacute, { XK_dead_acute, 'l', 0 } },
  { XK_nacute, { XK_dead_acute, 'n', 0 } },
  { XK_oacute, { XK_dead_acute, 'o', 0 } },
  { XK_racute, { XK_dead_acute, 'r', 0 } },
  { XK_sacute, { XK_dead_acute, 's', 0 } },
  { XK_uacute, { XK_dead_acute, 'u', 0 } },
  { XK_yacute, { XK_dead_acute, 'y', 0 } },
  { XK_zacute, { XK_dead_acute, 'z', 0 } },
  { XK_acute,  { XK_dead_acute, XK_dead_acute, 0 } },

  { XK_Abreve, { XK_dead_breve, 'A', 0 } },
  { XK_Gbreve, { XK_dead_breve, 'G', 0 } },
  { XK_Ubreve, { XK_dead_breve, 'U', 0 } },
  { XK_abreve, { XK_dead_breve, 'a', 0 } },
  { XK_gbreve, { XK_dead_breve, 'g', 0 } },
  { XK_ubreve, { XK_dead_breve, 'u', 0 } },
  { XK_breve,  { XK_dead_breve, XK_dead_breve, 0 } },

  { XK_Ccaron, { XK_dead_caron, 'C', 0 } },
  { XK_Dcaron, { XK_dead_caron, 'D', 0 } },
  { XK_Ecaron, { XK_dead_caron, 'E', 0 } },
  { XK_Lcaron, { XK_dead_caron, 'L', 0 } },
  { XK_Ncaron, { XK_dead_caron, 'N', 0 } },
  { XK_Rcaron, { XK_dead_caron, 'R', 0 } },
  { XK_Scaron, { XK_dead_caron, 'S', 0 } },
  { XK_Tcaron, { XK_dead_caron, 'T', 0 } },
  { XK_Zcaron, { XK_dead_caron, 'Z', 0 } },
  { XK_ccaron, { XK_dead_caron, 'c', 0 } },
  { XK_dcaron, { XK_dead_caron, 'd', 0 } },
  { XK_ecaron, { XK_dead_caron, 'e', 0 } },
  { XK_lcaron, { XK_dead_caron, 'l', 0 } },
  { XK_ncaron, { XK_dead_caron, 'n', 0 } },
  { XK_rcaron, { XK_dead_caron, 'r', 0 } },
  { XK_scaron, { XK_dead_caron, 's', 0 } },
  { XK_tcaron, { XK_dead_caron, 't', 0 } },
  { XK_zcaron, { XK_dead_caron, 'z', 0 } },
  { XK_caron,  { XK_dead_caron, XK_dead_caron, 0 } },

  { XK_Ccedilla, { XK_dead_cedilla, 'C', 0 } },
  { XK_Gcedilla, { XK_dead_cedilla, 'G', 0 } },
  { XK_Kcedilla, { XK_dead_cedilla, 'K', 0 } },
  { XK_Lcedilla, { XK_dead_cedilla, 'L', 0 } },
  { XK_Ncedilla, { XK_dead_cedilla, 'N', 0 } },
  { XK_Rcedilla, { XK_dead_cedilla, 'R', 0 } },
  { XK_Scedilla, { XK_dead_cedilla, 'S', 0 } },
  { XK_Tcedilla, { XK_dead_cedilla, 'T', 0 } },
  { XK_ccedilla, { XK_dead_cedilla, 'c', 0 } },
  { XK_gcedilla, { XK_dead_cedilla, 'g', 0 } },
  { XK_kcedilla, { XK_dead_cedilla, 'k', 0 } },
  { XK_lcedilla, { XK_dead_cedilla, 'l', 0 } },
  { XK_ncedilla, { XK_dead_cedilla, 'n', 0 } },
  { XK_rcedilla, { XK_dead_cedilla, 'r', 0 } },
  { XK_scedilla, { XK_dead_cedilla, 's', 0 } },
  { XK_tcedilla, { XK_dead_cedilla, 't', 0 } },
  { XK_cedilla,  { XK_dead_cedilla, XK_dead_cedilla, 0 } },

  { XK_Acircumflex, { XK_dead_circumflex, 'A', 0 } },
  { XK_Ccircumflex, { XK_dead_circumflex, 'C', 0 } },
  { XK_Ecircumflex, { XK_dead_circumflex, 'E', 0 } },
  { XK_Gcircumflex, { XK_dead_circumflex, 'G', 0 } },
  { XK_Hcircumflex, { XK_dead_circumflex, 'H', 0 } },
  { XK_Icircumflex, { XK_dead_circumflex, 'I', 0 } },
  { XK_Jcircumflex, { XK_dead_circumflex, 'J', 0 } },
  { XK_Ocircumflex, { XK_dead_circumflex, 'O', 0 } },
  { XK_Scircumflex, { XK_dead_circumflex, 'S', 0 } },
  { XK_Ucircumflex, { XK_dead_circumflex, 'U', 0 } },
  { XK_acircumflex, { XK_dead_circumflex, 'a', 0 } },
  { XK_ccircumflex, { XK_dead_circumflex, 'c', 0 } },
  { XK_ecircumflex, { XK_dead_circumflex, 'e', 0 } },
  { XK_gcircumflex, { XK_dead_circumflex, 'g', 0 } },
  { XK_hcircumflex, { XK_dead_circumflex, 'h', 0 } },
  { XK_icircumflex, { XK_dead_circumflex, 'i', 0 } },
  { XK_jcircumflex, { XK_dead_circumflex, 'j', 0 } },
  { XK_ocircumflex, { XK_dead_circumflex, 'o', 0 } },
  { XK_scircumflex, { XK_dead_circumflex, 's', 0 } },
  { XK_ucircumflex, { XK_dead_circumflex, 'u', 0 } },
  { XK_asciicircum, { XK_dead_circumflex, XK_dead_circumflex, 0 } },

  { XK_Adiaeresis, { XK_dead_diaeresis, 'A', 0 } },
  { XK_Ediaeresis, { XK_dead_diaeresis, 'E', 0 } },
  { XK_Idiaeresis, { XK_dead_diaeresis, 'I', 0 } },
  { XK_Odiaeresis, { XK_dead_diaeresis, 'O', 0 } },
  { XK_Udiaeresis, { XK_dead_diaeresis, 'U', 0 } },
  { XK_adiaeresis, { XK_dead_diaeresis, 'a', 0 } },
  { XK_ediaeresis, { XK_dead_diaeresis, 'e', 0 } },
  { XK_idiaeresis, { XK_dead_diaeresis, 'i', 0 } },
  { XK_odiaeresis, { XK_dead_diaeresis, 'o', 0 } },
  { XK_udiaeresis, { XK_dead_diaeresis, 'u', 0 } },
  { XK_ydiaeresis, { XK_dead_diaeresis, 'y', 0 } },
  { XK_diaeresis,  { XK_dead_diaeresis, XK_dead_diaeresis, 0 } },

  { XK_Odoubleacute, { XK_dead_doubleacute, 'O', 0 } },
  { XK_Udoubleacute, { XK_dead_doubleacute, 'U', 0 } },
  { XK_odoubleacute, { XK_dead_doubleacute, 'o', 0 } },
  { XK_udoubleacute, { XK_dead_doubleacute, 'u', 0 } },
  { XK_doubleacute,  { XK_dead_doubleacute, XK_dead_doubleacute, 0 } },

  { XK_Agrave, { XK_dead_grave, 'A', 0 } },
  { XK_Egrave, { XK_dead_grave, 'E', 0 } },
  { XK_Igrave, { XK_dead_grave, 'I', 0 } },
  { XK_Ograve, { XK_dead_grave, 'O', 0 } },
  { XK_Ugrave, { XK_dead_grave, 'U', 0 } },
  { XK_agrave, { XK_dead_grave, 'a', 0 } },
  { XK_egrave, { XK_dead_grave, 'e', 0 } },
  { XK_igrave, { XK_dead_grave, 'i', 0 } },
  { XK_ograve, { XK_dead_grave, 'o', 0 } },
  { XK_ugrave, { XK_dead_grave, 'u', 0 } },
  { XK_grave,  { XK_dead_grave, XK_dead_grave, 0 } },

  { XK_Amacron, { XK_dead_macron, 'A', 0 } },
  { XK_Emacron, { XK_dead_macron, 'E', 0 } },
  { XK_Imacron, { XK_dead_macron, 'I', 0 } },
  { XK_Omacron, { XK_dead_macron, 'O', 0 } },
  { XK_Umacron, { XK_dead_macron, 'U', 0 } },
  { XK_amacron, { XK_dead_macron, 'a', 0 } },
  { XK_emacron, { XK_dead_macron, 'e', 0 } },
  { XK_imacron, { XK_dead_macron, 'i', 0 } },
  { XK_omacron, { XK_dead_macron, 'o', 0 } },
  { XK_umacron, { XK_dead_macron, 'u', 0 } },
  { XK_macron,  { XK_dead_macron, XK_dead_macron, 0 } },

  { XK_Aogonek, { XK_dead_ogonek, 'A', 0 } },
  { XK_Eogonek, { XK_dead_ogonek, 'E', 0 } },
  { XK_Iogonek, { XK_dead_ogonek, 'I', 0 } },
  { XK_Uogonek, { XK_dead_ogonek, 'U', 0 } },
  { XK_aogonek, { XK_dead_ogonek, 'a', 0 } },
  { XK_eogonek, { XK_dead_ogonek, 'e', 0 } },
  { XK_iogonek, { XK_dead_ogonek, 'i', 0 } },
  { XK_uogonek, { XK_dead_ogonek, 'u', 0 } },
  { XK_ogonek,  { XK_dead_ogonek, XK_dead_ogonek, 0 } },

  { XK_Atilde, 	 { XK_dead_tilde, 'A', 0 } },
  { XK_Itilde, 	 { XK_dead_tilde, 'I', 0 } },
  { XK_Ntilde, 	 { XK_dead_tilde, 'N', 0 } },
  { XK_Otilde, 	 { XK_dead_tilde, 'O', 0 } },
  { XK_Utilde, 	 { XK_dead_tilde, 'U', 0 } },
  { XK_atilde, 	 { XK_dead_tilde, 'a', 0 } },
  { XK_itilde, 	 { XK_dead_tilde, 'i', 0 } },
  { XK_ntilde, 	 { XK_dead_tilde, 'n', 0 } },
  { XK_otilde, 	 { XK_dead_tilde, 'o', 0 } },
  { XK_utilde, 	 { XK_dead_tilde, 'u', 0 } },
  { XK_asciitilde, { XK_dead_tilde, XK_dead_tilde, 0 } },

  { XK_Greek_ALPHAaccent,   { XK_dead_acute, XK_Greek_ALPHA, 0 } },
  { XK_Greek_EPSILONaccent, { XK_dead_acute, XK_Greek_EPSILON, 0 } },
  { XK_Greek_ETAaccent,     { XK_dead_acute, XK_Greek_ETA, 0 } },
  { XK_Greek_IOTAaccent,    { XK_dead_acute, XK_Greek_IOTA, 0 } },
  { XK_Greek_OMICRONaccent, { XK_dead_acute, XK_Greek_OMICRON, 0 } },
  { XK_Greek_UPSILONaccent, { XK_dead_acute, XK_Greek_UPSILON, 0 } },
  { XK_Greek_OMEGAaccent,   { XK_dead_acute, XK_Greek_OMEGA, 0 } },
  { XK_Greek_alphaaccent,   { XK_dead_acute, XK_Greek_alpha, 0 } },
  { XK_Greek_epsilonaccent, { XK_dead_acute, XK_Greek_epsilon, 0 } },
  { XK_Greek_etaaccent,     { XK_dead_acute, XK_Greek_eta, 0 } },
  { XK_Greek_iotaaccent,    { XK_dead_acute, XK_Greek_iota, 0 } },
  { XK_Greek_omicronaccent, { XK_dead_acute, XK_Greek_omicron, 0 } },
  { XK_Greek_upsilonaccent, { XK_dead_acute, XK_Greek_upsilon, 0 } },
  { XK_Greek_omegaaccent,   { XK_dead_acute, XK_Greek_omega, 0 } },

  { XK_Greek_IOTAdieresis,    { XK_dead_diaeresis, XK_Greek_IOTA, 0 } },
  { XK_Greek_UPSILONdieresis, { XK_dead_diaeresis, XK_Greek_UPSILON, 0 } },
  { XK_Greek_iotadieresis,    { XK_dead_diaeresis, XK_Greek_iota, 0 } },
  { XK_Greek_upsilondieresis, { XK_dead_diaeresis, XK_Greek_upsilon, 0 } },

  { XK_Greek_iotaaccentdieresis,    { XK_dead_acute, XK_dead_diaeresis, XK_Greek_iota } },
  { XK_Greek_upsilonaccentdieresis, { XK_dead_acute, XK_dead_diaeresis, XK_Greek_upsilon } }
};
static const int num_decompositions = G_N_ELEMENTS (decompositions);

static void vino_input_initialize_keycodes (Display *xdisplay);

static void
vino_input_initialize_keycodes_core (Display *xdisplay)
{
  int      min_keycodes, max_keycodes;
  int      keysyms_per_keycode;
  KeySym   sym;
  int      keycode, level, i;
  XModifierKeymap *modmap;

  global_input_data.alt_gr_keysym = XK_Mode_switch;

  XDisplayKeycodes (xdisplay, &min_keycodes, &max_keycodes);
  XGetKeyboardMapping (xdisplay, min_keycodes, 0, &keysyms_per_keycode);

  /* We iterate by level first, then by keycode, to ensure that we
   * find an unshifted match for each keysym, if possible.
   */
  for (level = 0; level < MAX (keysyms_per_keycode, VINO_NUM_LEVELS); level++)
    {
      for (keycode = min_keycodes; keycode < max_keycodes; keycode++)
	{
	  VinoKeybinding *binding;
	  gboolean unmodifiable = FALSE;

	  sym = XKeycodeToKeysym (xdisplay, keycode, level);
	  if (sym == NoSymbol)
	    continue;

	  if (g_hash_table_lookup (global_input_data.keybindings,
				   GUINT_TO_POINTER (sym)))
	    continue;

	  binding = g_new (VinoKeybinding, VINO_NUM_GROUPS);
	  g_hash_table_insert (global_input_data.keybindings,
			       GUINT_TO_POINTER (sym), binding);

	  /* There's only one group in plain X, so use "binding[0]". */
	  binding[0].keycode = keycode;
	  binding[0].keypad = VINO_IS_KEYPAD_KEYSYM (sym);

	  /* Check if this is a key like XK_uparrow that doesn't change
	   * when you press Shift/Alt-Gr.
	   */
	  if (level == VINO_LEVEL_PLAIN)
	    {
	      KeySym shiftsym, altgrsym;

	      shiftsym = XKeycodeToKeysym (xdisplay, keycode, VINO_LEVEL_SHIFT);
	      altgrsym = (keysyms_per_keycode <= VINO_LEVEL_ALTGR) ? NoSymbol :
		XKeycodeToKeysym (xdisplay, keycode, VINO_LEVEL_ALTGR);

	      unmodifiable = (shiftsym == NoSymbol) && (altgrsym == NoSymbol);
	    }

	  if (unmodifiable)
	    binding[0].level = -1;
	  else if (binding[0].keypad && level == VINO_LEVEL_SHIFT)
	    binding[0].level = VINO_LEVEL_NUM_LOCK;
	  else
	    binding[0].level = level;

	  dprintf (INPUT, "\t0x%.2x (%s) -> key %d level %d\n", (guint)sym,
		   XKeysymToString (sym), keycode, level);
	}
    }

  /* Find the modifier mask corresponding to NumLock */
  keycode = XKeysymToKeycode (xdisplay, XK_Num_Lock);
  modmap = XGetModifierMapping (xdisplay);
  for (i = 0; i < 8 * modmap->max_keypermod; i++)
    {
      if (modmap->modifiermap[i] == keycode)
	{
	  global_input_data.num_lock_mod = 1 << (i / modmap->max_keypermod);
	  break;
	}
    }
  XFreeModifiermap (modmap);
}

#ifdef HAVE_XKB
static void
vino_input_initialize_keycodes_xkb (Display *xdisplay)
{
  XkbDescPtr xkb;
  XkbStateRec state;
  int levelmap[XkbMaxKeyTypes][VINO_NUM_LEVELS], kc, kt, level, group;
  int LevelThreeMask;
  VinoKeybinding *binding;
  KeySym sym;

  global_input_data.alt_gr_keysym = XK_ISO_Level3_Shift;

  xkb = XkbGetMap (xdisplay, XkbAllClientInfoMask, XkbUseCoreKbd);
  g_assert (xkb != NULL);

  LevelThreeMask = XkbKeysymToModifiers (xdisplay, XK_ISO_Level3_Shift);

  /* XKB's levels don't map to VinoLevel, and in fact, may be
   * different for different types of keys (eg, the second level on
   * the function keys is Ctrl+Alt, not Shift). So we create
   * "levelmap" to map VinoLevel values to keytype-specific levels.
   */
  for (kt = 0; kt < xkb->map->num_types; kt++)
    {
      XkbKeyTypePtr type;
      XkbModsPtr mods;
      int ktl;

      levelmap[kt][VINO_LEVEL_PLAIN] = 0;
      levelmap[kt][VINO_LEVEL_SHIFT] = -1;
      levelmap[kt][VINO_LEVEL_ALTGR] = -1;
      levelmap[kt][VINO_LEVEL_SHIFT_ALTGR] = -1;

      type = &xkb->map->types[kt];
      for (ktl = 0; ktl < type->map_count; ktl++)
	{
	  if (!type->map[ktl].active)
	    continue;

	  mods = &type->map[ktl].mods;
	  if (mods->mask == ShiftMask)
	    levelmap[kt][VINO_LEVEL_SHIFT] = type->map[ktl].level;
	  else if (mods->mask == LevelThreeMask)
	    levelmap[kt][VINO_LEVEL_ALTGR] = type->map[ktl].level;
	  else if (mods->mask == (ShiftMask | LevelThreeMask))
	    levelmap[kt][VINO_LEVEL_SHIFT_ALTGR] = type->map[ktl].level;
	}

      if (levelmap[kt][VINO_LEVEL_SHIFT] == -1)
	levelmap[kt][VINO_LEVEL_SHIFT] = levelmap[kt][VINO_LEVEL_PLAIN];
      if (levelmap[kt][VINO_LEVEL_ALTGR] == -1)
	levelmap[kt][VINO_LEVEL_ALTGR] = levelmap[kt][VINO_LEVEL_PLAIN];
      if (levelmap[kt][VINO_LEVEL_SHIFT_ALTGR] == -1)
	levelmap[kt][VINO_LEVEL_SHIFT_ALTGR] = levelmap[kt][VINO_LEVEL_SHIFT];
    }

  /* Now map out the keysyms. As in the core case, we iterate levels
   * before keycodes, so our first match for a keysym in each group
   * will be unshifted if possible.
   */
  for (group = 0; group < VINO_NUM_GROUPS; group++)
    {
      for (level = 0; level < VINO_NUM_LEVELS; level++)
	{
	  for (kc = xkb->min_key_code; kc <= xkb->max_key_code; kc++)
	    {
	      int ngroups, kgroup;

	      ngroups = XkbKeyNumGroups (xkb, kc);
	      if (ngroups == 0)
		continue;

	      /* If the key has fewer groups than the keyboard as a
	       * whole does, figure out which of the key's groups
	       * ("kgroup") will end up being used when the keyboard
	       * as a whole is using group "group".
	       */
	      if (group >= ngroups)
		{
		  if (xkb->map->key_sym_map[kc].group_info & XkbRedirectIntoRange)
		    kgroup = 0;
		  else if (xkb->map->key_sym_map[kc].group_info & XkbClampIntoRange)
		    kgroup = ngroups - 1;
		  else
		    kgroup = group % ngroups;
		}
	      else
		kgroup = group;

	      kt = xkb->map->key_sym_map[kc].kt_index[kgroup];
	      sym = XkbKeySymEntry (xkb, kc, levelmap[kt][level], kgroup);
	      if (!sym)
		continue;

	      binding = g_hash_table_lookup (global_input_data.keybindings,
					     GUINT_TO_POINTER (sym));
	      if (!binding)
		{
		  binding = g_new0 (VinoKeybinding, VINO_NUM_GROUPS);
		  g_hash_table_insert (global_input_data.keybindings,
				       GUINT_TO_POINTER (sym), binding);
		}

	      if (!binding[group].keycode)
		{
		  binding[group].keycode = kc;
		  binding[group].keypad = (kt == XkbKeypadIndex);
		  if (xkb->map->types[kt].num_levels == 1)
		    binding[group].level = -1;
		  else if (binding[group].keypad && level == VINO_LEVEL_SHIFT)
		    binding[group].level = VINO_LEVEL_NUM_LOCK;
		  else
		    binding[group].level = level;
		}

	      if (kgroup == group)
		{
		  dprintf (INPUT, "\t0x%.2x (%s) -> key %d group %d level %d\n",
			   (guint)sym, XKeysymToString (sym), kc, group, level);
		}
	    }
	}
    }

  /* Figure out how switching between groups works; we only support
   * XkbWrapIntoRange. (In theory, xkb->ctrls->group_info should equal
   * XkbWrapIntoRange (ie, 0) when that's the case, but in practice
   * there seems to be junk in the lower 4 bits, so we test for the
   * absence of the other two flags instead.)
   */
  if (XkbGetControls (xdisplay, XkbGroupsWrapMask, xkb) == Success)
    {
      if (!(xkb->ctrls->groups_wrap & (XkbClampIntoRange | XkbRedirectIntoRange)))
	{
	  dprintf (INPUT, "%d groups configured\n", xkb->ctrls->num_groups);
	  global_input_data.xkb_num_groups = xkb->ctrls->num_groups;
	}
      else if (xkb->ctrls->num_groups > 1)
	{
	  dprintf (INPUT, "groups_wrap is not WrapIntoRange, can't use\n");
	  global_input_data.xkb_num_groups = -1;
	}
    }

  /* Find NumLock modifier mask and get initial NumLock state */
  global_input_data.num_lock_mod = XkbKeysymToModifiers (xdisplay, XK_Num_Lock);
  if (XkbGetState (xdisplay, XkbUseCoreKbd, &state) == Success)
    {
      if (state.locked_mods & global_input_data.num_lock_mod)
	global_input_data.modifier_state |= VINO_NUM_LOCK;
    }

  XkbFreeKeyboard (xkb, 0, True);
}

static GdkFilterReturn
xkb_event_filter (GdkXEvent *xevent, GdkEvent *event, gpointer user_data)
{
  VinoInputData *data = user_data;
  XkbStateNotifyEvent *state;

  if (((XEvent *)xevent)->type == data->xkb_event_type)
    {
      switch (((XkbAnyEvent *)xevent)->xkb_type)
	{
	case XkbStateNotify:
	  state = (XkbStateNotifyEvent *)xevent;

	  if (state->changed & XkbGroupStateMask)
	    {
	      data->current_group = state->group;
	      dprintf (INPUT, "current_group -> %d\n", data->current_group);
	    }
	  if (state->changed & XkbModifierLockMask)
	    {
	      if (state->locked_mods & data->num_lock_mod)
		data->modifier_state |= VINO_NUM_LOCK;
	      else
		data->modifier_state &= ~VINO_NUM_LOCK;
	      dprintf (INPUT, "locked_mods -> %u, modifier_state -> %u\n",
		       state->locked_mods, data->modifier_state);
	    }
	  break;

	case XkbMapNotify:
	  XkbRefreshKeyboardMapping ((XkbMapNotifyEvent *)xevent);
	  /* fall through */

	case XkbControlsNotify:
	  vino_input_initialize_keycodes (((XkbAnyEvent *)xevent)->display);
	  break;
	}
    }

  return GDK_FILTER_CONTINUE;
}
#endif

static void
vino_input_initialize_keycodes (Display *xdisplay)
{
  dprintf (INPUT, "Initializing keysym to keycode/modifier mapping\n");

  if (global_input_data.keybindings)
    g_hash_table_destroy (global_input_data.keybindings);
  global_input_data.keybindings =
    g_hash_table_new_full (NULL, NULL, NULL, g_free);

#ifdef HAVE_XKB
  if (global_input_data.xkb_supported)
    vino_input_initialize_keycodes_xkb (xdisplay);
#endif
  if (!global_input_data.xkb_supported)
    vino_input_initialize_keycodes_core (xdisplay);

  global_input_data.left_shift_keycode   = XKeysymToKeycode (xdisplay, XK_Shift_L);
  global_input_data.right_shift_keycode  = XKeysymToKeycode (xdisplay, XK_Shift_R);
  global_input_data.left_control_keycode = XKeysymToKeycode (xdisplay, XK_Control_L);
  global_input_data.num_lock_keycode     = XKeysymToKeycode (xdisplay, XK_Num_Lock);
  global_input_data.alt_gr_keycode       = XKeysymToKeycode (xdisplay, global_input_data.alt_gr_keysym);
}
#endif /* HAVE_XTEST */

gboolean
vino_input_init (GdkDisplay *display)
{
#ifdef HAVE_XTEST
  Display *xdisplay;
  int      ignore, *i = &ignore;
  int      d;

  g_assert (global_input_data.initialized != TRUE);

  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  if (XTestQueryExtension (xdisplay, i, i, i, i))
    {
      XTestGrabControl (xdisplay, True);

      global_input_data.xtest_supported = TRUE;
    }

#ifdef HAVE_XKB
  if (XkbQueryExtension (xdisplay, NULL, &global_input_data.xkb_event_type,
			 NULL, NULL, NULL))
    {
      XkbStateRec state;
  global_input_data.n_pointer_buttons = XGetPointerMapping(xdisplay, NULL, 0);

      dprintf (INPUT, "Using XKB\n");

      XkbGetState (xdisplay, XkbUseCoreKbd, &state);
      global_input_data.current_group = state.group;

      XkbSelectEventDetails (xdisplay, XkbUseCoreKbd, XkbStateNotify,
			     XkbGroupStateMask | XkbModifierLockMask,
			     XkbGroupStateMask | XkbModifierLockMask);
      XkbSelectEventDetails (xdisplay, XkbUseCoreKbd, XkbMapNotify,
			     XkbAllClientInfoMask, XkbAllClientInfoMask);
      XkbSelectEventDetails (xdisplay, XkbUseCoreKbd, XkbControlsNotify,
			     XkbGroupsWrapMask, XkbGroupsWrapMask);

      gdk_window_add_filter (NULL, xkb_event_filter, &global_input_data);

      global_input_data.xkb_supported = TRUE;
    }
  else
#endif
    global_input_data.xkb_supported = FALSE;

  global_input_data.decompositions = g_hash_table_new (NULL, NULL);
  for (d = 0; d < num_decompositions; d++)
    {
      g_hash_table_insert (global_input_data.decompositions,
			   GUINT_TO_POINTER (decompositions[d].composed),
			   decompositions[d].decomposed);
    }

  vino_input_initialize_keycodes (xdisplay);

  global_input_data.initialized = TRUE;

  return global_input_data.xtest_supported;
#else
  return global_input_data.xtest_supported = FALSE;
#endif /* HAVE_XTEST */
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

  for (i = 0; i < global_input_data.n_pointer_buttons; i++)
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

/* If @key_press is %TRUE, adjusts the modifier state on the keyboard
 * such that it's possible to type @binding. If @key_press is %FALSE,
 * undoes that.
 */
static void
vino_input_fake_modifiers (Display           *xdisplay,
			   VinoInputData     *input_data,
			   VinoKeybinding    *binding,
			   gboolean           key_press)
{
  VinoModifierState modifier_state = input_data->modifier_state;
  int cur_level;

  if (binding->level == -1)
    {
      /* The keysym is unaffected by modifier keys, so we should leave
       * the modifiers in their current state.
       */
      return;
    }

  if (binding->keypad && !input_data->xkb_supported)
    {
      Window root, child;
      int rx, ry, wx, wy;
      unsigned int mask;

      XQueryPointer (xdisplay, RootWindow (xdisplay, DefaultScreen (xdisplay)),
		     &root, &child, &rx, &ry, &wx, &wy, &mask);
      if (mask & input_data->num_lock_mod)
	modifier_state |= VINO_NUM_LOCK;
      else
	modifier_state &= ~VINO_NUM_LOCK;
    }

  cur_level = VINO_LEVEL (modifier_state);

  if (VINO_LEVEL_IS_SHIFT (cur_level) && !VINO_LEVEL_IS_SHIFT (binding->level))
    {
      dprintf (INPUT, "Faking Shift %s\n", key_press ? "release" : "press");

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

  if (!VINO_LEVEL_IS_SHIFT (cur_level) && VINO_LEVEL_IS_SHIFT (binding->level))
    {
      dprintf (INPUT, "Faking Shift %s\n", key_press ? "press" : "release");

      XTestFakeKeyEvent (xdisplay,
			 input_data->left_shift_keycode,
			 key_press,
			 CurrentTime);
    }

  if (VINO_LEVEL_IS_ALTGR (cur_level) != VINO_LEVEL_IS_ALTGR (binding->level))
    {
      gboolean want_pressed = VINO_LEVEL_IS_ALTGR (binding->level) ? key_press : !key_press;

      dprintf (INPUT, "Faking Alt-Gr %s\n", want_pressed ? "press" : "release");

      XTestFakeKeyEvent (xdisplay,
			 input_data->alt_gr_keycode,
			 want_pressed,
			 CurrentTime);
    }

  if (VINO_LEVEL_IS_NUM_LOCK (cur_level) != VINO_LEVEL_IS_NUM_LOCK (binding->level))
    {
      dprintf (INPUT, "Faking NumLock press/release\n");

      XTestFakeKeyEvent (xdisplay,
			 input_data->num_lock_keycode,
			 True,
			 CurrentTime);
      XTestFakeKeyEvent (xdisplay,
			 input_data->num_lock_keycode,
			 False,
			 CurrentTime);
    }
}

static gboolean
vino_input_fake_keypress (Display *xdisplay, guint32 keysym)
{
  VinoKeybinding *binding = g_hash_table_lookup (global_input_data.keybindings,
						 GUINT_TO_POINTER (keysym));

  if (binding)
    {
      int group;

#ifdef HAVE_XKB
      if (global_input_data.xkb_supported)
	{
	  if (binding[global_input_data.current_group].keycode)
	    group = global_input_data.current_group;
	  else if (global_input_data.xkb_num_groups > 1)
	    {
	      for (group = 0; group < VINO_NUM_GROUPS; group ++)
		{
		  if (binding[group].keycode)
		    {
		      dprintf (INPUT, "Latching group to %d\n", group);
		      XkbLatchGroup (xdisplay, XkbUseCoreKbd,
				     (group - global_input_data.current_group) %
				     global_input_data.xkb_num_groups);
		      break;
		    }
		}
	      if (group == VINO_NUM_GROUPS)
		return FALSE;
	    }
	  else
	    return FALSE;
	}
      else
#endif
	group = 0;

      vino_input_fake_modifiers (xdisplay, &global_input_data,
				 &binding[group], TRUE);

      dprintf (INPUT, "Injecting keysym 0x%.2x (%s) press/release (via keycode %d, level %d)\n",
	       keysym, XKeysymToString (keysym), binding[group].keycode, binding[group].level);

      XTestFakeKeyEvent (xdisplay, binding[group].keycode, TRUE,  CurrentTime);
      XTestFakeKeyEvent (xdisplay, binding[group].keycode, FALSE, CurrentTime);

      vino_input_fake_modifiers (xdisplay, &global_input_data,
				 &binding[group], FALSE);

      return TRUE;
    }
  else
    {
      guint32 *decomposition =
	g_hash_table_lookup (global_input_data.decompositions,
			     GUINT_TO_POINTER (keysym));
      int i;

      if (decomposition)
	{
	  for (i = 0; i < 3 && decomposition[i]; i++)
	    {
	      if (!vino_input_fake_keypress (xdisplay, decomposition[i]))
		return FALSE;
	    }

	  return TRUE;
	}
    }

  return FALSE;
}
#endif /* HAVE_XTEST */

void
vino_input_handle_key_event (GdkScreen *screen,
			     guint32    keysym,
			     gboolean   key_press)
{
#ifdef HAVE_XTEST
  Display *xdisplay;

  dprintf (INPUT, "Got key %s for %s\n", key_press ? "press" : "release",
	   XKeysymToString (keysym));

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
				    VINO_LEFT_CONTROL, XK_Control_L,
				    keysym, key_press);

  vino_input_update_modifier_state (&global_input_data,
				    VINO_ALT_GR, global_input_data.alt_gr_keysym,
				    keysym, key_press);

  if (!VINO_IS_MODIFIER_KEYSYM (keysym))
    {
      vino_input_fake_keypress (xdisplay, keysym);
    }
  else if (key_press && keysym == XK_Alt_R &&
	   (global_input_data.modifier_state & VINO_LEFT_CONTROL))
    {
      /* Windows translates Alt-Gr to XK_Ctrl_L + XK_Alt_R */

      dprintf (INPUT, "Translating Ctrl+Alt press into Alt-Gr\n");

      XTestFakeKeyEvent (xdisplay, global_input_data.left_control_keycode,
			 FALSE,  CurrentTime);
      XTestFakeKeyEvent (xdisplay, global_input_data.alt_gr_keycode,
			 TRUE, CurrentTime);

      global_input_data.modifier_state |= (VINO_RIGHT_ALT | VINO_ALT_GR);
    }
  else if (!key_press && keysym == XK_Control_L &&
	   (global_input_data.modifier_state & VINO_RIGHT_ALT))
    {
      dprintf (INPUT, "Translating Ctrl+Alt release into Alt-Gr\n");

      XTestFakeKeyEvent (xdisplay, global_input_data.alt_gr_keycode,
			 FALSE, CurrentTime);

      global_input_data.modifier_state &= ~(VINO_RIGHT_ALT | VINO_ALT_GR);
    }
  else if (keysym != XK_Caps_Lock && keysym != XK_Num_Lock)
    {
      KeyCode keycode;

      if ((keycode = XKeysymToKeycode (xdisplay, keysym)) != NoSymbol)
	{
	  dprintf (INPUT, "Injecting keysym 0x%.2x (%s) %s (keycode %d)\n",
		   keysym, XKeysymToString (keysym),
		   key_press ? "press" : "release", keycode);

	  XTestFakeKeyEvent (xdisplay, keycode, key_press, CurrentTime);
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
