/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2004 Anders Carlsson <andersca@gnome.org>
 *
 *  Authors:
 *    Jonathan Blandford <jrb@alum.mit.edu>
 *    Anders Carlsson <andersca@gnome.org>
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

#include "ev-sidebar-thumbnails.h"
#include "ev-document-thumbnails.h"
#include "ev-utils.h"

#define THUMBNAIL_WIDTH 96
/* Amount of time we devote to each iteration of the idle, in microseconds */
#define IDLE_WORK_LENGTH 5000

struct _EvSidebarThumbnailsPrivate {
	GtkWidget *tree_view;
	GtkListStore *list_store;
	EvDocument *document;
	
	guint idle_id;
	gint current_page, n_pages;
};

enum {
	COLUMN_PAGE_STRING,
	COLUMN_PIXBUF,
	NUM_COLUMNS
};

static GtkVBoxClass *parent_class;

G_DEFINE_TYPE (EvSidebarThumbnails, ev_sidebar_thumbnails, GTK_TYPE_VBOX);

#define EV_SIDEBAR_THUMBNAILS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_THUMBNAILS, EvSidebarThumbnailsPrivate));

static void
ev_sidebar_thumbnails_destroy (GtkObject *object)
{
	EvSidebarThumbnails *ev_sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (object);
	EvSidebarThumbnailsPrivate *priv = ev_sidebar_thumbnails->priv;
	
	if (priv->idle_id != 0) {
		g_source_remove (priv->idle_id);

		priv->idle_id = 0;
	}
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
ev_sidebar_thumbnails_class_init (EvSidebarThumbnailsClass *ev_sidebar_thumbnails_class)
{
	GObjectClass *g_object_class;
	GtkObjectClass *gtk_object_class;
	
	g_object_class = G_OBJECT_CLASS (ev_sidebar_thumbnails_class);
	gtk_object_class = GTK_OBJECT_CLASS (ev_sidebar_thumbnails_class);

 	parent_class = g_type_class_peek_parent (ev_sidebar_thumbnails_class);
	
	gtk_object_class->destroy = ev_sidebar_thumbnails_destroy;
	
	g_type_class_add_private (g_object_class, sizeof (EvSidebarThumbnailsPrivate));

}

static void
ev_sidebar_thumbnails_init (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	GtkWidget *swindow;
	EvSidebarThumbnailsPrivate *priv;

	priv = ev_sidebar_thumbnails->priv = EV_SIDEBAR_THUMBNAILS_GET_PRIVATE (ev_sidebar_thumbnails);

	priv->list_store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, GDK_TYPE_PIXBUF);
	priv->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->list_store));

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view), -1,
						     NULL, gtk_cell_renderer_pixbuf_new (),
						     "pixbuf", 1, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view), -1,
						     NULL, gtk_cell_renderer_text_new (),
						     "text", 0, NULL);
	
	g_object_unref (priv->list_store);

	swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);

	gtk_container_add (GTK_CONTAINER (swindow), priv->tree_view);
	gtk_box_pack_start (GTK_BOX (ev_sidebar_thumbnails), swindow, TRUE, TRUE, 0);
	
	gtk_widget_show_all (swindow);
}

GtkWidget *
ev_sidebar_thumbnails_new (void)
{
	GtkWidget *ev_sidebar_thumbnails;

	ev_sidebar_thumbnails = g_object_new (EV_TYPE_SIDEBAR_THUMBNAILS, NULL);

	return ev_sidebar_thumbnails;
}

static gboolean
do_one_iteration (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = ev_sidebar_thumbnails->priv;
	GdkPixbuf *tmp, *pixbuf;
	GtkTreePath *path;
	GtkTreeIter iter;

	tmp = ev_document_thumbnails_get_thumbnail (EV_DOCUMENT_THUMBNAILS (priv->document),
						       priv->current_page, THUMBNAIL_WIDTH);

#if 1
	/* Don't add the shadow for now, as it's really slow */
	pixbuf = g_object_ref (tmp);
#else
	/* Add shadow */
	pixbuf = ev_pixbuf_add_shadow (tmp, 5, 0, 0, 0.5);
#endif
	path = gtk_tree_path_new_from_indices (priv->current_page, -1);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->list_store), &iter, path);
	gtk_tree_path_free (path);

	gtk_list_store_set (priv->list_store, &iter,
			    COLUMN_PIXBUF, pixbuf,
			    -1);

	g_object_unref (tmp);
	g_object_unref (pixbuf);
	
	priv->current_page++;
	
	if (priv->current_page == priv->n_pages)
		return FALSE;
	else
		return TRUE;
}

static gboolean
populate_thumbnails_idle (gpointer data)
{
	GTimer *timer;
	gint i;
	gulong microseconds = 0;

	EvSidebarThumbnails *ev_sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (data);
	EvSidebarThumbnailsPrivate *priv = ev_sidebar_thumbnails->priv;

	if (priv->current_page == priv->n_pages) {
		priv->idle_id = 0;
		return FALSE;
	}

	timer = g_timer_new ();
	i = 0;
	g_timer_start (timer);
	while (do_one_iteration (ev_sidebar_thumbnails)) {
		i++;
		g_timer_elapsed (timer, &microseconds);
		if (microseconds > IDLE_WORK_LENGTH)
			break;
	}
	g_timer_destroy (timer);
#if 1
	g_print ("%d rows done this idle in %d\n", i, (int)microseconds);
#endif

	return TRUE;
}

void
ev_sidebar_thumbnails_set_document (EvSidebarThumbnails *sidebar_thumbnails,
				    EvDocument          *document)
{
	GtkIconTheme *theme;
	GdkPixbuf *loading_icon;
	gint i, n_pages;
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	
	g_return_if_fail (EV_IS_DOCUMENT_THUMBNAILS (document));

	if (priv->idle_id != 0) {
		g_source_remove (priv->idle_id);
	}
	
	theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (sidebar_thumbnails)));

	loading_icon = gtk_icon_theme_load_icon (theme, "gnome-fs-loading-icon",
						 THUMBNAIL_WIDTH, 0, NULL);

	n_pages = ev_document_get_n_pages (document);
	
	for (i = 0; i < n_pages; i++) {
		GtkTreeIter iter;
		gchar *page;

		page = g_strdup_printf ("Page %d", i + 1);
		gtk_list_store_append (sidebar_thumbnails->priv->list_store,
				       &iter);
		gtk_list_store_set (sidebar_thumbnails->priv->list_store,
				    &iter,
				    COLUMN_PAGE_STRING, page,
				    COLUMN_PIXBUF, loading_icon,
				    -1);
		g_free (page);
	}

	priv->document = document;
	priv->idle_id = g_idle_add (populate_thumbnails_idle, sidebar_thumbnails);
	priv->n_pages = n_pages;
	priv->current_page = 0;
}

