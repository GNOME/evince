/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "ev-backend-info.h"

#define EV_BACKENDS_GROUP     "Evince Backend"
#define EV_BACKENDS_EXTENSION ".evince-backend"

/*
 * _ev_backend_info_free:
 * @info:
 *
 * Increases refcount of @info by 1.
 */
EvBackendInfo *
_ev_backend_info_ref (EvBackendInfo *info)
{
        g_return_val_if_fail (info != NULL, NULL);
        g_return_val_if_fail (info->ref_count >= 1, NULL);

        g_atomic_int_inc (&info->ref_count);
        return info;
}

/*
 * _ev_backend_info_free:
 * @info:
 *
 * Decreases refcount of @info by 1, and frees @info if the refcount reaches 0.
 */
void
_ev_backend_info_unref (EvBackendInfo *info)
{
        if (info == NULL)
                return;

        g_return_if_fail (info->ref_count >= 1);

        if (!g_atomic_int_dec_and_test (&info->ref_count))
                return;

	g_free (info->module_name);
	g_free (info->type_desc);
	g_strfreev (info->mime_types);
	g_slice_free (EvBackendInfo, info);
}

/**
 * _ev_backend_info_new_from_file:
 * @path: path to the backends file
 * @error: a location to store a #GError, or %NULL
 *
 * Loads backend information from @path.
 *
 * Returns: a new #EvBackendInfo, or %NULL on error with @error filled in
 */
static EvBackendInfo *
_ev_backend_info_new_from_file (const char *file,
                                GError **error)
{
	EvBackendInfo *info = NULL;
	GKeyFile      *backend_file = NULL;

	backend_file = g_key_file_new ();
	if (!g_key_file_load_from_file (backend_file, file, G_KEY_FILE_NONE, error))
                goto err;

	info = g_slice_new0 (EvBackendInfo);
        info->ref_count = 1;

	info->module_name = g_key_file_get_string (backend_file, EV_BACKENDS_GROUP,
						   "Module", error);
	if (!info->module_name)
                goto err;

	info->resident = g_key_file_get_boolean (backend_file, EV_BACKENDS_GROUP,
						 "Resident", NULL);

	info->type_desc = g_key_file_get_locale_string (backend_file, EV_BACKENDS_GROUP,
							"TypeDescription", NULL, error);
	if (!info->type_desc)
                goto err;

	info->mime_types = g_key_file_get_string_list (backend_file, EV_BACKENDS_GROUP,
						       "MimeType", NULL, error);
	if (!info->mime_types)
                goto err;

	g_key_file_free (backend_file);

	return info;

    err:
        g_key_file_free (backend_file);
        _ev_backend_info_unref (info);
        return NULL;
}

/*
 * _ev_backend_info_load_from_dir:
 * @path: a directory name
 *
 * Load all backend infos from @path.
 *
 * Returns: a newly allocated #GList containing newly allocated
 *   #EvBackendInfo objects
 */
GList
*_ev_backend_info_load_from_dir (const char *path)
{
        GList       *list = NULL;
        GDir        *dir;
        const gchar *dirent;
        GError      *error = NULL;

        dir = g_dir_open (path, 0, &error);
        if (!dir) {
                g_warning ("%s", error->message);
                g_error_free (error);

                return FALSE;
        }

        while ((dirent = g_dir_read_name (dir))) {
                EvBackendInfo *info;
                gchar         *file;

                if (!g_str_has_suffix (dirent, EV_BACKENDS_EXTENSION))
                        continue;

                file = g_build_filename (path, dirent, NULL);
                info = _ev_backend_info_new_from_file (file, &error);
                if (error != NULL) {
                        g_warning ("Failed to load backend info from '%s': %s\n",
                                   file, error->message);
                        g_clear_error (&error);
                }
                g_free (file);

                if (info == NULL)
                        continue;

                list = g_list_prepend (list, info);
        }

        g_dir_close (dir);

        return list;
}
