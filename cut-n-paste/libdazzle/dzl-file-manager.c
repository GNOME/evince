/* dzl-file-manager.c
 *
 * Copyright (C) 1995-2017 GIMP Authors
 * Copyright (C) 2015-2017 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2020      Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "dzl-file-manager"

#include "config.h"

#include <glib/gi18n.h>

#if defined(G_OS_WIN32)
/* This is a hack for Windows known directory support.
 * DATADIR (autotools-generated constant) is a type defined in objidl.h
 * so we must #undef it before including shlobj.h in order to avoid a
 * name clash. */
#undef DATADIR
#include <windows.h>
#include <shlobj.h>
#endif

#ifdef PLATFORM_OSX
#include <AppKit/AppKit.h>
#endif

#include "dzl-file-manager.h"

#if !(defined(G_OS_WIN32) || defined(PLATFORM_OSX))
static void
show_items_cb (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GDBusProxy *proxy = (GDBusProxy *)source_object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  if (!(reply = g_dbus_proxy_call_finish (proxy, result, &error)))
    g_warning ("Failed to show items: %s", error->message);
}
#endif /* !(defined(G_OS_WIN32) || defined(PLATFORM_OSX)) */

/* Copied from the GIMP */
gboolean
dzl_file_manager_show (GFile   *file,
                       GError **error)
{
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

#if defined(G_OS_WIN32)

  {
    gboolean ret;
    char *filename;
    int n;
    LPWSTR w_filename = NULL;
    ITEMIDLIST *pidl = NULL;

    ret = FALSE;

    /* Calling this function mutiple times should do no harm, but it is
       easier to put this here as it needs linking against ole32. */
    CoInitialize (NULL);

    filename = g_file_get_path (file);
    if (!filename)
      {
        g_set_error_literal (error, G_FILE_ERROR, 0,
                             _("File path is NULL"));
        goto out;
      }

    n = MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS,
                             filename, -1, NULL, 0);
    if (n == 0)
      {
        g_set_error_literal (error, G_FILE_ERROR, 0,
                             _("Error converting UTF-8 filename to wide char"));
        goto out;
      }

    w_filename = g_malloc_n (n + 1, sizeof (wchar_t));
    n = MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS,
                             filename, -1,
                             w_filename, (n + 1) * sizeof (wchar_t));
    if (n == 0)
      {
        g_set_error_literal (error, G_FILE_ERROR, 0,
                             _("Error converting UTF-8 filename to wide char"));
        goto out;
      }

    pidl = ILCreateFromPathW (w_filename);
    if (!pidl)
      {
        g_set_error_literal (error, G_FILE_ERROR, 0,
                             _("ILCreateFromPath() failed"));
        goto out;
      }

    SHOpenFolderAndSelectItems (pidl, 0, NULL, 0);
    ret = TRUE;

  out:
    if (pidl)
      ILFree (pidl);
    g_free (w_filename);
    g_free (filename);

    return ret;
  }

#elif defined(PLATFORM_OSX)

  {
    gchar    *uri;
    NSString *filename;
    NSURL    *url;
    gboolean  retval = TRUE;

    uri = g_file_get_uri (file);
    filename = [NSString stringWithUTF8String:uri];

    url = [NSURL URLWithString:filename];
    if (url)
      {
        NSArray *url_array = [NSArray arrayWithObject:url];

        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:url_array];
      }
    else
      {
        g_set_error (error, G_FILE_ERROR, 0,
                     _("Cannot convert “%s” into a valid NSURL."), uri);
        retval = FALSE;
      }

    g_free (uri);

    return retval;
  }

#else /* UNIX */

  {
    g_autofree gchar *uri = g_file_get_uri (file);
    g_autoptr(GVariantBuilder) builder = NULL;
    g_autoptr(GDBusProxy) proxy = NULL;

    proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                           (G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                            G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION),
                                           NULL,
                                           "org.freedesktop.FileManager1",
                                           "/org/freedesktop/FileManager1",
                                           "org.freedesktop.FileManager1",
                                           NULL,
                                           error);

    /* Implausible */
    if (proxy == NULL)
      return FALSE;

    builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    g_variant_builder_add (builder, "s", uri);
    g_dbus_proxy_call (proxy,
                       "ShowItems",
                       g_variant_new ("(ass)", builder, ""),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       show_items_cb,
                       NULL);

    return TRUE;
  }

#endif
}
