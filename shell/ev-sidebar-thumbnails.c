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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-document-misc.h"
#include "ev-document-thumbnails.h"
#include "ev-job-scheduler.h"
#include "ev-sidebar-page.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-utils.h"
#include "ev-window.h"

#define THUMBNAIL_WIDTH 100

/* The IconView doesn't scale nearly as well as the TreeView, so we arbitrarily
 * limit its use */
#define MAX_ICON_VIEW_PAGE_COUNT 1500


struct _EvSidebarThumbnailsPrivate {
	GtkWidget *swindow;
	GtkWidget *icon_view;
	GtkWidget *tree_view;
	GtkAdjustment *vadjustment;
	GtkListStore *list_store;
	GHashTable *loading_icons;
	EvDocument *document;
	EvPageCache *page_cache;

	gint n_pages, pages_done;

	int rotation;

	/* Visible pages */
	gint start_page, end_page;
};

enum {
	COLUMN_PAGE_STRING,
	COLUMN_PIXBUF,
	COLUMN_THUMBNAIL_SET,
	COLUMN_JOB,
	NUM_COLUMNS
};

enum {
	PROP_0,
	PROP_WIDGET,
};

static void         ev_sidebar_thumbnails_clear_model      (EvSidebarThumbnails *sidebar);
static gboolean     ev_sidebar_thumbnails_support_document (EvSidebarPage       *sidebar_page,
							    EvDocument          *document);
static void         ev_sidebar_thumbnails_page_iface_init  (EvSidebarPageIface  *iface);
static void         ev_sidebar_thumbnails_set_document     (EvSidebarPage       *sidebar_page,
							    EvDocument          *document);
static const gchar* ev_sidebar_thumbnails_get_label        (EvSidebarPage       *sidebar_page);
static void         thumbnail_job_completed_callback       (EvJobThumbnail      *job,
							    EvSidebarThumbnails *sidebar_thumbnails);
static void         adjustment_changed_cb                  (EvSidebarThumbnails *sidebar_thumbnails);

G_DEFINE_TYPE_EXTENDED (EvSidebarThumbnails, 
                        ev_sidebar_thumbnails, 
                        GTK_TYPE_VBOX,
                        0, 
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE, 
					       ev_sidebar_thumbnails_page_iface_init))

#define EV_SIDEBAR_THUMBNAILS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_THUMBNAILS, EvSidebarThumbnailsPrivate));


static void
ev_sidebar_thumbnails_dispose (GObject *object)
{
	EvSidebarThumbnails *sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (object);
	
	if (sidebar_thumbnails->priv->loading_icons) {
		g_hash_table_destroy (sidebar_thumbnails->priv->loading_icons);
		sidebar_thumbnails->priv->loading_icons = NULL;
	}
	
	if (sidebar_thumbnails->priv->list_store) {
		ev_sidebar_thumbnails_clear_model (sidebar_thumbnails);
		g_object_unref (sidebar_thumbnails->priv->list_store);
		sidebar_thumbnails->priv->list_store = NULL;
	}
	
	G_OBJECT_CLASS (ev_sidebar_thumbnails_parent_class)->dispose (object);
}

