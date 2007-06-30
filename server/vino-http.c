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

#include "vino-http.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <gtk/gtkicontheme.h>

#include "vino-util.h"
#include "vino-mdns.h"

#define VINO_CLIENT_HTML_FILE "vino-client.html"
#define VINO_CLIENT_ARCHIVE   "vino-client.jar"
#define VINO_CLIENT_CODE      "vncviewer/VNCViewer.class"
#define VINO_CLIENT_LOGO      "vino-client.png"

#define VINO_RFB_MIN_PORT        5900
#define VINO_RFB_MAX_PORT        5999
#define VINO_HTTP_MIN_PORT       5800
#define VINO_HTTP_MAX_PORT       5899
#define VINO_HTTP_MAX_HEADER_LEN (1024 * 10)

struct _VinoHTTPPrivate
{
  GPtrArray  *rfb_ports;
  int         http_port;
  int         socket;
  GIOChannel *io_channel;
  guint       io_watch;
  GSList     *clients;
};

typedef struct
{
  VinoHTTP   *http;

  guint       io_in_watch;
  GIOChannel *io_channel;
  int         socket;
  GString    *header;

  guint       io_out_watch;
  GString    *pending_response;
  int         response_index;
} VinoHTTPClient;

static gboolean vino_http_data_writable (GIOChannel     *source,
					 GIOCondition    condition,
					 VinoHTTPClient *client);

static VinoHTTP *singleton_http = NULL;
static gpointer  parent_class;


static inline GString *
file_not_found_response (void)
{
  return g_string_new ("HTTP/1.0 404 Not found\r\n\r\n" \
		       "<html><head><title>File Not Found</title></head>\n" \
		       "<body><h1>File Not Found</h1></body></html\n");
}

static inline GString *
invalid_request_response (void)
{
  return g_string_new ("HTTP/1.0 400 Invalid Request\r\n\r\n" \
		       "<html><head><title>Invalid Request</title></head>\n" \
		       "<body><h1>Invalid Request</h1></body></html\n");
}

static inline GString *
ok_response (void)
{
  return g_string_new ("HTTP/1.0 200 OK\n" \
		       "Content-Type: text/html\r\n\r\n");
}

static GString *
vino_http_insert_applet_text (VinoHTTP *http,
			      GString  *string,
			      int       index)
{
  int i;

  if (!http->priv->rfb_ports)
    return string;

  for (i = http->priv->rfb_ports->len - 1; i >= 0; i--)
    {
      int   port = GPOINTER_TO_UINT (http->priv->rfb_ports->pdata [i]);
      char *text;

      text = g_strdup_printf ("<applet code=\"%s\" archive=\"%s\">" \
			      "<param name=\"port\" value=\"%d\">"  \
			      "</applet>",
			      VINO_CLIENT_CODE, VINO_CLIENT_ARCHIVE, port);

      string = g_string_insert (string, index, text);
      
      g_free (text);
    }

  return string;
}

static struct
{
  char      *name;
  int        name_len;
  GString *(*insert_text) (VinoHTTP *http,
			   GString  *string,
			   int       index);
} vino_http_substitutions [] = {
  { "APPLET", sizeof ("APPLET"), vino_http_insert_applet_text },
};

static GString *
vino_http_perform_substitutions (VinoHTTP *http,
				 GString  *response)
{
  char *p;

  g_return_val_if_fail (response != NULL, NULL);

  p = response->str;
  while ((p = strchr (p, '$')))
    {
      int i;

      for (i = 0; i < G_N_ELEMENTS (vino_http_substitutions); i++)
	{
	  if (g_str_has_prefix (p + 1, vino_http_substitutions [i].name))
	    {
	      int index = p - response->str;

	      response = g_string_erase (response,
					 index,
					 vino_http_substitutions [i].name_len);
	      response = vino_http_substitutions [i].insert_text (http, response, index);
	      p = response->str;
	    }
	}
    }

  return response;
}

static char *
vino_http_lookup_client_logo (void)
{
#define ICON_SIZE_STANDARD 48

  GtkIconTheme *icon_theme;
  GtkIconInfo  *info;
  char         *icon_path = NULL;

  icon_theme = gtk_icon_theme_get_default ();

  info = gtk_icon_theme_lookup_icon (icon_theme,
				     "gnome-remote-desktop",
				     ICON_SIZE_STANDARD,
				     GTK_ICON_LOOKUP_NO_SVG);
  if (info)
    {
      icon_path = g_strdup (gtk_icon_info_get_filename (info));
      gtk_icon_info_free (info);
    }

  return icon_path;
  
#undef ICON_SIZE_STANDARD
}

