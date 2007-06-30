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

#include <config.h>

#include "vino-prefs.h"

#include <string.h>
#include <gconf/gconf-client.h>
#include "vino-util.h"
#include "vino-mdns.h"

#define VINO_PREFS_DIR                    "/desktop/gnome/remote_access"
#define VINO_PREFS_ENABLED                VINO_PREFS_DIR "/enabled"
#define VINO_PREFS_PROMPT_ENABLED         VINO_PREFS_DIR "/prompt_enabled"
#define VINO_PREFS_VIEW_ONLY              VINO_PREFS_DIR "/view_only"
#define VINO_PREFS_LOCAL_ONLY             VINO_PREFS_DIR "/local_only"
#define VINO_PREFS_USE_ALTERNATIVE_PORT   VINO_PREFS_DIR "/use_alternative_port"
#define VINO_PREFS_ALTERNATIVE_PORT       VINO_PREFS_DIR "/alternative_port"
#define VINO_PREFS_REQUIRE_ENCRYPTION     VINO_PREFS_DIR "/require_encryption"
#define VINO_PREFS_AUTHENTICATION_METHODS VINO_PREFS_DIR "/authentication_methods"
#define VINO_PREFS_VNC_PASSWORD           VINO_PREFS_DIR "/vnc_password"
#define VINO_PREFS_LOCK_SCREEN            VINO_PREFS_DIR "/lock_screen_on_disconnect"

#define VINO_N_LISTENERS 10

static GConfClient *vino_client  = NULL;
static GSList      *vino_servers = NULL;
static guint        vino_listeners [VINO_N_LISTENERS] = { 0, };

static gboolean        vino_enabled              = FALSE;
static gboolean        vino_prompt_enabled       = FALSE;
static gboolean        vino_view_only            = FALSE;
static gboolean        vino_local_only           = FALSE;
static gboolean        vino_require_encryption   = FALSE;
static VinoAuthMethod  vino_auth_methods         = VINO_AUTH_VNC;
static char           *vino_vnc_password         = NULL;
static gboolean        vino_use_alternative_port = FALSE;
static int             vino_alternative_port     = VINO_SERVER_DEFAULT_PORT;
static gboolean        vino_lock_screen          = FALSE;


static void
vino_prefs_enabled_changed (GConfClient *client,
			    guint        cnxn_id,
			    GConfEntry  *entry)
{
  gboolean  enabled;
  GSList   *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;
  
  enabled = gconf_value_get_bool (entry->value) != FALSE;

  if (vino_enabled == enabled)
    return;

  vino_enabled = enabled;
  
  dprintf (PREFS, "Access enabled changed: %s\n", vino_enabled ? "(true)" : "(false)");

  if (vino_enabled)
    vino_mdns_start ();
  else
    vino_mdns_stop ();

  for (l = vino_servers; l; l = l->next)
    vino_server_set_on_hold (l->data, !enabled);
}

static void
vino_prefs_prompt_enabled_changed (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry)
{
  gboolean  prompt_enabled;
  GSList   *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;
  
  prompt_enabled = gconf_value_get_bool (entry->value) != FALSE;

  if (vino_prompt_enabled == prompt_enabled)
    return;

  vino_prompt_enabled = prompt_enabled;
  
  dprintf (PREFS, "Prompt enabled changed: %s\n", vino_prompt_enabled ? "(true)" : "(false)");

  for (l = vino_servers; l; l = l->next)
    vino_server_set_prompt_enabled (l->data, prompt_enabled);
}

static void
vino_prefs_view_only_changed (GConfClient *client,
			      guint        cnxn_id,
			      GConfEntry  *entry)
{
  gboolean  view_only;
  GSList   *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;
  
  view_only = gconf_value_get_bool (entry->value) != FALSE;

  if (vino_view_only == view_only)
    return;

  vino_view_only = view_only;
  
  dprintf (PREFS, "View only changed: %s\n", vino_view_only ? "(true)" : "(false)");

  for (l = vino_servers; l; l = l->next)
    vino_server_set_view_only (l->data, view_only);
}