static void
ev_sidebar_thumbnails_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	EvSidebarThumbnails *sidebar = EV_SIDEBAR_THUMBNAILS (object);

	switch (prop_id) {
	case PROP_WIDGET:
		if (sidebar->priv->tree_view)
			g_value_set_object (value, sidebar->priv->tree_view);
		else
			g_value_set_object (value, sidebar->priv->icon_view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_sidebar_thumbnails_map (GtkWidget *widget)
{
	EvSidebarThumbnails *sidebar;

	sidebar = EV_SIDEBAR_THUMBNAILS (widget);

	GTK_WIDGET_CLASS (ev_sidebar_thumbnails_parent_class)->map (widget);
	
	adjustment_changed_cb (sidebar);
}

static void
ev_sidebar_thumbnails_class_init (EvSidebarThumbnailsClass *ev_sidebar_thumbnails_class)
{
	GObjectClass *g_object_class;
	GtkObjectClass *gtk_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_thumbnails_class);
	gtk_object_class = GTK_OBJECT_CLASS (ev_sidebar_thumbnails_class);
	widget_class = GTK_WIDGET_CLASS (ev_sidebar_thumbnails_class);

	g_object_class->dispose = ev_sidebar_thumbnails_dispose;
	g_object_class->get_property = ev_sidebar_thumbnails_get_property;
	widget_class->map = ev_sidebar_thumbnails_map;

	g_object_class_override_property (g_object_class,
					  PROP_WIDGET,
					  "main-widget");

	g_type_class_add_private (g_object_class, sizeof (EvSidebarThumbnailsPrivate));
}

GtkWidget *
ev_sidebar_thumbnails_new (void)
{
	GtkWidget *ev_sidebar_thumbnails;

	ev_sidebar_thumbnails = g_object_new (EV_TYPE_SIDEBAR_THUMBNAILS, NULL);

	return ev_sidebar_thumbnails;
}

static GdkPixbuf *
ev_sidebar_thumbnails_get_loading_icon (EvSidebarThumbnails *sidebar_thumbnails,
					gint                 width,
					gint                 height)
{
	GdkPixbuf *icon;
	gchar     *key;

	key = g_strdup_printf ("%dx%d", width, height);
	icon = g_hash_table_lookup (sidebar_thumbnails->priv->loading_icons, key);
	if (!icon) {
		icon = ev_document_misc_get_thumbnail_frame (width, height, NULL);
		g_hash_table_insert (sidebar_thumbnails->priv->loading_icons,
				     key, icon);
	} else {
		g_free (key);
	}
	
	return icon;
}

static void
clear_range (EvSidebarThumbnails *sidebar_thumbnails,
	     gint                 start_page,
	     gint                 end_page)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean result;
	gint prev_width = -1;
	gint prev_height = -1;

	g_assert (start_page <= end_page);

	path = gtk_tree_path_new_from_indices (start_page, -1);
	for (result = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->list_store), &iter, path);
	     result && start_page <= end_page;
	     result = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->list_store), &iter), start_page ++) {
		EvJobThumbnail *job;
		GdkPixbuf *loading_icon = NULL;
		gint width, height;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store),
				    &iter,
				    COLUMN_JOB, &job,
				    -1);

		if (job) {
			g_signal_handlers_disconnect_by_func (job, thumbnail_job_completed_callback, sidebar_thumbnails);
			ev_job_cancel (EV_JOB (job));
			g_object_unref (job);
		}

		ev_page_cache_get_thumbnail_size (priv->page_cache, start_page,
						  priv->rotation,
						  &width, &height);
		if (!loading_icon || (width != prev_width && height != prev_height)) {
			loading_icon =
				ev_sidebar_thumbnails_get_loading_icon (sidebar_thumbnails,
									width, height);
		}

		prev_width = width;
		prev_height = height;

		gtk_list_store_set (priv->list_store, &iter,
				    COLUMN_JOB, NULL,
				    COLUMN_THUMBNAIL_SET, FALSE,
				    COLUMN_PIXBUF, loading_icon,
				    -1);
	}
	gtk_tree_path_free (path);
}

static gdouble
get_scale_for_page (EvSidebarThumbnails *sidebar_thumbnails,
		    gint                 page)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	gint width, height;

	ev_page_cache_get_size (priv->page_cache,
				page, 0,
				1.0, &width, &height);
	
	return (gdouble)THUMBNAIL_WIDTH / (gdouble)width;
}

