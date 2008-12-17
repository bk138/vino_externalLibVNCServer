/*
 * Copyright (C) 2008 Jonh Wendell
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
 *      Jonh Wendell <wendell@bani.com.br>
 */

#include <miniupnp/miniupnpc.h>
#include <miniupnp/upnpcommands.h>

#include "vino-upnp.h"
#include "vino-util.h"

struct _VinoUpnpPrivate
{
  struct UPNPUrls *urls;
  struct IGDdatas *data;
  char             lanaddr[16];
  gboolean         have_igd;
  int              port;
};

G_DEFINE_TYPE (VinoUpnp, vino_upnp, G_TYPE_OBJECT);

static void
clean_upnp_data (VinoUpnp *upnp)
{
  if (upnp->priv->urls)
    {
      FreeUPNPUrls (upnp->priv->urls);
      g_free (upnp->priv->urls);
      upnp->priv->urls = NULL;
    }

  if (upnp->priv->data)
    {
      g_free (upnp->priv->data);
      upnp->priv->data = NULL;
    }
}

static gboolean
update_upnp_status (VinoUpnp *upnp)
{
  struct UPNPDev * devlist;
  int res;

  if (upnp->priv->have_igd)
    return TRUE;

  clean_upnp_data (upnp);

  dprintf (UPNP, "UPnP: Doing the discovery... ");
  devlist = upnpDiscover (2000, NULL, NULL, 0);
  if (!devlist)
    {
      dprintf (UPNP, "nothing found, aborting.");
      return FALSE;
    }
  dprintf (UPNP, "found.\n");
  dprintf (UPNP, "UPnP: Looking for a valid IGD... ");

  upnp->priv->urls = g_new (struct UPNPUrls, 1);
  upnp->priv->data = g_new (struct IGDdatas, 1);

  res = UPNP_GetValidIGD (devlist,
			  upnp->priv->urls,
			  upnp->priv->data,
                          upnp->priv->lanaddr,
                          sizeof (upnp->priv->lanaddr));

  if (res == 1 || res == 2)
    {
      dprintf (UPNP, "found: %s\n", upnp->priv->urls->controlURL);
      upnp->priv->have_igd = TRUE;
    }
  else
    {
      dprintf (UPNP, "none found, aborting.\n");
      upnp->priv->have_igd = FALSE;
    }

  freeUPNPDevlist (devlist);
  return upnp->priv->have_igd;
}

static void
vino_upnp_finalize (GObject *object)
{
  VinoUpnp *upnp = VINO_UPNP (object);

  clean_upnp_data (upnp);

  G_OBJECT_CLASS (vino_upnp_parent_class)->finalize (object);
}

static void
vino_upnp_dispose (GObject *object)
{
  VinoUpnp *upnp = VINO_UPNP (object);

  vino_upnp_remove_port (upnp);

  G_OBJECT_CLASS (vino_upnp_parent_class)->dispose (object);
}

static void
vino_upnp_class_init (VinoUpnpClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose  = vino_upnp_dispose;
  gobject_class->finalize = vino_upnp_finalize;

  g_type_class_add_private (gobject_class, sizeof (VinoUpnpPrivate));
}

static void
vino_upnp_init (VinoUpnp *upnp)
{
  upnp->priv = G_TYPE_INSTANCE_GET_PRIVATE (upnp, VINO_TYPE_UPNP, VinoUpnpPrivate);

  upnp->priv->urls = NULL;
  upnp->priv->data = NULL;
  upnp->priv->have_igd = FALSE;
  upnp->priv->port = -1;
}

VinoUpnp *
vino_upnp_new (void)
{
  return VINO_UPNP (g_object_new (VINO_TYPE_UPNP, NULL));
}

gchar *
vino_upnp_get_external_ip (VinoUpnp *upnp)
{
  gchar ip[16];

  g_return_val_if_fail (VINO_IS_UPNP (upnp), NULL);

  if (!update_upnp_status (upnp))
    return NULL;

  UPNP_GetExternalIPAddress (upnp->priv->urls->controlURL,
			     upnp->priv->data->servicetype,
			     ip);
  if (ip[0])
    if (strcmp (ip, "0.0.0.0") == 0)
      return NULL;
    else
      return g_strdup (ip);
  else
    return NULL;
}

int
vino_upnp_add_port (VinoUpnp *upnp, int port)
{
  char *ext_port, *int_port, *desc;
  int   err, local_port;
  char  int_client_tmp[16], int_port_tmp[6];

  g_return_val_if_fail (VINO_IS_UPNP (upnp), -1);

  if (!update_upnp_status (upnp))
    return -1;

  vino_upnp_remove_port (upnp);

  local_port = port;
  do
    {
      ext_port = g_strdup_printf ("%d", local_port);
      dprintf (UPNP, "UPnP: Trying to forward port %d...: ", local_port);
      UPNP_GetSpecificPortMappingEntry (upnp->priv->urls->controlURL,
					upnp->priv->data->servicetype,
					ext_port,
					"TCP",
					int_client_tmp,
					int_port_tmp);
      if (int_client_tmp[0])
	{
	  dprintf (UPNP, "Failed, this port is already forwarded to %s:%s\n", int_client_tmp, int_port_tmp);
	  g_free (ext_port);
	}
      else
	{
	  dprintf (UPNP, "OK, this port is free on the router\n");
	  break;
	}

      local_port++;
    } while (local_port < INT_MAX);

  if (local_port == INT_MAX)
    {
      dprintf (UPNP, "UPnP: Not forwarding any port, tried so much\n");
      return -1;
    }

  int_port = g_strdup_printf ("%d", port);
  desc = g_strdup_printf ("VNC: %s@%s",
			  g_get_user_name (),
			  g_get_host_name ());  

  err = UPNP_AddPortMapping (upnp->priv->urls->controlURL,
			     upnp->priv->data->servicetype,
			     ext_port,
			     int_port,
			     upnp->priv->lanaddr,
			     desc,
			     "TCP");
  if (err == 0)
    {
      upnp->priv->port = local_port;
      dprintf (UPNP, "UPnP: Successfuly forwarded port %d\n", local_port);
    }
  else
    dprintf (UPNP, "Failed to forward port %d, with status %d\n", local_port, err);

  g_free (ext_port);
  g_free (int_port);
  g_free (desc);

  return upnp->priv->port;
}

void
vino_upnp_remove_port (VinoUpnp *upnp)
{
  char *port;
  int   err;

  g_return_if_fail (VINO_IS_UPNP (upnp));

  if (upnp->priv->port == -1)
    return;

  if (!update_upnp_status (upnp))
    return;

  port = g_strdup_printf ("%d", upnp->priv->port);
  err = UPNP_DeletePortMapping (upnp->priv->urls->controlURL,
				upnp->priv->data->servicetype,
				port,
				"TCP");
  if (err == 0)
    dprintf (UPNP, "UPnP: Removed forwarded port %d\n", upnp->priv->port);
  else
    dprintf (UPNP, "UPnP: Failed to remove forwarded port %d with status %d\n", upnp->priv->port, err);

  g_free (port);
  upnp->priv->port = -1;
}

int
vino_upnp_get_external_port (VinoUpnp *upnp)
{
  g_return_val_if_fail (VINO_IS_UPNP (upnp), -1);

  return upnp->priv->port;
}
