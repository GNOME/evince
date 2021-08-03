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

typedef struct {
	char *stock_id;
	char *icon;
} EvStockIcon;

/* Evince stock icons */
static const EvStockIcon stock_icons [] = {
	{ EV_STOCK_ANNOT_TEXT,       "annotation-text-symbolic" },
	{ EV_STOCK_ANNOT_SQUIGGLY,   "annotation-squiggly-symbolic" },
	{ EV_STOCK_FIND_UNSUPPORTED, "find-unsupported-symbolic" },
	{ EV_STOCK_ZOOM,	     "zoom" },
	{ EV_STOCK_ZOOM_PAGE,        "zoom-fit-height" },
	{ EV_STOCK_ZOOM_WIDTH,       "zoom-fit-width" },
	{ EV_STOCK_VIEW_DUAL,        "view-page-facing" },
	{ EV_STOCK_VIEW_CONTINUOUS,  "view-page-continuous" },
	{ EV_STOCK_ROTATE_LEFT,      "object-rotate-left"},
	{ EV_STOCK_ROTATE_RIGHT,     "object-rotate-right"},
	{ EV_STOCK_RUN_PRESENTATION, "x-office-presentation"},
	{ EV_STOCK_VISIBLE,          "visible-symbolic"},
	{ EV_STOCK_RESIZE_SE,        "resize-se"},
	{ EV_STOCK_RESIZE_SW,        "resize-sw"},
	{ EV_STOCK_CLOSE,            "close"},
	{ EV_STOCK_INVERTED_COLORS,  "stock_filters-invert"},
	{ EV_STOCK_ATTACHMENT,       "mail-attachment"},
	{ EV_STOCK_SEND_TO,          "document-send"},
	{ EV_STOCK_VIEW_SIDEBAR,     "view-sidebar-symbolic"},
	{ EV_STOCK_OUTLINE,          "outline-symbolic"},
};

static gchar *ev_icons_path;

static void
ev_stock_icons_add_icons_path_for_display (GdkDisplay *display)
{
	GtkIconTheme *icon_theme;

	g_return_if_fail (ev_icons_path != NULL);

	if (!display)
		display = gdk_display_get_default ();

	icon_theme = gtk_icon_theme_get_for_display (display);
	if (icon_theme) {
		gchar **path = NULL, **tp;
		gboolean found = FALSE;

		/* GtkIconTheme will then look in Evince custom hicolor dir
		 * for icons as well as the standard search paths
		 */
		path = gtk_icon_theme_get_search_path (icon_theme);
		for (tp = path; tp && *tp; tp++) {
			if (g_ascii_strcasecmp (ev_icons_path, *tp) == 0) {
				found = TRUE;
				break;
			}
		}

		if (!found)
			gtk_icon_theme_add_search_path (icon_theme,
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
#endif

	ev_stock_icons_add_icons_path_for_display (gdk_display_get_default ());
}

void
ev_stock_icons_set_display (GdkDisplay *display)
{
	g_return_if_fail (GDK_IS_DISPLAY (display));

	ev_stock_icons_add_icons_path_for_display (display);
}

void
ev_stock_icons_shutdown (void)
{
	g_free (ev_icons_path);
}