static void
add_range (EvSidebarThumbnails *sidebar_thumbnails,
	   gint                 start_page,
	   gint                 end_page)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean result;
	gint page = start_page;

	g_assert (start_page <= end_page);

	path = gtk_tree_path_new_from_indices (start_page, -1);
	for (result = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->list_store), &iter, path);
	     result && page <= end_page;
	     result = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->list_store), &iter), page ++) {
		EvJob *job;
		gboolean thumbnail_set;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store), &iter,
				    COLUMN_JOB, &job,
				    COLUMN_THUMBNAIL_SET, &thumbnail_set,
				    -1);

		if (job == NULL && !thumbnail_set) {
			job = ev_job_thumbnail_new (priv->document,
						    page, priv->rotation,
						    get_scale_for_page (sidebar_thumbnails, page));
			ev_job_scheduler_push_job (EV_JOB (job), EV_JOB_PRIORITY_HIGH);
			
			g_object_set_data_full (G_OBJECT (job), "tree_iter",
						gtk_tree_iter_copy (&iter),
						(GDestroyNotify) gtk_tree_iter_free);
			g_signal_connect (job, "finished",
					  G_CALLBACK (thumbnail_job_completed_callback),
					  sidebar_thumbnails);
			gtk_list_store_set (priv->list_store, &iter,
					    COLUMN_JOB, job,
					    -1);
			
			/* The queue and the list own a ref to the job now */
			g_object_unref (job);
		} else if (job) {
			g_object_unref (job);
		}
	}
	gtk_tree_path_free (path);
}

/* This modifies start */
static void
update_visible_range (EvSidebarThumbnails *sidebar_thumbnails,
		      gint                 start_page,
		      gint                 end_page)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	int old_start_page, old_end_page;

	old_start_page = priv->start_page;
	old_end_page = priv->end_page;

	if (start_page == old_start_page &&
	    end_page == old_end_page)
		return;

	/* Clear the areas we no longer display */
	if (old_start_page >= 0 && old_start_page < start_page)
		clear_range (sidebar_thumbnails, old_start_page, MIN (start_page - 1, old_end_page));
	
	if (old_end_page > 0 && old_end_page > end_page)
		clear_range (sidebar_thumbnails, MAX (end_page + 1, old_start_page), old_end_page);

	add_range (sidebar_thumbnails, start_page, end_page);
	
	priv->start_page = start_page;
	priv->end_page = end_page;
}

static void
adjustment_changed_cb (EvSidebarThumbnails *sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreePath *path = NULL;
	GtkTreePath *path2 = NULL;
	gint wy1;
	gint wy2;

	/* Widget is not currently visible */
	if (!GTK_WIDGET_MAPPED (sidebar_thumbnails))
		return;

	if (priv->vadjustment->page_size == 0)
		return;
	
	if (priv->tree_view) {
		if (! GTK_WIDGET_REALIZED (priv->tree_view))
			return;

		gtk_tree_view_convert_tree_to_bin_window_coords (GTK_TREE_VIEW (priv->tree_view),
								 0, (int) priv->vadjustment->value,
								 NULL, &wy1);
		gtk_tree_view_convert_tree_to_bin_window_coords (GTK_TREE_VIEW (priv->tree_view),
								 0, (int) (priv->vadjustment->value + priv->vadjustment->page_size),
								 NULL, &wy2);
		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
					       1, wy1 + 1, &path,
					       NULL, NULL, NULL);
		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
					       1, wy2 -1, &path2,
					       NULL, NULL, NULL);
	} else if (priv->icon_view) {
		if (! GTK_WIDGET_REALIZED (priv->icon_view))
			return;
		if (! gtk_icon_view_get_visible_range (GTK_ICON_VIEW (priv->icon_view), &path, &path2))
			return;
	} else {
		return;
	}

	if (path && path2) {
		update_visible_range (sidebar_thumbnails,
				      gtk_tree_path_get_indices (path)[0],
				      gtk_tree_path_get_indices (path2)[0]);
	}

	gtk_tree_path_free (path);
	gtk_tree_path_free (path2);
}