static GString *
vino_http_construct_response (VinoHTTP       *http,
			      VinoHTTPClient *client,
			      char           *filename)
{
  GString *retval;
  GError  *error;
  char    *path;
  char    *freeme = NULL;
  char    *contents;
  gsize    len;

  if (!filename)
    return invalid_request_response ();
  
  if (!strcmp (filename, "/" VINO_CLIENT_ARCHIVE))
    {
      path = VINO_CLIENTDIR "/" VINO_CLIENT_ARCHIVE;
    }
  else if (!strcmp (filename, "/" VINO_CLIENT_LOGO))
    {
      path = vino_http_lookup_client_logo ();
      freeme = path;
    }
  else
    {
      path = VINO_CLIENTDIR "/" VINO_CLIENT_HTML_FILE;
    }

  if (!path || !g_file_test (path, G_FILE_TEST_EXISTS))
    {
      g_free (freeme);
      g_warning ("Cannot locate \"%s\"", path ? path : filename);
      return file_not_found_response ();
    }

  dprintf (HTTP, "%p: loading \"%s\" in response to request\n", client, path);

  contents = NULL;
  len = 0;
  error = NULL;
  if (!g_file_get_contents (path, &contents, &len, &error))
    {
      g_free (freeme);
      g_warning ("Error opening \"%s\": %s\n", path, error->message);
      g_error_free (error);
      return file_not_found_response ();
    }

  g_free (freeme);

  retval = ok_response ();
  retval = g_string_append_len (retval, contents, len);
  retval = vino_http_perform_substitutions (http, retval);

  g_free (contents);

  return retval;
}

static void
vino_http_finalize_client (VinoHTTP       *http,
			   VinoHTTPClient *client,
			   gboolean        remove_from_list)
{
  dprintf (HTTP, "client %p: Finalizing client\n", client);

  if (remove_from_list)
    http->priv->clients = g_slist_remove (http->priv->clients, client);

  if (client->io_in_watch)
    g_source_remove (client->io_in_watch);
  client->io_in_watch = 0;
  
  if (client->io_out_watch)
    g_source_remove (client->io_out_watch);
  client->io_out_watch = 0;

  if (client->io_channel)
    g_io_channel_unref (client->io_channel);
  client->io_channel = NULL;

  if (client->socket)
    close (client->socket);
  client->socket = 0;

  if (client->pending_response)
    g_string_free (client->pending_response, TRUE);
  client->pending_response = NULL;

  if (client->header)
    g_string_free (client->header, TRUE);
  client->header = NULL;
  
  g_free (client);
}

static void
vino_http_queue_pending_response (VinoHTTP       *http,
				  VinoHTTPClient *client,
				  GString        *response,
				  int             index)
{
  dprintf (HTTP, "client %p: Not finished writing HTTP response ... queueing\n", client);

  if (!client->pending_response)
    {
      g_assert (client->io_out_watch == 0);

      client->pending_response = response;
      client->response_index   = index;
      client->io_out_watch     = g_io_add_watch (client->io_channel,
						 G_IO_OUT,
						 (GIOFunc) vino_http_data_writable,
						 client);
    }
  else
    {
      g_assert (client->pending_response == response);
      g_assert (client->io_out_watch != 0);

      client->response_index = index;
    }
}
				  
static gboolean
vino_http_write_string (VinoHTTP       *http,
			VinoHTTPClient *client,
			GString        *response,
			int             index)
{
  char *p;
  int   len;

  p   = response->str + index;
  len = response->len - index;

  while (len > 0)
    {
      int n;

      n = write (client->socket, p, len);
      if (n == 0)
	{
	  return FALSE;
	}
      else if (n < 0)
	{
	  if (errno == EINTR)
	    {
	      continue;
	    }
	  else if (errno == EAGAIN)
	    {
	      vino_http_queue_pending_response (http, client, response, response->len - len);
	      return TRUE;
	    }
	    
	  g_warning ("Error writing HTTP response: %s\n", g_strerror (errno));
	  return FALSE;
	}

      p   += n;
      len -= n;
    }

  return FALSE;
}

