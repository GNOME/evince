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

#include "ev-sidebar-bookmarks.h"

struct _EvSidebarBookmarksPrivate {
	GtkWidget *tree_view;
};

enum {
	BOOKMARKS_COLUMN_MARKUP,
	BOOKMARKS_COLUMN_OUTLINE,
	BOOKMARKS_COLUMN_PAGE_NUM,
	BOOKMARKS_COLUMN_PAGE_VALID,
	NUM_COLUMNS
};

G_DEFINE_TYPE (EvSidebarBookmarks, ev_sidebar_bookmarks, GTK_TYPE_VBOX)

#define EV_SIDEBAR_BOOKMARKS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarksPrivate))

static void
ev_sidebar_bookmarks_class_init (EvSidebarBookmarksClass *ev_sidebar_bookmarks_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_bookmarks_class);

	g_type_class_add_private (g_object_class, sizeof (EvSidebarBookmarksPrivate));

}


static void
ev_sidebar_bookmarks_construct (EvSidebarBookmarks *ev_sidebar_bookmarks)
{
	EvSidebarBookmarksPrivate *priv;
	GtkWidget *swindow;

	priv = ev_sidebar_bookmarks->priv;
	swindow = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);

	/* Create tree view */
	priv->tree_view = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
	gtk_container_add (GTK_CONTAINER (swindow), priv->tree_view);

	gtk_box_pack_start (GTK_BOX (ev_sidebar_bookmarks), swindow, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (ev_sidebar_bookmarks));
}

static void
ev_sidebar_bookmarks_init (EvSidebarBookmarks *ev_sidebar_bookmarks)
{
	ev_sidebar_bookmarks->priv = EV_SIDEBAR_BOOKMARKS_GET_PRIVATE (ev_sidebar_bookmarks);

	ev_sidebar_bookmarks_construct (ev_sidebar_bookmarks);
}

GtkWidget *
ev_sidebar_bookmarks_new (void)
{
	GtkWidget *ev_sidebar_bookmarks;

	ev_sidebar_bookmarks = g_object_new (EV_TYPE_SIDEBAR_BOOKMARKS, NULL);

	return ev_sidebar_bookmarks;
}
