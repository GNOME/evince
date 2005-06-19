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
#include <glib.h>
#include <libgnome/gnome-init.h>
#include <unistd.h>

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

		dot_dir = g_build_filename (g_get_home_dir (),
					    GNOME_DOT_GNOME,
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