static void
vino_prefs_local_only_changed (GConfClient *client,
			       guint	    cnxn_id,
			       GConfEntry  *entry)
{
  gboolean  local_only;
  GSList   *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  local_only = gconf_value_get_bool (entry->value) != FALSE;

  if (vino_local_only == local_only)
    return;

  vino_local_only = local_only;

  dprintf (PREFS, "Local only changed: %s\n", vino_local_only ? "(true)" : "(false)");

  for (l = vino_servers; l; l = l->next)
    vino_server_set_local_only (l->data, local_only);
}

static void
vino_prefs_require_encryption_changed (GConfClient *client,
				       guint        cnxn_id,
				       GConfEntry  *entry)
{
  gboolean  require_encryption;
  GSList   *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;
  
  require_encryption = gconf_value_get_bool (entry->value) != FALSE;

  if (vino_require_encryption == require_encryption)
    return;

  vino_require_encryption = require_encryption;

  dprintf (PREFS, "Require encryption changed: %s\n", vino_require_encryption ? "(true)" : "(false)");
      
  for (l = vino_servers; l; l = l->next)
    vino_server_set_require_encryption (l->data, require_encryption);
}

static VinoAuthMethod
vino_prefs_translate_auth_methods_list (GSList   *auth_methods_list,
					gboolean  value_list)
{
  VinoAuthMethod  retval = VINO_AUTH_INVALID;
  GSList         *l;

  for (l = auth_methods_list; l; l = l->next)
    {
      const char *method;

      if (value_list)
	method = gconf_value_get_string (l->data);
      else
	method = l->data;
    
      dprintf (PREFS, " %s", method);

      if (!g_ascii_strcasecmp (method, "none"))
	retval |= VINO_AUTH_NONE;

      else if (!g_ascii_strcasecmp (method, "vnc"))
	retval |= VINO_AUTH_VNC;
    }

  return retval != VINO_AUTH_INVALID ? retval : VINO_AUTH_NONE;
}

static void
vino_prefs_authentication_methods_changed (GConfClient *client,
					   guint        cnxn_id,
					   GConfEntry  *entry)
{
  VinoAuthMethod  auth_methods;
  GSList         *auth_methods_list;
  GSList         *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_LIST ||
      gconf_value_get_list_type (entry->value) != GCONF_VALUE_STRING)
    return;
  
  auth_methods_list = gconf_value_get_list (entry->value);

  dprintf (PREFS, "Authentication methods changed:");
  auth_methods = vino_prefs_translate_auth_methods_list (auth_methods_list, TRUE);
  dprintf (PREFS, "\n");

  if (vino_auth_methods == auth_methods)
    return;

  vino_auth_methods = auth_methods;

  for (l = vino_servers; l; l = l->next)
    vino_server_set_auth_methods (l->data, auth_methods);
}

static void
vino_prefs_vnc_password_changed (GConfClient *client,
				 guint        cnxn_id,
				 GConfEntry  *entry)
{
  const char *vnc_password;
  GSList     *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
    return;
  
  vnc_password = gconf_value_get_string (entry->value);

  if (!vnc_password && !vino_vnc_password)
    return;

  if (vnc_password && vino_vnc_password &&
      !strcmp (vnc_password, vino_vnc_password))
    return;

  if (vino_vnc_password)
    g_free (vino_vnc_password);
  vino_vnc_password = g_strdup (vnc_password);

  dprintf (PREFS, "Encoded password changed: %s\n",
	   vino_vnc_password ? vino_vnc_password : "(null)");

  for (l = vino_servers; l; l = l->next)
    vino_server_set_vnc_password (l->data, vnc_password);
}

static void
vino_prefs_use_alternative_port_changed (GConfClient *client,
                                         guint        cnxn_id,
                                         GConfEntry  *entry)
{
  gboolean  enabled;
  GSList   *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  enabled = gconf_value_get_bool (entry->value) != FALSE;

  if (vino_use_alternative_port == enabled)
    return;

  vino_use_alternative_port = enabled;

  dprintf (PREFS, "Use alternative port changed: %s\n",
           vino_use_alternative_port ? "(true)" : "(false)");

  for (l = vino_servers; l; l = l->next)
    vino_server_set_use_alternative_port (l->data, vino_use_alternative_port);
}

