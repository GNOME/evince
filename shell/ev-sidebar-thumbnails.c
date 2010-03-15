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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

typedef struct _EvThumbsSize
{
	gint width;
	gint height;
} EvThumbsSize;

typedef struct _EvThumbsSizeCache {
	gboolean uniform;
	gint uniform_width;
	gint uniform_height;
	EvThumbsSize *sizes;
} EvThumbsSizeCache;

struct _EvSidebarThumbnailsPrivate {
	GtkWidget *swindow;
	GtkWidget *icon_view;
	GtkWidget *tree_view;
	GtkAdjustment *vadjustment;
	GtkListStore *list_store;
	GHashTable *loading_icons;
	EvDocument *document;
	EvDocumentModel *model;
	EvThumbsSizeCache *size_cache;

	gint n_pages, pages_done;

	int rotation;
	gboolean inverted_colors;

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

/* Thumbnails dimensions cache */
#define EV_THUMBNAILS_SIZE_CACHE_KEY "ev-thumbnails-size-cache"

static EvThumbsSizeCache *
ev_thumbnails_size_cache_new (EvDocument *document)
{
	EvThumbsSizeCache *cache;
	EvRenderContext *rc = NULL;
	gint i, n_pages;
	EvThumbsSize *thumb_size;

	cache = g_new0 (EvThumbsSizeCache, 1);

	n_pages = ev_document_get_n_pages (document);

	/* Assume all pages are the same size until proven otherwise */
	cache->uniform = TRUE;

	for (i = 0; i < n_pages; i++) {
		EvPage *page;
		gdouble page_width, page_height;
		gint    thumb_width = 0;
		gint    thumb_height = 0;

		page = ev_document_get_page (document, i);

		ev_document_get_page_size (document, i, &page_width, &page_height);

		if (!rc) {
			rc = ev_render_context_new (page, 0, (gdouble)THUMBNAIL_WIDTH / page_width);
		} else {
			ev_render_context_set_page (rc, page);
			ev_render_context_set_scale (rc, (gdouble)THUMBNAIL_WIDTH / page_width);
		}

		ev_document_thumbnails_get_dimensions (EV_DOCUMENT_THUMBNAILS (document),
						       rc, &thumb_width, &thumb_height);

		if (i == 0) {
			cache->uniform_width = thumb_width;
			cache->uniform_height = thumb_height;
		} else if (cache->uniform &&
			   (cache->uniform_width != thumb_width ||
			    cache->uniform_height != thumb_height)) {
			/* It's a different thumbnail size.  Backfill the array. */
			int j;

			cache->sizes = g_new0 (EvThumbsSize, n_pages);

			for (j = 0; j < i; j++) {
				thumb_size = &(cache->sizes[j]);
				thumb_size->width = cache->uniform_width;
				thumb_size->height = cache->uniform_height;
			}
			cache->uniform = FALSE;
		}

		if (! cache->uniform) {
			thumb_size = &(cache->sizes[i]);

			thumb_size->width = thumb_width;
			thumb_size->height = thumb_height;
		}

		g_object_unref (page);
	}

	if (rc) {
		g_object_unref (rc);
	}

	return cache;
}

static void
ev_thumbnails_size_cache_get_size (EvThumbsSizeCache *cache,
				   gint               page,
				   gint               rotation,
				   gint              *width,
				   gint              *height)
{
	gint w, h;

	if (cache->uniform) {
		w = cache->uniform_width;
		h = cache->uniform_height;
	} else {
		EvThumbsSize *thumb_size;

		thumb_size = &(cache->sizes[page]);

		w = thumb_size->width;
		h = thumb_size->height;
	}

	if (rotation == 0 || rotation == 180) {
		if (width) *width = w;
		if (height) *height = h;
	} else {
		if (width) *width = h;
		if (height) *height = w;
	}
}

static void
ev_thumbnails_size_cache_free (EvThumbsSizeCache *cache)
{
	if (cache->sizes) {
		g_free (cache->sizes);
		cache->sizes = NULL;
	}

	g_free (cache);
}

static EvThumbsSizeCache *
ev_thumbnails_size_cache_get (EvDocument *document)
{
	EvThumbsSizeCache *cache;

	cache = g_object_get_data (G_OBJECT (document), EV_THUMBNAILS_SIZE_CACHE_KEY);
	if (!cache) {
		cache = ev_thumbnails_size_cache_new (document);
		g_object_set_data_full (G_OBJECT (document),
					EV_THUMBNAILS_SIZE_CACHE_KEY,
					cache,
					(GDestroyNotify)ev_thumbnails_size_cache_free);
	}

	return cache;
}


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

