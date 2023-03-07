/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Stock icons for Evince
 *
 * Copyright (C) 2003 Martin Kretzschmar <Martin.Kretzschmar@inf.tu-dresden.de>
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <gtk/gtk.h>

#include "ev-stock-icons.h"

static gchar *ev_icons_path;

static void
ev_stock_icons_add_icons_path_for_screen (GdkScreen *screen)
{
	GtkIconTheme *icon_theme;

	g_return_if_fail (ev_icons_path != NULL);

	icon_theme = screen ? gtk_icon_theme_get_for_screen (screen) : gtk_icon_theme_get_default ();
	if (icon_theme) {
		gchar **path = NULL;
		gint    n_paths;
		gint    i;

		/* GtkIconTheme will then look in Evince custom hicolor dir
		 * for icons as well as the standard search paths
		 */
		gtk_icon_theme_get_search_path (icon_theme, &path, &n_paths);
		for (i = n_paths - 1; i >= 0; i--) {
			if (g_ascii_strcasecmp (ev_icons_path, path[i]) == 0)
				break;
		}

		if (i < 0)
			gtk_icon_theme_append_search_path (icon_theme,
							   ev_icons_path);

		g_strfreev (path);
	}
}

/**
 * ev_stock_icons_init:
 *
 * Creates a new icon factory, adding the base stock icons to it.
 */
void
ev_stock_icons_init (void)
{
#ifdef G_OS_WIN32
	gchar *dir;

	dir = g_win32_get_package_installation_directory_of_module (NULL);
	ev_icons_path = g_build_filename (dir, "share", "evince", "icons", NULL);
	g_free (dir);
#else
	ev_icons_path = g_build_filename (EVINCEDATADIR, "icons", NULL);
	if (g_getenv ("EV_ICONS_DIR") != NULL)
		ev_icons_path = g_build_filename (g_getenv ("EV_ICONS_DIR"), NULL);
#endif

	ev_stock_icons_add_icons_path_for_screen (gdk_screen_get_default ());
}

void
ev_stock_icons_set_screen (GdkScreen *screen)
{
	g_return_if_fail (GDK_IS_SCREEN (screen));

	ev_stock_icons_add_icons_path_for_screen (screen);
}

void
ev_stock_icons_shutdown (void)
{
	g_free (ev_icons_path);
}