static void
ev_sidebar_thumbnails_fill_model (EvSidebarThumbnails *sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreeIter iter;
	int i;
	gint prev_width = -1;
	gint prev_height = -1;

	for (i = 0; i < sidebar_thumbnails->priv->n_pages; i++) {
		gchar     *page_label;
		gchar     *page_string;
		GdkPixbuf *loading_icon = NULL;
		gint       width, height;

		page_label = ev_page_cache_get_page_label (priv->page_cache, i);
		page_string = g_markup_printf_escaped ("<i>%s</i>", page_label);
		ev_page_cache_get_thumbnail_size (sidebar_thumbnails->priv->page_cache, i,
						  sidebar_thumbnails->priv->rotation,
						  &width, &height);
		if (!loading_icon || (width != prev_width && height != prev_height)) {
			loading_icon =
				ev_sidebar_thumbnails_get_loading_icon (sidebar_thumbnails,
									width, height);
		}

		prev_width = width;
		prev_height = height;
		
		gtk_list_store_append (priv->list_store, &iter);
		gtk_list_store_set (priv->list_store, &iter,
				    COLUMN_PAGE_STRING, page_string,
				    COLUMN_PIXBUF, loading_icon,
				    COLUMN_THUMBNAIL_SET, FALSE,
				    -1);
		g_free (page_label);
		g_free (page_string);
	}
}

static gboolean
refresh (EvSidebarThumbnails *sidebar_thumbnails)
{
	adjustment_changed_cb (sidebar_thumbnails);
	return FALSE;
}

void
ev_sidebar_thumbnails_refresh (EvSidebarThumbnails *sidebar_thumbnails,
			       int                  rotation)
{
	sidebar_thumbnails->priv->rotation = rotation;
	if (sidebar_thumbnails->priv->loading_icons)
		g_hash_table_remove_all (sidebar_thumbnails->priv->loading_icons);

	if (sidebar_thumbnails->priv->document == NULL ||
	    sidebar_thumbnails->priv->n_pages <= 0)
		return;

	ev_sidebar_thumbnails_clear_model (sidebar_thumbnails);
	ev_sidebar_thumbnails_fill_model (sidebar_thumbnails);

	/* Trigger a redraw */
	sidebar_thumbnails->priv->start_page = -1;
	sidebar_thumbnails->priv->end_page = -1;
	g_idle_add ((GSourceFunc)refresh, sidebar_thumbnails);
}

static void
ev_sidebar_tree_selection_changed (GtkTreeSelection *selection,
				   EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = ev_sidebar_thumbnails->priv;
	GtkTreePath *path;
	GtkTreeIter iter;
	int page;

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->list_store),
					&iter);
	page = gtk_tree_path_get_indices (path)[0];
	gtk_tree_path_free (path);

	ev_page_cache_set_current_page_history (priv->page_cache, page);
}

static void
ev_sidebar_icon_selection_changed (GtkIconView         *icon_view,
				   EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = ev_sidebar_thumbnails->priv;
	GtkTreePath *path;
	GList *selected;
	int page;

	selected = gtk_icon_view_get_selected_items (icon_view);
	if (selected == NULL)
		return;

	/* We don't handle or expect multiple selection. */
	g_assert (selected->next == NULL);

	path = selected->data;
	page = gtk_tree_path_get_indices (path)[0];

	gtk_tree_path_free (path);
	g_list_free (selected);

	ev_page_cache_set_current_page_history (priv->page_cache, page);
}

static void
ev_sidebar_init_tree_view (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;

	priv = ev_sidebar_thumbnails->priv;
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
	gtk_container_add (GTK_CONTAINER (priv->swindow), priv->tree_view);
	gtk_widget_show (priv->tree_view);
}

