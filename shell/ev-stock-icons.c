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

#include <config.h>

#include "ev-stock-icons.h"

#include <gtk/gtkiconfactory.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkpixbuf.h>

/* Toolbar icons files */
#define STOCK_ZOOM_FIT_WIDTH_FILE "ev-stock-zoom-fit-width.png"

#define EV_ADD_STOCK_ICON(id, file, def_id)				        \
{				  					        \
	GdkPixbuf *pixbuf;						        \
	GtkIconSet *icon_set = NULL;					        \
        pixbuf = gdk_pixbuf_new_from_file (GNOMEICONDIR "/evince/" file, NULL); \
        if (pixbuf) {							        \
        	icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);	        \
	} else if (def_id) {						        \
		icon_set = gtk_icon_factory_lookup_default (def_id);	        \
		gtk_icon_set_ref (icon_set);				        \
	}								        \
        gtk_icon_factory_add (factory, id, icon_set);   		        \
        gtk_icon_set_unref (icon_set);					        \
}

void
ev_stock_icons_init (void)
{
	static const char *icon_theme_items[] =	{
		EV_STOCK_LEAVE_FULLSCREEN
	};
        GtkIconFactory *factory;
	guint i;

        factory = gtk_icon_factory_new ();
        gtk_icon_factory_add_default (factory);

	/* fitwidth stock icon */
	EV_ADD_STOCK_ICON (EV_STOCK_ZOOM_FIT_WIDTH, STOCK_ZOOM_FIT_WIDTH_FILE, GTK_STOCK_ZOOM_FIT);

	for (i = 0; i < G_N_ELEMENTS (icon_theme_items); i++) {
		GtkIconSet *icon_set;
		GtkIconSource *icon_source;

		icon_set = gtk_icon_set_new ();
		icon_source = gtk_icon_source_new ();
		gtk_icon_source_set_icon_name (icon_source, icon_theme_items[i]);
		gtk_icon_set_add_source (icon_set, icon_source);
		gtk_icon_factory_add (factory, icon_theme_items[i], icon_set);
		gtk_icon_set_unref (icon_set);
		gtk_icon_source_free (icon_source);
	}

	g_object_unref (G_OBJECT (factory));
}