static void
vino_prefs_alternative_port_changed (GConfClient *client,
                                     guint        cnxn_id,
                                     GConfEntry  *entry)
{
  GSList *l;
  int     port;

  if (!entry->value || entry->value->type != GCONF_VALUE_INT)
    return;

  port = gconf_value_get_int (entry->value);

  if (vino_alternative_port == port || !VINO_SERVER_VALID_PORT (port))
    return;

  vino_alternative_port = port;

  dprintf (PREFS, "Alternative port changed: %d\n", vino_alternative_port);

  for (l = vino_servers; l; l = l->next)
    vino_server_set_alternative_port (l->data, vino_alternative_port);
}

static void
vino_prefs_lock_screen_changed (GConfClient *client,
			        guint	    cnxn_id,
			        GConfEntry  *entry)
{
  gboolean  lock_screen;
  GSList   *l;

  if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
    return;

  lock_screen = gconf_value_get_bool (entry->value) != FALSE;

  if (vino_lock_screen == lock_screen)
    return;

  vino_lock_screen = lock_screen;

  dprintf (PREFS, "Lock Screen changed: %s\n", vino_lock_screen ? "(true)" : "(false)");

  for (l = vino_servers; l; l = l->next)
    vino_server_set_lock_screen (l->data, lock_screen);
}

void
vino_prefs_create_server (GdkScreen *screen)
{
  VinoServer *server;

  server = g_object_new (VINO_TYPE_SERVER,
			 "prompt-enabled",       vino_prompt_enabled,
			 "view-only",            vino_view_only,
			 "local-only",           vino_local_only,
			 "use-alternative-port", vino_use_alternative_port,
			 "alternative-port",     vino_alternative_port,
			 "auth-methods",         vino_auth_methods,
			 "require-encryption",   vino_require_encryption,
			 "vnc-password",         vino_vnc_password,
			 "on-hold",              !vino_enabled,
			 "screen",               screen,
                         "lock-screen",          vino_lock_screen,
			 NULL);

  vino_servers = g_slist_prepend (vino_servers, server);
  if (vino_enabled)
    vino_mdns_start ();
}

