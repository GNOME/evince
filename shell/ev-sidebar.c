/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Author:
 *    Jonathan Blandford <jrb@alum.mit.edu>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "ev-sidebar.h"

struct _EvSidebarPrivate {
	GtkWidget *option_menu;
	GtkWidget *notebook;
};

G_DEFINE_TYPE (EvSidebar, ev_sidebar, GTK_TYPE_VBOX)

#define EV_SIDEBAR_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR, EvSidebarPrivate))

static void
ev_sidebar_class_init (EvSidebarClass *ev_sidebar_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_class);

	g_type_class_add_private (g_object_class, sizeof (EvSidebarPrivate));

}

static void
ev_sidebar_init (EvSidebar *ev_sidebar)
{
	ev_sidebar->priv = EV_SIDEBAR_GET_PRIVATE (ev_sidebar);

	ev_sidebar->priv->notebook = gtk_notebook_new ();

	gtk_notebook_set_show_border (GTK_NOTEBOOK (ev_sidebar->priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (ev_sidebar->priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (ev_sidebar), ev_sidebar->priv->notebook,
			    TRUE, TRUE, 0);
	gtk_widget_show_all (ev_sidebar->priv->notebook);
}


GtkWidget *
ev_sidebar_new (void)
{
	GtkWidget *ev_sidebar;

	ev_sidebar = g_object_new (EV_TYPE_SIDEBAR, NULL);

	return ev_sidebar;
}
