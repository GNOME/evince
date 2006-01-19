/*
 *  Copyright (C) 2002 Jorn Baayen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <libgnome/gnome-init.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-xfer.h>

#include "ev-file-helpers.h"

static char *dot_dir = NULL;
static char *tmp_dir = NULL;
static int  count = 0;

static gboolean
ensure_dir_exists (const char *dir)
{
	if (g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE)
	{
		if (g_file_test (dir, G_FILE_TEST_EXISTS) == TRUE)
		{
			g_warning ("%s exists, please move it out of the way.", dir);
			return FALSE;
		}

		if (mkdir (dir, 488) != 0)
		{
			g_warning ("Failed to create directory %s.", dir);
			return FALSE;
		}
	}

	return TRUE;
}

const char *
ev_dot_dir (void)
{
	if (dot_dir == NULL)
	{
		gboolean exists;

		dot_dir = g_build_filename (gnome_user_dir_get (),
					    "evince",
					    NULL);

		exists = ensure_dir_exists (dot_dir);
		g_assert (exists);
	}

	return dot_dir;
}

void
ev_file_helpers_init (void)
{
}

void
ev_file_helpers_shutdown (void)
{	
	if (tmp_dir != NULL)	
		rmdir (tmp_dir);

	g_free (tmp_dir);
	g_free (dot_dir);

	dot_dir = NULL;
	tmp_dir = NULL;
}

gchar* 
ev_tmp_filename (void)
{
	gchar *basename;
	gchar *filename = NULL;

	if (tmp_dir == NULL) {
		gboolean exists;
		gchar   *dirname;
		
		dirname = g_strdup_printf ("evince-%u", getpid());
		tmp_dir = g_build_filename (g_get_tmp_dir (),
					    dirname,
					    NULL);
		g_free (dirname);

		exists = ensure_dir_exists (tmp_dir);
		g_assert (exists);
	}
	
	
	do {
		if (filename != NULL)
			g_free (filename);
			
		basename = g_strdup_printf ("document-%d", count ++);
		
		filename = g_build_filename (tmp_dir, basename, NULL);
		
		g_free (basename);
	} while (g_file_test (filename, G_FILE_TEST_EXISTS));
			
	return filename;
}

gboolean
ev_xfer_uri_simple (const char *from,
		    const char *to,
		    GError     **error)
{
	GnomeVFSResult result;
	GnomeVFSURI *source_uri;
	GnomeVFSURI *target_uri;
	
	if (!from)
		return FALSE;
	
	source_uri = gnome_vfs_uri_new (from);
	target_uri = gnome_vfs_uri_new (to);

	result = gnome_vfs_xfer_uri (source_uri, target_uri, 
				     GNOME_VFS_XFER_DEFAULT | GNOME_VFS_XFER_FOLLOW_LINKS,
				     GNOME_VFS_XFER_ERROR_MODE_ABORT,
				     GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				     NULL,
				     NULL);
	gnome_vfs_uri_unref (target_uri);
	gnome_vfs_uri_unref (source_uri);
    
	if (result != GNOME_VFS_OK)
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_FAILED,
			     gnome_vfs_result_to_string (result));
	return (result == GNOME_VFS_OK);

}
