/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Stock icons for Evince
 *
 * Copyright (C) 2003 Martin Kretzschmar
 *
 * Author:
 *   Martin Kretzschmar <Martin.Kretzschmar@inf.tu-dresden.de>
 *
 * GPdf is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPdf is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ev-stock-icons.h"

#include <gtk/gtkiconfactory.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkpixbuf.h>

typedef struct {
	char *stock_id;
	char *icon;
} EvStockIcon;

/* Evince stock icons */
static const EvStockIcon stock_icons [] = {
	{ EV_STOCK_ZOOM_PAGE,        "zoom-fit-page" },
	{ EV_STOCK_ZOOM_WIDTH,       "zoom-fit-width" },
	{ EV_STOCK_VIEW_DUAL,        "view-page-facing" },
	{ EV_STOCK_VIEW_CONTINUOUS,  "view-page-continuous" },
	{ EV_STOCK_ROTATE_LEFT,      "object-rotate-left"},
	{ EV_STOCK_ROTATE_RIGHT,     "object-rotate-right"},
	{ EV_STOCK_RUN_PRESENTATION, "x-office-presentation"},
};

/**
 * ev_stock_icons_init:
 *
 * Creates a new icon factory, adding the base stock icons to it.
 */
void
ev_stock_icons_init (void)
{
	GtkIconFactory *factory;
	GtkIconSource *source;
	gint i;

        factory = gtk_icon_factory_new ();
        gtk_icon_factory_add_default (factory);

	source = gtk_icon_source_new ();

	for (i = 0; i < G_N_ELEMENTS (stock_icons); i++) {
		GtkIconSet *set;

		gtk_icon_source_set_icon_name (source, stock_icons [i].icon);

		set = gtk_icon_set_new ();
		gtk_icon_set_add_source (set, source);

		gtk_icon_factory_add (factory, stock_icons [i].stock_id, set);
		gtk_icon_set_unref (set);
	}

	gtk_icon_source_free (source);

	g_object_unref (G_OBJECT (factory));
}
