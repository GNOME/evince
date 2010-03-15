/*
 * Copyright (C) 2010, Hib Eris <hib@hiberis.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "texmfcnf.h"

#include <glib.h>
#include <stdlib.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif

gchar *
get_texmfcnf(void)
{
	char *env = getenv("TEXMFCNF");
	if (env)
		return g_strdup(env);

#ifdef G_OS_WIN32
	gchar *texmfcnf = NULL;
	TCHAR path[_MAX_PATH];

	if (SearchPath(NULL, "mktexpk", ".exe", _MAX_PATH, path, NULL))
	{
		gchar *sdir, *sdir_parent, *sdir_grandparent;
		const gchar *texmfcnf_fmt = "{%s,%s,%s}{,{/share,}/texmf{-local,}/web2c}";

		sdir = g_path_get_dirname(path);
		sdir_parent = g_path_get_dirname(sdir);
		sdir_grandparent = g_path_get_dirname(sdir_parent);

		texmfcnf = g_strdup_printf(texmfcnf_fmt,
			sdir, sdir_parent, sdir_grandparent);

		g_free(sdir);
		g_free(sdir_parent);
		g_free(sdir_grandparent);
	}
	return texmfcnf;
#else
	return NULL;
#endif
}




