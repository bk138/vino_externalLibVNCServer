/*
 * vino-url-webservice.c
 * Copyright (C) Jonh Wendell 2009 <wendell@bani.com.br>
 * 
 * vino-url-webservice.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * vino-url-webservice.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *      Jonh Wendell <wendell@bani.com.br>
 */

#include "vino-url-webservice.h"

#define VINO_WEBSERVICE_FILE "webservices"

gchar *
vino_url_webservice_get_random (void)
{
  gchar       *file_contents, *result;
  gchar      **entries;
  gint32      i;
  GSList      *string_list = NULL;
  GError      *error = NULL;
  const gchar *filename;

  if (g_file_test (VINO_WEBSERVICE_FILE, G_FILE_TEST_EXISTS))
    filename = VINO_WEBSERVICE_FILE;
  else
    filename = VINO_UIDIR G_DIR_SEPARATOR_S VINO_WEBSERVICE_FILE;

  if (!g_file_get_contents (filename,
                            &file_contents,
                            NULL,
                            &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  entries = g_strsplit (file_contents, "\n", 0);
  for (i=0; entries[i]; i++)
    {
      g_strstrip (entries[i]);
      if (entries[i][0] != '\0' && entries[i][0] != '#')
        string_list = g_slist_append (string_list, entries[i]);
    }

  i = g_slist_length (string_list);
  switch (i)
    {
      case 0: result = NULL; break;
      case 1: result = g_strdup (string_list->data); break;
      default: result = g_strdup (g_slist_nth_data (string_list, g_random_int_range (0,  i)));
    }

  g_free (file_contents);
  g_strfreev (entries);
  g_slist_free (string_list);

  return result;
}