static gboolean
vino_http_data_writable (GIOChannel     *source,
			 GIOCondition    condition,
			 VinoHTTPClient *client)
{
  gboolean retval;

  g_assert (condition == G_IO_OUT);
  g_assert (client->pending_response != NULL);

  dprintf (HTTP, "client %p: Socket writable again ... finishing sending response\n", client);

  retval = vino_http_write_string (client->http,
				   client,
				   client->pending_response,
				   client->response_index);

  if (!retval)
    vino_http_finalize_client (client->http, client, TRUE);

  return retval;
}

static gboolean
vino_http_write_response (VinoHTTP       *http,
			  VinoHTTPClient *client,
			  char           *filename)
{
  GString *response;
  gboolean retval;

  response = vino_http_construct_response (http, client, filename);

  g_assert (response != NULL);

  dprintf (HTTP, "client %p: Writing HTTP response:\n\n%s\n", client, response->str);

  if (!(retval = vino_http_write_string (http, client, response, 0)))
    g_string_free (response, TRUE);

  return retval;
}

static gboolean
vino_http_data_pending (GIOChannel     *source,
			GIOCondition    condition,
			VinoHTTPClient *client)
{
  VinoHTTP *http = client->http;
  char     *p;
  char     *filename;

  g_assert (condition == G_IO_IN || condition == G_IO_PRI);

  dprintf (HTTP, "%p: Processing data from client\n", client);

  while (1)
    {
      char buffer [256];
      int  n;

      n = read (client->socket, buffer, sizeof (buffer) - 1);
      if (n == 0)
	{
	  goto out;
	}
      else if (n < 0)
	{
	  if (errno == EAGAIN)
	    {
	      break;
	    }
	  else if (errno == EINTR)
	    {
	      continue;
	    }
	  
	  g_warning ("Error reading HTTP request: %s\n", g_strerror (errno));
	  goto invalid_request;
	}

      buffer [n] = '\0';
      
      if (!client->header)
	client->header = g_string_new (buffer);
      else
	client->header = g_string_append (client->header, buffer);

      if (client->header->len  > VINO_HTTP_MAX_HEADER_LEN)
	{
	  g_warning ("HTTP header far too long (%d bytes). Disconnecting client",
		     client->header->len);
	  goto invalid_request;
	}
    }

  if (!client->header)
    return TRUE;

  dprintf (HTTP, "client %p: Processing HTTP header:\n\n%s", client, client->header->str);
  
  /* Header not complete yet ? */
  if (!strstr (client->header->str, "\r\r") &&
      !strstr (client->header->str, "\n\n") &&
      !strstr (client->header->str, "\r\n\r\n") &&
      !strstr (client->header->str, "\n\r\n\r"))
    {
      dprintf (HTTP, "client %p: HTTP request not complete:\n\n%s\n\n", client, client->header->str);
      return TRUE;
    }

  client->header = g_string_truncate (client->header,
				      strcspn (client->header->str, "\n\r"));

  g_assert (client->header != NULL);

  p = client->header->str;

  /* Header format is: "GET $path HTTP/1.x" */
  if (client->header->len < 3 ||
      p [0] != 'G' ||
      p [1] != 'E' ||
      p [2] != 'T')
    {
      dprintf (HTTP, "client %p: Not a HTTP GET request\n", client);
      goto invalid_request;
    }

  p = strstr (client->header->str, "HTTP");
  if (!p ||
      p [0] != 'H' ||
      p [1] != 'T' ||
      p [2] != 'T' ||
      p [3] != 'P' ||
      p [4] != '/' ||
      p [5] != '1' ||
      p [6] != '.' ||
      !g_ascii_isdigit (p [7]))
    {
      dprintf (HTTP, "client %p: Not a HTTP/1.x request\n", client);
      goto invalid_request;
    }

  *(--p) = '\0';
  if (p < (client->header->str + 4))
    {
      dprintf (HTTP, "client %p: Not a HTTP/1.x request\n", client);
      goto invalid_request;
    }

  filename = g_strdup (client->header->str + 4);

  dprintf (HTTP, "client %p: HTTP Request is for \"%s\"\n", client, filename);

  if (vino_http_write_response (http, client, filename))
    {
      g_free (filename);
      return TRUE;
    }

  g_free (filename);

 out:
  vino_http_finalize_client (http, client, TRUE);
  
  return FALSE;
  
 invalid_request:
  if (vino_http_write_response (http, client, NULL))
    return TRUE;

  goto out;
}