		ev_thumbnails_size_cache_get_size (priv->size_cache, start_page,
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
	gdouble width;

	ev_document_get_page_size (priv->document, page, &width, NULL);

	return (gdouble)THUMBNAIL_WIDTH / width;
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
	gdouble page_size;
	gdouble value;
	gint wy1;
	gint wy2;

	/* Widget is not currently visible */
	if (!gtk_widget_get_mapped (GTK_WIDGET (sidebar_thumbnails)))
		return;

	page_size = gtk_adjustment_get_page_size (priv->vadjustment);

	if (page_size == 0)
		return;

	value = gtk_adjustment_get_value (priv->vadjustment);
	
	if (priv->tree_view) {
		if (! gtk_widget_get_realized (priv->tree_view))
			return;

		gtk_tree_view_convert_tree_to_bin_window_coords (GTK_TREE_VIEW (priv->tree_view),
								 0, (int) value,
								 NULL, &wy1);
		gtk_tree_view_convert_tree_to_bin_window_coords (GTK_TREE_VIEW (priv->tree_view),
								 0, (int) (value + page_size),
								 NULL, &wy2);
		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
					       1, wy1 + 1, &path,
					       NULL, NULL, NULL);
		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
					       1, wy2 -1, &path2,
					       NULL, NULL, NULL);
	} else if (priv->icon_view) {
		if (! gtk_widget_get_realized (priv->icon_view))
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

		page_label = ev_document_get_page_label (priv->document, i);
		page_string = g_markup_printf_escaped ("<i>%s</i>", page_label);
		ev_thumbnails_size_cache_get_size (sidebar_thumbnails->priv->size_cache, i,
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

	ev_document_model_set_page (priv->model, page);
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

	ev_document_model_set_page (priv->model, page);
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

	return (ev_document_get_n_pages (priv->document) <= MAX_ICON_VIEW_PAGE_COUNT);
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
ev_sidebar_thumbnails_set_current_page (EvSidebarThumbnails *sidebar,
					gint                 page)
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
page_changed_cb (EvSidebarThumbnails *sidebar,
		 gint                 old_page,
		 gint                 new_page)
{
	ev_sidebar_thumbnails_set_current_page (sidebar, new_page);
}

static gboolean
refresh (EvSidebarThumbnails *sidebar_thumbnails)
{
	adjustment_changed_cb (sidebar_thumbnails);
	return FALSE;
}

static void
ev_sidebar_thumbnails_reload (EvSidebarThumbnails *sidebar_thumbnails)
{
	EvDocumentModel *model;

	if (sidebar_thumbnails->priv->loading_icons)
		g_hash_table_remove_all (sidebar_thumbnails->priv->loading_icons);

	if (sidebar_thumbnails->priv->document == NULL ||
	    sidebar_thumbnails->priv->n_pages <= 0)
		return;

	model = sidebar_thumbnails->priv->model;

	ev_sidebar_thumbnails_clear_model (sidebar_thumbnails);
	ev_sidebar_thumbnails_fill_model (sidebar_thumbnails);

	/* Trigger a redraw */
	sidebar_thumbnails->priv->start_page = -1;
	sidebar_thumbnails->priv->end_page = -1;
	ev_sidebar_thumbnails_set_current_page (sidebar_thumbnails,
						ev_document_model_get_page (model));
	g_idle_add ((GSourceFunc)refresh, sidebar_thumbnails);
}

static void
ev_sidebar_thumbnails_rotation_changed_cb (EvDocumentModel     *model,
					   GParamSpec          *pspec,
					   EvSidebarThumbnails *sidebar_thumbnails)
{
	gint rotation = ev_document_model_get_rotation (model);

	sidebar_thumbnails->priv->rotation = rotation;
	ev_sidebar_thumbnails_reload (sidebar_thumbnails);
}

static void
ev_sidebar_thumbnails_inverted_colors_changed_cb (EvDocumentModel     *model,
						  GParamSpec          *pspec,
						  EvSidebarThumbnails *sidebar_thumbnails)
{
	gboolean inverted_colors = ev_document_model_get_inverted_colors (model);

	sidebar_thumbnails->priv->inverted_colors = inverted_colors;
	ev_sidebar_thumbnails_reload (sidebar_thumbnails);
}

static void
thumbnail_job_completed_callback (EvJobThumbnail      *job,
				  EvSidebarThumbnails *sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreeIter *iter;

	iter = (GtkTreeIter *) g_object_get_data (G_OBJECT (job), "tree_iter");
	if (priv->inverted_colors)
		ev_document_misc_invert_pixbuf (job->thumbnail);
	gtk_list_store_set (priv->list_store,
			    iter,
			    COLUMN_PIXBUF, job->thumbnail,
			    COLUMN_THUMBNAIL_SET, TRUE,
			    COLUMN_JOB, NULL,
			    -1);
}

static void
ev_sidebar_thumbnails_document_changed_cb (EvDocumentModel     *model,
					   GParamSpec          *pspec,
					   EvSidebarThumbnails *sidebar_thumbnails)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;

	if (!EV_IS_DOCUMENT_THUMBNAILS (document) ||
	    ev_document_get_n_pages (document) <= 0 ||
	    !ev_document_check_dimensions (document)) {
		return;
	}

	priv->size_cache = ev_thumbnails_size_cache_get (document);
	priv->document = document;
	priv->n_pages = ev_document_get_n_pages (document);
	priv->rotation = ev_document_model_get_rotation (model);
	priv->inverted_colors = ev_document_model_get_inverted_colors (model);
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
	g_signal_connect_swapped (priv->model, "page-changed",
				  G_CALLBACK (page_changed_cb),
				  sidebar_thumbnails);
	g_signal_connect (priv->model, "notify::rotation",
			  G_CALLBACK (ev_sidebar_thumbnails_rotation_changed_cb),
			  sidebar_thumbnails);
	g_signal_connect (priv->model, "notify::inverted-colors",
			  G_CALLBACK (ev_sidebar_thumbnails_inverted_colors_changed_cb),
			  sidebar_thumbnails);
	sidebar_thumbnails->priv->start_page = -1;
	sidebar_thumbnails->priv->end_page = -1;
	ev_sidebar_thumbnails_set_current_page (sidebar_thumbnails,
						ev_document_model_get_page (model));
	adjustment_changed_cb (sidebar_thumbnails);
}

static void
ev_sidebar_thumbnails_set_model (EvSidebarPage   *sidebar_page,
				 EvDocumentModel *model)
{
	EvSidebarThumbnails *sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (sidebar_page);
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;

	if (priv->model == model)
		return;

	priv->model = model;
	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_thumbnails_document_changed_cb),
			  sidebar_page);
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
	iface->set_model = ev_sidebar_thumbnails_set_model;
	iface->get_label = ev_sidebar_thumbnails_get_label;
}
