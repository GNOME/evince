/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2004, 2005 Anders Carlsson <andersca@gnome.org>
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
#include "ev-document-misc.h"
#include "ev-window.h"
#include "ev-utils.h"

#define THUMBNAIL_WIDTH 75
/* Amount of time we devote to each iteration of the idle, in microseconds */
#define IDLE_WORK_LENGTH 5000

struct _EvSidebarThumbnailsPrivate {
	GtkWidget *tree_view;
	GtkAdjustment *vadjustment;
	GtkListStore *list_store;
	EvDocument *document;

	guint idle_id;
	gint current_page, n_pages, pages_done;
	GtkTreeIter current_page_iter;
};

enum {
	COLUMN_PAGE_STRING,
	COLUMN_PIXBUF,
	COLUMN_THUMBNAIL_SET,
	NUM_COLUMNS
};

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

	GTK_OBJECT_CLASS (ev_sidebar_thumbnails_parent_class)->destroy (object);
}

static void
ev_sidebar_thumbnails_class_init (EvSidebarThumbnailsClass *ev_sidebar_thumbnails_class)
{
	GObjectClass *g_object_class;
	GtkObjectClass *gtk_object_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_thumbnails_class);
	gtk_object_class = GTK_OBJECT_CLASS (ev_sidebar_thumbnails_class);

	gtk_object_class->destroy = ev_sidebar_thumbnails_destroy;

	g_type_class_add_private (g_object_class, sizeof (EvSidebarThumbnailsPrivate));

}

static void
adjustment_changed_cb (GtkAdjustment       *adjustment,
		       EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv;
	GtkTreePath *path;
	GtkTreeIter iter;
	int page;
	gboolean thumbnail_set;

	priv = ev_sidebar_thumbnails->priv = EV_SIDEBAR_THUMBNAILS_GET_PRIVATE (ev_sidebar_thumbnails);

	gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
				       1, 1, &path,
				       NULL, NULL, NULL);
	if (!path)
		return;

	page = gtk_tree_path_get_indices (path)[0];
	if (page == priv->current_page)
		return;
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->list_store),
				 &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store), &iter,
			    COLUMN_THUMBNAIL_SET, &thumbnail_set,
			    -1);
	if (! thumbnail_set) {
		priv->current_page = page;
		priv->current_page_iter = iter;
		
	}
}

static void
ev_sidebar_tree_selection_changed (GtkTreeSelection *selection,
				   EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv;
	GtkWidget *window;
	GtkTreePath *path;
	GtkTreeIter iter;
	int page;

	priv = ev_sidebar_thumbnails->priv = EV_SIDEBAR_THUMBNAILS_GET_PRIVATE (ev_sidebar_thumbnails);
  
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;
	
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->list_store),
					&iter);

	page = gtk_tree_path_get_indices (path)[0] + 1;

	gtk_tree_path_free (path);

	window = gtk_widget_get_ancestor (GTK_WIDGET (ev_sidebar_thumbnails),
					  EV_TYPE_WINDOW);
	if (window && ev_document_get_page (priv->document) != page) {
		ev_window_open_page (EV_WINDOW (window), page);
	}
}

static void
ev_sidebar_thumbnails_init (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	GtkWidget *swindow;
	EvSidebarThumbnailsPrivate *priv;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;

	priv = ev_sidebar_thumbnails->priv = EV_SIDEBAR_THUMBNAILS_GET_PRIVATE (ev_sidebar_thumbnails);
	
	priv->list_store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN);
	priv->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->list_store));
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (ev_sidebar_tree_selection_changed), ev_sidebar_thumbnails);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
	renderer = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
				 "xpad", 2,
				 "ypad", 2,
				 NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view), -1,
						     NULL, renderer,
						     "pixbuf", 1,
						     NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view), -1,
						     NULL, gtk_cell_renderer_text_new (),
						     "markup", 0, NULL);

	g_object_unref (priv->list_store);

	swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);
	priv->vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (swindow));
	g_signal_connect (G_OBJECT (priv->vadjustment), "value-changed",
			  G_CALLBACK (adjustment_changed_cb),
			  ev_sidebar_thumbnails);
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
	GdkPixbuf *pixbuf;
	gboolean thumbnail_set;

	gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store),
			    &(priv->current_page_iter),
			    COLUMN_THUMBNAIL_SET, &thumbnail_set,
			    -1);
	if (!thumbnail_set) {
		pixbuf = ev_document_thumbnails_get_thumbnail (EV_DOCUMENT_THUMBNAILS (priv->document),
							       priv->current_page, THUMBNAIL_WIDTH);

		gtk_list_store_set (priv->list_store,
				    &(priv->current_page_iter),
				    COLUMN_PIXBUF, pixbuf,
				    -1);

		g_object_unref (pixbuf);
		priv->pages_done ++;
	}

	priv->current_page++;

	if (priv->current_page == priv->n_pages) {
		priv->current_page = 0;
		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->list_store),
					       &(priv->current_page_iter));
	} else {
		gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->list_store),
					  &(priv->current_page_iter));
	}

	if (priv->pages_done == priv->n_pages)
		return FALSE;
	else
		return TRUE;
}