void
vino_prefs_init (gboolean view_only)
{
  GSList *auth_methods_list, *l;
  int i = 0;

  vino_client = gconf_client_get_default ();

  gconf_client_add_dir (vino_client,
			VINO_PREFS_DIR,
			GCONF_CLIENT_PRELOAD_ONELEVEL,
			NULL);

  vino_enabled = gconf_client_get_bool (vino_client, VINO_PREFS_ENABLED, NULL);
  dprintf (PREFS, "Access enabled: %s\n", vino_enabled ? "(true)" : "(false)");

  vino_prompt_enabled = gconf_client_get_bool (vino_client,
					       VINO_PREFS_PROMPT_ENABLED,
					       NULL);
  dprintf (PREFS, "Prompt enabled: %s\n", vino_prompt_enabled ? "(true)" : "(false)");

  if (view_only)
    {
      vino_view_only = TRUE;
    }
  else
    {
      vino_view_only = gconf_client_get_bool (vino_client,
					      VINO_PREFS_VIEW_ONLY,
					      NULL);
    }
  dprintf (PREFS, "View only: %s\n", vino_view_only ? "(true)" : "(false)");
      
  vino_local_only = gconf_client_get_bool (vino_client,
					   VINO_PREFS_LOCAL_ONLY,
					   NULL);
  dprintf (PREFS, "Local only: %s\n", vino_local_only ? "(true)" : "(false)");

  vino_use_alternative_port = gconf_client_get_bool (vino_client,
                                                     VINO_PREFS_USE_ALTERNATIVE_PORT,
                                                     NULL);
  dprintf (PREFS, "Use alternative port: %s\n",
           vino_use_alternative_port ? "(true)" : "(false)");

  vino_alternative_port = gconf_client_get_int (vino_client,
                                                VINO_PREFS_ALTERNATIVE_PORT,
                                                NULL);
  if (!VINO_SERVER_VALID_PORT (vino_alternative_port))
    vino_alternative_port = VINO_SERVER_DEFAULT_PORT;
  dprintf (PREFS, "Alternative port: %d\n", vino_alternative_port);

  vino_require_encryption = gconf_client_get_bool (vino_client,
						   VINO_PREFS_REQUIRE_ENCRYPTION,
						   NULL);
  dprintf (PREFS, "Require encryption: %s\n", vino_require_encryption ? "(true)" : "(false)");

  auth_methods_list = gconf_client_get_list (vino_client,
					     VINO_PREFS_AUTHENTICATION_METHODS,
					     GCONF_VALUE_STRING,
					     NULL);

  dprintf (PREFS, "Authentication methods:");
  vino_auth_methods = vino_prefs_translate_auth_methods_list (auth_methods_list, FALSE);
  dprintf (PREFS, "\n");

  for (l = auth_methods_list; l; l = l->next)
    g_free (l->data);
  g_slist_free (auth_methods_list);

  vino_vnc_password = gconf_client_get_string (vino_client,
					       VINO_PREFS_VNC_PASSWORD,
					       NULL);
  dprintf (PREFS, "Encoded password: %s\n", vino_vnc_password ? vino_vnc_password : "(null)");

  vino_lock_screen = gconf_client_get_bool (vino_client,
                                            VINO_PREFS_LOCK_SCREEN,
                                            NULL);
  dprintf (PREFS, "Lock screen on disconnect: %s\n",
           vino_lock_screen ? "(true)" : "(false)");

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_ENABLED,
			     (GConfClientNotifyFunc) vino_prefs_enabled_changed,
			     NULL, NULL, NULL);
  i++;

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_PROMPT_ENABLED,
			     (GConfClientNotifyFunc) vino_prefs_prompt_enabled_changed,
			     NULL, NULL, NULL);
  i++;

  if (!view_only)
    {
      vino_listeners [i] =
	gconf_client_notify_add (vino_client,
				 VINO_PREFS_VIEW_ONLY,
				 (GConfClientNotifyFunc) vino_prefs_view_only_changed,
				 NULL, NULL, NULL);
    }
  i++;

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_LOCAL_ONLY,
			     (GConfClientNotifyFunc) vino_prefs_local_only_changed,
			     NULL, NULL, NULL);
  i++;

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_USE_ALTERNATIVE_PORT,
			     (GConfClientNotifyFunc) vino_prefs_use_alternative_port_changed,
			     NULL, NULL, NULL);
  i++;


  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_ALTERNATIVE_PORT,
			     (GConfClientNotifyFunc) vino_prefs_alternative_port_changed,
			     NULL, NULL, NULL);
  i++;

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_REQUIRE_ENCRYPTION,
			     (GConfClientNotifyFunc) vino_prefs_require_encryption_changed,
			     NULL, NULL, NULL);
  i++;

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_AUTHENTICATION_METHODS,
			     (GConfClientNotifyFunc) vino_prefs_authentication_methods_changed,
			     NULL, NULL, NULL);
  i++;

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_VNC_PASSWORD,
			     (GConfClientNotifyFunc) vino_prefs_vnc_password_changed,
			     NULL, NULL, NULL);
  i++;

  vino_listeners [i] =
    gconf_client_notify_add (vino_client,
			     VINO_PREFS_LOCK_SCREEN,
			     (GConfClientNotifyFunc) vino_prefs_lock_screen_changed,
			     NULL, NULL, NULL);

  i++;

  g_assert (i == VINO_N_LISTENERS);
}

void
vino_prefs_shutdown (void)
{
  GSList *l;
  int     i;

  for (l = vino_servers; l; l = l->next)
    g_object_unref (l->data);
  g_slist_free (vino_servers);
  vino_servers = NULL;

  if (vino_vnc_password)
    g_free (vino_vnc_password);
  vino_vnc_password = NULL;
      
  for (i = 0; i < VINO_N_LISTENERS; i++) {
    if (vino_listeners [i])
      gconf_client_notify_remove (vino_client, vino_listeners [i]);
    vino_listeners [i] = 0;
  }

  g_object_unref (vino_client);
  vino_client = NULL;
}