static void
ev_sidebar_init_icon_view (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv;

	priv = ev_sidebar_thumbnails->priv;

	priv->icon_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (priv->list_store));
	gtk_icon_view_set_markup_column (GTK_ICON_VIEW (priv->icon_view), 0);
	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (priv->icon_view), 1);
	g_signal_connect (priv->icon_view, "selection-changed",
			  G_CALLBACK (ev_sidebar_icon_selection_changed), ev_sidebar_thumbnails);

	gtk_container_add (GTK_CONTAINER (priv->swindow), priv->icon_view);
	gtk_widget_show (priv->icon_view);
}

static gboolean
ev_sidebar_thumbnails_use_icon_view (EvSidebarThumbnails *sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	if (ev_page_cache_get_n_pages (priv->page_cache) > MAX_ICON_VIEW_PAGE_COUNT)
		return FALSE;
	return TRUE;
}

static void
ev_sidebar_thumbnails_init (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv;

	priv = ev_sidebar_thumbnails->priv = EV_SIDEBAR_THUMBNAILS_GET_PRIVATE (ev_sidebar_thumbnails);

	priv->list_store = gtk_list_store_new (NUM_COLUMNS,
					       G_TYPE_STRING,
					       GDK_TYPE_PIXBUF,
					       G_TYPE_BOOLEAN,
					       EV_TYPE_JOB_THUMBNAIL);

	priv->swindow = gtk_scrolled_window_new (NULL, NULL);
	
	/* We actually don't want GTK_POLICY_AUTOMATIC for horizontal scrollbar here
	 * it's just a workaround for bug #449462
	 */
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->swindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->swindow),
					     GTK_SHADOW_IN);
	priv->vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->swindow));
	g_signal_connect_data (priv->vadjustment, "value-changed",
			       G_CALLBACK (adjustment_changed_cb),
			       ev_sidebar_thumbnails, NULL,
			       G_CONNECT_SWAPPED | G_CONNECT_AFTER);
	g_signal_connect_swapped (priv->swindow, "size-allocate",
				  G_CALLBACK (adjustment_changed_cb),
				  ev_sidebar_thumbnails);
	gtk_box_pack_start (GTK_BOX (ev_sidebar_thumbnails), priv->swindow, TRUE, TRUE, 0);

	/* Put it all together */
	gtk_widget_show_all (priv->swindow);
}

static void
page_changed_cb (EvPageCache         *page_cache,
		 int                  page,
		 EvSidebarThumbnails *sidebar)
{
	GtkTreeView *tree_view;
	GtkTreePath *path;

	path = gtk_tree_path_new_from_indices (page, -1);

	if (sidebar->priv->tree_view) {
		tree_view = GTK_TREE_VIEW (sidebar->priv->tree_view);
		gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
		gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);
	} else if (sidebar->priv->icon_view) {

		g_signal_handlers_block_by_func
			(sidebar->priv->icon_view,
			 G_CALLBACK (ev_sidebar_icon_selection_changed), sidebar);

		gtk_icon_view_select_path (GTK_ICON_VIEW (sidebar->priv->icon_view), path);

		g_signal_handlers_unblock_by_func
			(sidebar->priv->icon_view,
			 G_CALLBACK (ev_sidebar_icon_selection_changed), sidebar);

		gtk_icon_view_set_cursor (GTK_ICON_VIEW (sidebar->priv->icon_view), path, NULL, FALSE);
	}

	gtk_tree_path_free (path);
}

static void
thumbnail_job_completed_callback (EvJobThumbnail      *job,
				  EvSidebarThumbnails *sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreeIter *iter;

	iter = (GtkTreeIter *) g_object_get_data (G_OBJECT (job), "tree_iter");
	gtk_list_store_set (priv->list_store,
			    iter,
			    COLUMN_PIXBUF, job->thumbnail,
			    COLUMN_THUMBNAIL_SET, TRUE,
			    COLUMN_JOB, NULL,
			    -1);
}