static gboolean
populate_thumbnails_idle (gpointer data)
{
	GTimer *timer;
	int i;
	gdouble time_elapsed = 0;

	EvSidebarThumbnails *ev_sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (data);
	EvSidebarThumbnailsPrivate *priv = ev_sidebar_thumbnails->priv;

#if PROFILE_THUMB == 1
	static GTimer *total_timer;
	static gboolean first_time = TRUE;

	if (first_time) {
		total_timer = g_timer_new ();
		first_time = FALSE;
		g_timer_start (total_timer);
	}
#endif

	/* undo the thumbnailing idle and handler */
	if (priv->pages_done == priv->n_pages) {
		priv->idle_id = 0;
		g_signal_handlers_disconnect_by_func (priv->vadjustment,
						      adjustment_changed_cb,
						      ev_sidebar_thumbnails);
#if PROFILE_THUMB == 1
		time_elapsed = g_timer_elapsed (total_timer, NULL);
		g_timer_destroy (total_timer);
		g_print ("%d rows done in %f seconds\n",
			 gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->list_store), NULL),
			 time_elapsed);
#endif
		return FALSE;
	}

	timer = g_timer_new ();
	i = 0;
	g_timer_start (timer);
	while (do_one_iteration (ev_sidebar_thumbnails)) {
		i++;
		time_elapsed = g_timer_elapsed (timer, NULL);
		if (time_elapsed > IDLE_WORK_LENGTH/1000000)
			break;
	}
	g_timer_destroy (timer);
#if PROFILE_THUMB == 2
	g_print ("%d rows done this idle in %f seconds\n", i, time_elapsed);
#endif

	return TRUE;
}

void
ev_sidebar_thumbnails_select_page (EvSidebarThumbnails *sidebar,
				   int                  page)
{
	GtkTreePath *path;
	GtkTreeSelection *selection;

	/* if the EvSidebar's document can't provide thumbnails */
	if (sidebar->priv->document == NULL) 
		return;

	path = gtk_tree_path_new_from_indices (page - 1, -1);
	selection = gtk_tree_view_get_selection
			(GTK_TREE_VIEW (sidebar->priv->tree_view));

	if (path) {
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (sidebar->priv->tree_view),
					      path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_path_free (path);	
	}
}


void
ev_sidebar_thumbnails_set_document (EvSidebarThumbnails *sidebar_thumbnails,
				    EvDocument          *document)
{
	GdkPixbuf *loading_icon;
	gint i, n_pages;
	GtkTreeIter iter;
	gchar *page;
	gint width = THUMBNAIL_WIDTH;
	gint height = THUMBNAIL_WIDTH;

	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;

	g_return_if_fail (EV_IS_DOCUMENT_THUMBNAILS (document));

	if (priv->idle_id != 0) {
		g_source_remove (priv->idle_id);
	}
	n_pages = ev_document_get_n_pages (document);

	priv->document = document;
	priv->idle_id = g_idle_add (populate_thumbnails_idle, sidebar_thumbnails);
	priv->n_pages = n_pages;

	/* We get the dimensions of the first doc so that we can make a blank
	 * icon.  */
	ev_document_thumbnails_get_dimensions (EV_DOCUMENT_THUMBNAILS (priv->document),
					       1, THUMBNAIL_WIDTH, &width, &height);
	loading_icon = ev_document_misc_get_thumbnail_frame (width, height, NULL);

	for (i = 1; i <= n_pages; i++) {
		page = g_strdup_printf ("<i>%d</i>", i);
		gtk_list_store_append (priv->list_store, &iter);
		gtk_list_store_set (priv->list_store, &iter,
				    COLUMN_PAGE_STRING, page,
				    COLUMN_PIXBUF, loading_icon,
				    COLUMN_THUMBNAIL_SET, FALSE,
				    -1);
		g_free (page);
	}

	g_object_unref (loading_icon);
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->list_store),
				       &(priv->current_page_iter));
	priv->current_page = 0;
	priv->pages_done = 0;
}