static gboolean
vino_http_new_connection_pending (GIOChannel   *source,
				  GIOCondition  condition,
				  VinoHTTP     *http)
{
  VinoHTTPClient *client;
  int             sock;

  g_return_val_if_fail (VINO_IS_HTTP (http), FALSE);

  if ((sock = accept (http->priv->socket, NULL, NULL)) < 0)
    {
      g_warning ("Error accepting on socket: %s\n", g_strerror (errno));
      return FALSE;
    }
  
  if (fcntl (sock, F_SETFL, O_NONBLOCK) < 0)
    {
      g_warning ("Error setting O_NONBLOCK flag on socket: %s\n", g_strerror (errno));
      close (sock);
      return FALSE;
    }

  client = g_new0 (VinoHTTPClient, 1);

  client->http        = http;
  client->socket      = sock;
  client->io_channel  = g_io_channel_unix_new (client->socket);
  client->io_in_watch = g_io_add_watch (client->io_channel,
					G_IO_IN|G_IO_PRI,
					(GIOFunc) vino_http_data_pending,
					client);

  http->priv->clients = g_slist_prepend (http->priv->clients, client);
  
  dprintf (HTTP, "client %p: Accepted new HTTP client\n", client);

  return TRUE;
}

static inline int
start_probing_at (int rfb_port)
{
  if (rfb_port >= VINO_RFB_MIN_PORT && rfb_port <= VINO_RFB_MAX_PORT)
    return VINO_HTTP_MIN_PORT + (rfb_port - VINO_RFB_MIN_PORT);

  return VINO_HTTP_MIN_PORT;
}

static void
vino_http_create_listening_socket (VinoHTTP *http)
{
#ifdef ENABLE_IPV6
  struct sockaddr_in6  saddr_in6;
#endif
  struct sockaddr_in   saddr_in;
  struct sockaddr     *saddr;
  socklen_t            saddr_len;
  int                  sock;
  int                  opt;
  int                  http_port;

  if (http->priv->io_watch)
    g_source_remove (http->priv->io_watch);
  http->priv->io_watch = 0;

  if (http->priv->io_channel)
    g_io_channel_unref (http->priv->io_channel);
  http->priv->io_channel = NULL;

  if (http->priv->socket)
    close (http->priv->socket);
  http->priv->socket = 0;

  http->priv->http_port = 0;

  sock = -1;

#ifdef ENABLE_IPV6
  sock = socket (AF_INET6, SOCK_STREAM, 0);

  memset (&saddr_in6, 0, sizeof (struct sockaddr_in6));
  saddr_in6.sin6_family = AF_INET6;
  saddr_in6.sin6_addr = in6addr_any;

  saddr = (struct sockaddr *) &saddr_in6;
  saddr_len = sizeof (saddr_in6);
#endif

  if (sock < 0)
    {
      if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0)
        {
          g_warning ("Error creating socket: %s\n", g_strerror (errno));
          return;
        }

      memset (&saddr_in, 0, sizeof (struct sockaddr_in));
      saddr_in.sin_family = AF_INET;
      saddr_in.sin_addr.s_addr = htonl (INADDR_ANY);

      saddr = (struct sockaddr *) &saddr_in;
      saddr_len = sizeof (saddr_in);
    }

  opt = 1;
  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (int)) < 0)
    {
      g_warning ("Error setting SO_REUSEADDR socket option: %s\n", g_strerror (errno));
      close (sock);
      return;
    }

  if (fcntl (sock, F_SETFL, O_NONBLOCK) < 0)
    {
      g_warning ("Error setting O_NONBLOCK flag on socket: %s\n", g_strerror (errno));
      close (sock);
      return;
    }

  if (http->priv->rfb_ports)
    http_port = start_probing_at (GPOINTER_TO_UINT (http->priv->rfb_ports->pdata [0]));
  else
    http_port = VINO_HTTP_MIN_PORT;

  while (http_port <= VINO_HTTP_MAX_PORT)
    {
#ifdef ENABLE_IPV6
      if (saddr->sa_family == AF_INET6)
        ((struct sockaddr_in6 *) saddr)->sin6_port = htons (http_port);
      else
#endif
        ((struct sockaddr_in *) saddr)->sin_port = htons (http_port);

      if (bind (sock, saddr, saddr_len) == 0 )
	break;
      
      dprintf (HTTP, "Failed to probe port %d: %s\n", http_port, g_strerror (errno));
      ++http_port;
    }

  if (http_port > VINO_HTTP_MAX_PORT)
    {
      g_warning ("Failed to bind socket to port in range 5800-5899: %s\n", g_strerror (errno));
      close (sock);
      return;
    }

  if (listen (sock, 1024) < 0)
    {
      g_warning ("Error listening on socket: %s\n", g_strerror (errno));
      close (sock);
      return;
    }

  dprintf (HTTP, "Listening for HTTP requests on port %d\n", http_port);

  http->priv->http_port  = http_port;
  http->priv->socket     = sock;
  http->priv->io_channel = g_io_channel_unix_new (http->priv->socket);
  http->priv->io_watch   = g_io_add_watch (http->priv->io_channel,
					   G_IO_IN|G_IO_PRI,
					   (GIOFunc) vino_http_new_connection_pending,
					   http);

  vino_mdns_add_service ("_http._tcp", http_port);
}