static void
ev_sidebar_thumbnails_set_document (EvSidebarPage	*sidebar_page,
				    EvDocument          *document)
{
	EvSidebarThumbnails *sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (sidebar_page);

	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;

	priv->page_cache = ev_page_cache_get (document);

	if (!EV_IS_DOCUMENT_THUMBNAILS (document) ||
	    ev_page_cache_get_n_pages (priv->page_cache) <= 0 ||
	    ev_page_cache_check_dimensions (priv->page_cache)) {
		return;
	}

	priv->document = document;
	priv->n_pages = ev_page_cache_get_n_pages (priv->page_cache);
	priv->loading_icons = g_hash_table_new_full (g_str_hash,
						     g_str_equal,
						     (GDestroyNotify)g_free,
						     (GDestroyNotify)g_object_unref);

	ev_sidebar_thumbnails_clear_model (sidebar_thumbnails);
	ev_sidebar_thumbnails_fill_model (sidebar_thumbnails);

	/* Create the view widget, and remove the old one, if needed */
	if (ev_sidebar_thumbnails_use_icon_view (sidebar_thumbnails)) {
		if (priv->tree_view) {
			gtk_container_remove (GTK_CONTAINER (priv->swindow), priv->tree_view);
			priv->tree_view = NULL;
		}

		if (! priv->icon_view) {
			ev_sidebar_init_icon_view (sidebar_thumbnails);
			g_object_notify (G_OBJECT (sidebar_thumbnails), "main_widget");
		} else {
			gtk_widget_queue_resize (priv->icon_view);
		}
	} else {
		if (priv->icon_view) {
			gtk_container_remove (GTK_CONTAINER (priv->swindow), priv->icon_view);
			priv->icon_view = NULL;
		}

		if (! priv->tree_view) {
			ev_sidebar_init_tree_view (sidebar_thumbnails);
			g_object_notify (G_OBJECT (sidebar_thumbnails), "main_widget");
		}
	}

	/* Connect to the signal and trigger a fake callback */
	g_signal_connect (priv->page_cache, "page-changed", G_CALLBACK (page_changed_cb), sidebar_thumbnails);
	sidebar_thumbnails->priv->start_page = -1;
	sidebar_thumbnails->priv->end_page = -1;
	page_changed_cb (priv->page_cache,
			 ev_page_cache_get_current_page (priv->page_cache),
			 sidebar_thumbnails);
	adjustment_changed_cb (sidebar_thumbnails);
}

static gboolean
ev_sidebar_thumbnails_clear_job (GtkTreeModel *model,                                             
			         GtkTreePath *path,
			         GtkTreeIter *iter,
				 gpointer data)
{
	EvJob *job;
	
	gtk_tree_model_get (model, iter, COLUMN_JOB, &job, -1);
	
	if (job != NULL) {
		ev_job_cancel (job);
		g_signal_handlers_disconnect_by_func (job, thumbnail_job_completed_callback, data);
		g_object_unref (job);
	}
	
	return FALSE;    
}

static void 
ev_sidebar_thumbnails_clear_model (EvSidebarThumbnails *sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	
	gtk_tree_model_foreach (GTK_TREE_MODEL (priv->list_store), ev_sidebar_thumbnails_clear_job, sidebar_thumbnails);
	gtk_list_store_clear (priv->list_store);
}

static gboolean
ev_sidebar_thumbnails_support_document (EvSidebarPage   *sidebar_page,
				        EvDocument *document)
{
	return (EV_IS_DOCUMENT_THUMBNAILS (document));
}

static const gchar*
ev_sidebar_thumbnails_get_label (EvSidebarPage *sidebar_page)
{
	return _("Thumbnails");
}

static void
ev_sidebar_thumbnails_page_iface_init (EvSidebarPageIface *iface)
{
	iface->support_document = ev_sidebar_thumbnails_support_document;
	iface->set_document = ev_sidebar_thumbnails_set_document;
	iface->get_label = ev_sidebar_thumbnails_get_label;
}