static void
vino_http_finalize (GObject *object)
{
  VinoHTTP *http = VINO_HTTP (object);
  GSList   *l;

  for (l = http->priv->clients; l; l = l->next)
    vino_http_finalize_client (http, l->data, FALSE);
  g_slist_free (http->priv->clients);
  http->priv->clients = NULL;

  if (http->priv->rfb_ports)
    g_ptr_array_free (http->priv->rfb_ports, TRUE);
  http->priv->rfb_ports = NULL;

  if (http->priv->io_watch)
    g_source_remove (http->priv->io_watch);
  http->priv->io_watch = 0;

  if (http->priv->io_channel)
    g_io_channel_unref (http->priv->io_channel);
  http->priv->io_channel = NULL;

  if (http->priv->socket)
    close (http->priv->socket);
  http->priv->socket = 0;
  
  g_free (http->priv);
  http->priv = NULL;

  singleton_http = NULL;

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
vino_http_instance_init (VinoHTTP *http)
{
  http->priv = g_new0 (VinoHTTPPrivate, 1);
}

static void
vino_http_class_init (VinoHTTPClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  gobject_class->finalize = vino_http_finalize;
}

GType
vino_http_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (VinoHTTPClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) vino_http_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (VinoHTTP),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) vino_http_instance_init,
	};
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "VinoHTTP",
                                            &object_info, 0);
    }

  return object_type;
}


VinoHTTP *
vino_http_get (int rfb_port)
{
  if (!singleton_http)
    singleton_http = g_object_new (VINO_TYPE_HTTP, NULL);
  else
    g_object_ref (singleton_http);

  vino_http_add_rfb_port (singleton_http, rfb_port);

  return singleton_http;
}

static int
vino_http_sort_ports (gconstpointer a,
		      gconstpointer b)
{
  return GPOINTER_TO_UINT (a) - GPOINTER_TO_UINT (b);
}

void
vino_http_add_rfb_port (VinoHTTP *http,
			int       rfb_port)
{
  g_return_if_fail (VINO_IS_HTTP (http));
  g_return_if_fail (rfb_port > 0);

  if (!http->priv->rfb_ports)
    http->priv->rfb_ports = g_ptr_array_new ();

  g_ptr_array_add (http->priv->rfb_ports, GUINT_TO_POINTER (rfb_port));
  g_ptr_array_sort (http->priv->rfb_ports, vino_http_sort_ports);

  if (!http->priv->socket)
    vino_http_create_listening_socket (http);
}

void
vino_http_remove_rfb_port (VinoHTTP *http,
			   int       rfb_port)
{
  g_return_if_fail (VINO_IS_HTTP (http));
  g_return_if_fail (rfb_port > 0);

  g_ptr_array_remove (http->priv->rfb_ports, GUINT_TO_POINTER (rfb_port));

  if (!http->priv->rfb_ports->len)
    {
      g_ptr_array_free (http->priv->rfb_ports, TRUE);
      http->priv->rfb_ports = NULL;
    }
}

int
vino_get_http_server_port ()
{
  return singleton_http->priv->http_port;
}
