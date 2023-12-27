/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
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

#include <cairo-gobject.h>

#include "ev-document-misc.h"
#include "ev-job-scheduler.h"
#include "ev-sidebar.h"
#include "ev-sidebar-page.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-utils.h"
#include "ev-window.h"

#define THUMBNAIL_WIDTH 100

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
	GtkAdjustment *vadjustment;
	GtkListStore *list_store;
	GHashTable *loading_icons;
	EvDocument *document;
	EvDocumentModel *model;
	EvThumbsSizeCache *size_cache;
        gint width;

	gint n_pages, pages_done;

	int rotation;
	gboolean inverted_colors;
	gboolean blank_first_dual_mode; /* flag for when we're using a blank first thumbnail
					 * for dual mode with !odd_left preference. Issue #30 */
	/* Visible pages */
	gint start_page, end_page;
};

enum {
	COLUMN_PAGE_STRING,
	COLUMN_SURFACE,
	COLUMN_THUMBNAIL_SET,
	COLUMN_JOB,
	NUM_COLUMNS
};

enum {
	PROP_0,
	PROP_WIDGET,
	PROP_DOCUMENT_MODEL,
};

static void         ev_sidebar_thumbnails_clear_model      (EvSidebarThumbnails     *sidebar);
static gboolean     ev_sidebar_thumbnails_support_document (EvSidebarPage           *sidebar_page,
							    EvDocument              *document);
static void         ev_sidebar_thumbnails_set_model	   (EvSidebarPage   *sidebar_page,
							    EvDocumentModel *model);
static void         ev_sidebar_thumbnails_page_iface_init  (EvSidebarPageInterface  *iface);
static void         ev_sidebar_thumbnails_set_current_page (EvSidebarThumbnails *sidebar,
							    gint     page);
static void         thumbnail_job_completed_callback       (EvJobThumbnailCairo     *job,
							    EvSidebarThumbnails     *sidebar_thumbnails);
static void         ev_sidebar_thumbnails_reload           (EvSidebarThumbnails     *sidebar_thumbnails);
static void         adjustment_changed_cb                  (EvSidebarThumbnails     *sidebar_thumbnails);
static void         check_toggle_blank_first_dual_mode     (EvSidebarThumbnails     *sidebar_thumbnails);

G_DEFINE_TYPE_EXTENDED (EvSidebarThumbnails,
                        ev_sidebar_thumbnails,
                        GTK_TYPE_BOX,
                        0,
                        G_ADD_PRIVATE (EvSidebarThumbnails)
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
					       ev_sidebar_thumbnails_page_iface_init))

/* Thumbnails dimensions cache */
#define EV_THUMBNAILS_SIZE_CACHE_KEY "ev-thumbnails-size-cache"

static void
get_thumbnail_size_for_page (EvDocument *document,
			     guint       page,
			     gint       *width,
			     gint       *height)
{
	gdouble scale;
	gdouble w, h;

	ev_document_get_page_size (document, page, &w, &h);
	scale = (gdouble)THUMBNAIL_WIDTH / w;

	*width = MAX ((gint)(w * scale + 0.5), 1);
	*height = MAX ((gint)(h * scale + 0.5), 1);
}

static EvThumbsSizeCache *
ev_thumbnails_size_cache_new (EvDocument *document)
{
	EvThumbsSizeCache *cache;
	gint               i, n_pages;
	EvThumbsSize      *thumb_size;

	cache = g_new0 (EvThumbsSizeCache, 1);

	if (ev_document_is_page_size_uniform (document)) {
		cache->uniform = TRUE;
		get_thumbnail_size_for_page (document, 0,
					     &cache->uniform_width,
					     &cache->uniform_height);
		return cache;
	}

	n_pages = ev_document_get_n_pages (document);

	/* Check for potential integer overflow in allocation - Issue #2094 */
	if ((gsize)n_pages > G_MAXSIZE / sizeof(EvThumbsSize))
		g_error ("Exiting program due to abnormal page count detected: %d", n_pages);

	cache->sizes = g_new0 (EvThumbsSize, n_pages);

	for (i = 0; i < n_pages; i++) {
		thumb_size = &(cache->sizes[i]);
		get_thumbnail_size_for_page (document, i,
					     &thumb_size->width,
					     &thumb_size->height);
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
	g_clear_pointer (&cache->sizes, g_free);
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

static gboolean
ev_sidebar_thumbnails_page_is_in_visible_range (EvSidebarThumbnails *sidebar,
                                                guint                page)
{
        GtkTreePath *path;
        GtkTreePath *start, *end;
        gboolean     retval;
        GList *selection;

	if (!sidebar->priv->icon_view)
		return FALSE;

        selection = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (sidebar->priv->icon_view));
        if (!selection)
                return FALSE;

        path = (GtkTreePath *)selection->data;

        /* We don't handle or expect multiple selection. */
        g_assert (selection->next == NULL);
        g_list_free (selection);

        if (!gtk_icon_view_get_visible_range (GTK_ICON_VIEW (sidebar->priv->icon_view), &start, &end)) {
                gtk_tree_path_free (path);
                return FALSE;
        }

        retval = gtk_tree_path_compare (path, start) >= 0 && gtk_tree_path_compare (path, end) <= 0;
        gtk_tree_path_free (path);
        gtk_tree_path_free (start);
        gtk_tree_path_free (end);

        return retval;
}

static void
ev_sidebar_thumbnails_dispose (GObject *object)
{
	EvSidebarThumbnails *sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (object);

	g_clear_pointer (&sidebar_thumbnails->priv->loading_icons,
			 g_hash_table_destroy);

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
		g_value_set_object (value, sidebar->priv->icon_view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_sidebar_thumbnails_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	EvSidebarThumbnails *sidebar_thumbnails = EV_SIDEBAR_THUMBNAILS (object);

	switch (prop_id)
	{
	case PROP_DOCUMENT_MODEL:
		ev_sidebar_thumbnails_set_model (EV_SIDEBAR_PAGE (sidebar_thumbnails),
			EV_DOCUMENT_MODEL (g_value_get_object (value)));
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
ev_sidebar_check_reset_current_page (EvSidebarThumbnails *sidebar)
{
	guint page;

	if (!sidebar->priv->model)
		return;

	page = ev_document_model_get_page (sidebar->priv->model);
	if (!ev_sidebar_thumbnails_page_is_in_visible_range (sidebar, page))
		ev_sidebar_thumbnails_set_current_page (sidebar, page);
}

static void
ev_sidebar_thumbnails_size_allocate (GtkWidget	*widget,
				     int	 width,
				     int	 height,
				     int	 baseline)
{
        EvSidebarThumbnails *sidebar = EV_SIDEBAR_THUMBNAILS (widget);

        GTK_WIDGET_CLASS (ev_sidebar_thumbnails_parent_class)->size_allocate (widget, width, height, baseline);

        if (width != sidebar->priv->width) {
                sidebar->priv->width = width;

                /* Might have a new number of columns, reset current page */
                ev_sidebar_check_reset_current_page (sidebar);
        }
}

GtkWidget *
ev_sidebar_thumbnails_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_THUMBNAILS, NULL));
}

static cairo_surface_t *
ev_sidebar_thumbnails_get_loading_icon (EvSidebarThumbnails *sidebar_thumbnails,
					gint                 width,
					gint                 height)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
        cairo_surface_t *icon;
	gchar           *key;

	key = g_strdup_printf ("%dx%d", width, height);
	icon = g_hash_table_lookup (priv->loading_icons, key);
	if (!icon) {
		gboolean inverted_colors;
                gint device_scale = 1;

                device_scale = gtk_widget_get_scale_factor (GTK_WIDGET (sidebar_thumbnails));

		inverted_colors = ev_document_model_get_inverted_colors (priv->model);
                icon = ev_document_misc_render_loading_thumbnail_surface (GTK_WIDGET (sidebar_thumbnails),
                                                                          width * device_scale,
                                                                          height * device_scale,
                                                                          inverted_colors);
		g_hash_table_insert (priv->loading_icons, key, icon);
	} else {
		g_free (key);
	}

	return icon;
}

static void
cancel_running_jobs (EvSidebarThumbnails *sidebar_thumbnails,
		     gint                 start_page,
		     gint                 end_page)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean result;

	g_assert (start_page <= end_page);

	path = gtk_tree_path_new_from_indices (start_page, -1);
	for (result = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->list_store), &iter, path);
	     result && start_page <= end_page;
	     result = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->list_store), &iter), start_page ++) {
		EvJobThumbnailCairo *job;
		gboolean thumbnail_set;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store),
				    &iter,
				    COLUMN_JOB, &job,
				    COLUMN_THUMBNAIL_SET, &thumbnail_set,
				    -1);

		if (thumbnail_set) {
			g_assert (job == NULL);
			continue;
		}

		if (job) {
			g_signal_handlers_disconnect_by_func (job, thumbnail_job_completed_callback, sidebar_thumbnails);
			ev_job_cancel (EV_JOB (job));
			g_object_unref (job);
		}

		gtk_list_store_set (priv->list_store, &iter,
				    COLUMN_JOB, NULL,
				    COLUMN_THUMBNAIL_SET, FALSE,
				    -1);
	}
	gtk_tree_path_free (path);
}

static void
get_size_for_page (EvSidebarThumbnails *sidebar_thumbnails,
                   gint                 page,
                   gint                *width_return,
                   gint                *height_return)
{
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
        gdouble width, height;
        gint thumbnail_height, device_scale;

        device_scale = gtk_widget_get_scale_factor (GTK_WIDGET (sidebar_thumbnails));
        ev_document_get_page_size (priv->document, page, &width, &height);
        thumbnail_height = (int)(THUMBNAIL_WIDTH * height / width + 0.5);

        if (priv->rotation == 90 || priv->rotation == 270) {
                *width_return = thumbnail_height * device_scale;
                *height_return = THUMBNAIL_WIDTH * device_scale;
        } else {
                *width_return = THUMBNAIL_WIDTH * device_scale;
                *height_return = thumbnail_height * device_scale;
        }
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

	if (priv->blank_first_dual_mode)
		page--;

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
			gint thumbnail_width, thumbnail_height;
			get_size_for_page (sidebar_thumbnails, page, &thumbnail_width, &thumbnail_height);

			job = ev_job_thumbnail_cairo_new_with_target_size (priv->document,
									   page, priv->rotation,
									   thumbnail_width,
									   thumbnail_height);
			g_object_set_data_full (G_OBJECT (job), "tree_iter",
						gtk_tree_iter_copy (&iter),
						(GDestroyNotify) gtk_tree_iter_free);
			g_signal_connect (job, "finished",
					  G_CALLBACK (thumbnail_job_completed_callback),
					  sidebar_thumbnails);
			gtk_list_store_set (priv->list_store, &iter,
					    COLUMN_JOB, job,
					    -1);
			ev_job_scheduler_push_job (EV_JOB (job), EV_JOB_PRIORITY_HIGH);

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
	int n_pages_in_visible_range;

	/* Preload before and after current visible scrolling range, the same amount of
	 * thumbs in it, to help prevent thumbnail creation happening in the user's sight.
	 * https://bugzilla.gnome.org/show_bug.cgi?id=342110#c15 */
	n_pages_in_visible_range = (end_page - start_page) + 1;
	start_page = MAX (0, start_page - n_pages_in_visible_range);
	end_page = MIN (priv->n_pages - 1, end_page + n_pages_in_visible_range);

	old_start_page = priv->start_page;
	old_end_page = priv->end_page;

	if (start_page == old_start_page &&
	    end_page == old_end_page)
		return;

	/* Clear the areas we no longer display */
	if (old_start_page >= 0 && old_start_page < start_page)
		cancel_running_jobs (sidebar_thumbnails, old_start_page, MIN (start_page - 1, old_end_page));

	if (old_end_page > 0 && old_end_page > end_page)
		cancel_running_jobs (sidebar_thumbnails, MAX (end_page + 1, old_start_page), old_end_page);

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

	/* Widget is not currently visible */
	if (!gtk_widget_get_mapped (GTK_WIDGET (sidebar_thumbnails)))
		return;

	page_size = gtk_adjustment_get_page_size (priv->vadjustment);

	if (page_size == 0)
		return;

	if (priv->icon_view) {
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

static GdkTexture *
gdk_texture_new_for_surface (cairo_surface_t *surface)
{
  GdkTexture *texture;
  GBytes *bytes;

  g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
  g_return_val_if_fail (cairo_image_surface_get_width (surface) > 0, NULL);
  g_return_val_if_fail (cairo_image_surface_get_height (surface) > 0, NULL);

  bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
                                      cairo_image_surface_get_height (surface)
                                      * cairo_image_surface_get_stride (surface),
                                      (GDestroyNotify) cairo_surface_destroy,
                                      cairo_surface_reference (surface));

  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));

  g_bytes_unref (bytes);

  return texture;
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
		cairo_surface_t *loading_icon = NULL;
		GdkTexture *texture;
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

		texture = gdk_texture_new_for_surface (loading_icon);

		gtk_list_store_append (priv->list_store, &iter);
		gtk_list_store_set (priv->list_store, &iter,
				    COLUMN_PAGE_STRING, page_string,
				    COLUMN_SURFACE, texture,
				    COLUMN_THUMBNAIL_SET, FALSE,
				    -1);
		g_free (page_label);
		g_free (page_string);
		cairo_surface_destroy (loading_icon);
	}
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

	if (priv->blank_first_dual_mode) {
		if (page == 0) {
			gtk_icon_view_unselect_path (icon_view, path);
			gtk_tree_path_free (path);
			g_list_free (selected);
			return;
		}
		page--;

	}

	gtk_tree_path_free (path);
	g_list_free (selected);

	ev_document_model_set_page (priv->model, page);
}

static void
ev_sidebar_init_icon_view (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv;

	priv = ev_sidebar_thumbnails->priv;

	g_signal_connect_data (priv->model, "notify::dual-page",
			       G_CALLBACK (check_toggle_blank_first_dual_mode), ev_sidebar_thumbnails,
			       NULL, G_CONNECT_SWAPPED | G_CONNECT_AFTER);

	g_signal_connect_data (priv->model, "notify::dual-odd-left",
			       G_CALLBACK (check_toggle_blank_first_dual_mode), ev_sidebar_thumbnails,
			       NULL, G_CONNECT_SWAPPED | G_CONNECT_AFTER);

	check_toggle_blank_first_dual_mode (ev_sidebar_thumbnails);
}

static void
ev_sidebar_thumbnails_device_scale_factor_changed_cb (EvSidebarThumbnails *sidebar_thumbnails,
                                                      GParamSpec          *pspec)

{
        ev_sidebar_thumbnails_reload (sidebar_thumbnails);
}

static void
ev_sidebar_thumbnails_row_changed (GtkTreeModel *model,
                                   GtkTreePath  *path,
                                   GtkTreeIter  *iter,
                                   gpointer      data)
{
	guint signal_id;

	signal_id = GPOINTER_TO_UINT (data);

	/* PREVENT GtkIconView "row-changed" handler to be reached, as it will
	 * perform a full invalidate and relayout of all items, See bug:
	 * https://bugzilla.gnome.org/show_bug.cgi?id=691448#c9 */
	g_signal_stop_emission (model, signal_id, 0);
}

static void
ev_sidebar_thumbnails_init (EvSidebarThumbnails *ev_sidebar_thumbnails)
{
	EvSidebarThumbnailsPrivate *priv;
	guint signal_id;

	priv = ev_sidebar_thumbnails->priv = ev_sidebar_thumbnails_get_instance_private (ev_sidebar_thumbnails);
	priv->blank_first_dual_mode = FALSE;

	gtk_widget_init_template (GTK_WIDGET (ev_sidebar_thumbnails));

	signal_id = g_signal_lookup ("row-changed", GTK_TYPE_TREE_MODEL);
	g_signal_connect (GTK_TREE_MODEL (priv->list_store), "row-changed",
			  G_CALLBACK (ev_sidebar_thumbnails_row_changed),
			  GUINT_TO_POINTER (signal_id));


	priv->vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->swindow));
	g_signal_connect_data (priv->vadjustment, "value-changed",
			       G_CALLBACK (adjustment_changed_cb),
			       ev_sidebar_thumbnails, NULL,
			       G_CONNECT_SWAPPED | G_CONNECT_AFTER);

	g_signal_connect (ev_sidebar_thumbnails, "notify::scale-factor",
			  G_CALLBACK (ev_sidebar_thumbnails_device_scale_factor_changed_cb), NULL);
}

static void
ev_sidebar_thumbnails_set_current_page (EvSidebarThumbnails *sidebar,
					gint                 page)
{
	GtkTreePath *path;

	if (sidebar->priv->blank_first_dual_mode)
		page++;

	path = gtk_tree_path_new_from_indices (page, -1);

	if (sidebar->priv->icon_view) {

		g_signal_handlers_block_by_func
			(sidebar->priv->icon_view,
			 G_CALLBACK (ev_sidebar_icon_selection_changed), sidebar);

		gtk_icon_view_select_path (GTK_ICON_VIEW (sidebar->priv->icon_view), path);

		g_signal_handlers_unblock_by_func
			(sidebar->priv->icon_view,
			 G_CALLBACK (ev_sidebar_icon_selection_changed), sidebar);

		gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (sidebar->priv->icon_view), path, FALSE, 0.0, 0.0);
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
	g_idle_add_once ((GSourceOnceFunc)adjustment_changed_cb, sidebar_thumbnails);
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
thumbnail_job_completed_callback (EvJobThumbnailCairo *job,
				  EvSidebarThumbnails *sidebar_thumbnails)
{
        GtkWidget                  *widget = GTK_WIDGET (sidebar_thumbnails);
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;
	GtkTreeIter                *iter;
        cairo_surface_t            *surface;
	GdkTexture                 *texture;
        gint                        device_scale;

        if (ev_job_is_failed (EV_JOB (job)))
          return;

        device_scale = gtk_widget_get_scale_factor (widget);
        cairo_surface_set_device_scale (job->thumbnail_surface, device_scale, device_scale);

        surface = ev_document_misc_render_thumbnail_surface_with_frame (widget,
                                                                        job->thumbnail_surface,
                                                                        -1, -1);

	iter = (GtkTreeIter *) g_object_get_data (G_OBJECT (job), "tree_iter");
	if (priv->inverted_colors)
		ev_document_misc_invert_surface (surface);

	texture = gdk_texture_new_for_surface (surface);

	gtk_list_store_set (priv->list_store,
			    iter,
			    COLUMN_SURFACE, texture,
			    COLUMN_THUMBNAIL_SET, TRUE,
			    COLUMN_JOB, NULL,
			    -1);
        cairo_surface_destroy (surface);

	gtk_widget_queue_draw (priv->icon_view);
}

static void
ev_sidebar_thumbnails_document_changed_cb (EvDocumentModel     *model,
					   GParamSpec          *pspec,
					   EvSidebarThumbnails *sidebar_thumbnails)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvSidebarThumbnailsPrivate *priv = sidebar_thumbnails->priv;

	if (ev_document_get_n_pages (document) <= 0 ||
	    !ev_document_check_dimensions (document)) {
		return;
	}

	priv->size_cache = ev_thumbnails_size_cache_get (document);
	priv->document = document;
	priv->n_pages = ev_document_get_n_pages (document);
	priv->rotation = ev_document_model_get_rotation (model);
	priv->inverted_colors = ev_document_model_get_inverted_colors (model);
	if (priv->loading_icons) {
                g_hash_table_remove_all (priv->loading_icons);
	} else {
                priv->loading_icons = g_hash_table_new_full (g_str_hash,
                                                             g_str_equal,
                                                             (GDestroyNotify)g_free,
                                                             (GDestroyNotify)cairo_surface_destroy);
	}

	ev_sidebar_thumbnails_clear_model (sidebar_thumbnails);
	ev_sidebar_thumbnails_fill_model (sidebar_thumbnails);

	if (! priv->icon_view) {
		ev_sidebar_init_icon_view (sidebar_thumbnails);
	} else {
		gtk_widget_queue_resize (priv->icon_view);
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
	return TRUE;
}

static void
ev_sidebar_thumbnails_page_iface_init (EvSidebarPageInterface *iface)
{
	iface->support_document = ev_sidebar_thumbnails_support_document;
	iface->set_model = ev_sidebar_thumbnails_set_model;
}

static gboolean
iter_is_blank_thumbnail (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
	GdkTexture *texture = NULL;
	EvJob *job = NULL;
	gboolean thumbnail_set = FALSE;

	gtk_tree_model_get (tree_model, iter,
			    COLUMN_SURFACE, &texture,
			    COLUMN_THUMBNAIL_SET, &thumbnail_set,
			    COLUMN_JOB, &job, -1);

	/* The blank thumbnail item can be distinguished among all
	 * other items in the GtkIconView as it's the only one which
	 * has the COLUMN_SURFACE as NULL while COLUMN_THUMBNAIL_SET
	 * is set to TRUE. */
	return texture == NULL && job == NULL && thumbnail_set;
}

/* Returns the total horizontal(left+right) width of thumbnail frames.
 * As it was added in ev_document_misc_render_thumbnail_frame() */
static gint
ev_sidebar_thumbnails_frame_horizontal_width (EvSidebarThumbnails *sidebar)
{
        GtkWidget *widget;
        GtkStyleContext *context;
        GtkBorder border = {0, };
        gint offset;

        widget = GTK_WIDGET (sidebar);
        context = gtk_widget_get_style_context (widget);

        gtk_style_context_save (context);

        gtk_style_context_add_class (context, "page-thumbnail");
        gtk_style_context_get_border (context, &border);
        offset = border.left + border.right;

        gtk_style_context_restore (context);

        return offset;
}

static EvWindow *
ev_sidebar_thumbnails_get_ev_window (EvSidebarThumbnails *sidebar)
{
	GtkRoot *toplevel;

	toplevel = gtk_widget_get_root (GTK_WIDGET (sidebar));

	if (toplevel && EV_IS_WINDOW (toplevel))
		return EV_WINDOW (toplevel);

	return NULL;
}

static EvSidebar *
ev_sidebar_thumbnails_get_ev_sidebar (EvSidebarThumbnails *sidebar)
{
	return EV_SIDEBAR (gtk_widget_get_ancestor (GTK_WIDGET (sidebar),
						    EV_TYPE_SIDEBAR));
}

static void
ev_sidebar_thumbnails_get_column_widths (EvSidebarThumbnails *sidebar,
					 gint *one_column_width,
					 gint *two_columns_width,
					 gint *three_columns_width)
{
	EvSidebarThumbnailsPrivate *priv;
	GtkIconView *icon_view;
	gint margin, column_spacing, item_padding, thumbnail_width;
	static gint frame_horizontal_width;

	priv = sidebar->priv;
	icon_view = GTK_ICON_VIEW (priv->icon_view);

	ev_thumbnails_size_cache_get_size (priv->size_cache, 0,
					   priv->rotation,
					   &thumbnail_width, NULL);

	margin = gtk_icon_view_get_margin (icon_view);
	column_spacing = gtk_icon_view_get_column_spacing (icon_view);
	item_padding = gtk_icon_view_get_item_padding (icon_view);
	frame_horizontal_width = ev_sidebar_thumbnails_frame_horizontal_width (sidebar);

	if (one_column_width) {
		*one_column_width = 2 * margin +
				    2 * item_padding +
				    1 * frame_horizontal_width +
				    1 * thumbnail_width +
				    column_spacing;
	}
	if (two_columns_width) {
		*two_columns_width = 2 * margin +
				     4 * item_padding +
				     2 * frame_horizontal_width +
				     2 * thumbnail_width +
				     column_spacing;
	}
	if (three_columns_width) {
		*three_columns_width = 2 * margin +
				       6 * item_padding +
				       3 * frame_horizontal_width +
				       3 * thumbnail_width +
				       2 * column_spacing;
	}
}

static void
ev_sidebar_thumbnails_get_sidebar_width (EvSidebarThumbnails *sidebar,
					 gint *sidebar_width)
{
	EvWindow *ev_window;
	EvSidebarThumbnailsPrivate *priv;

	if (!sidebar_width)
		return;

	priv = sidebar->priv;

	if (priv->width == 0) {
		ev_window = ev_sidebar_thumbnails_get_ev_window (sidebar);
		if (ev_window)
			*sidebar_width = ev_window_get_metadata_sidebar_size (ev_window);
		else
			*sidebar_width = 0;
	} else {
		*sidebar_width = priv->width;
	}
}

static void
ev_sidebar_thumbnails_set_sidebar_width (EvSidebarThumbnails *sidebar,
					 gint sidebar_width)
{
	EvWindow *ev_window;
	EvSidebarThumbnailsPrivate *priv;

	if (sidebar_width <= 0)
		return;

	ev_window = ev_sidebar_thumbnails_get_ev_window (sidebar);
	if (ev_window) {
		priv = sidebar->priv;
		priv->width = sidebar_width;
		ev_window_set_divider_position (ev_window, sidebar_width);
		g_idle_add_once ((GSourceOnceFunc)ev_sidebar_check_reset_current_page, sidebar);
	}
}

/* Returns whether the thumbnail sidebar is currently showing
 * items in a two columns layout */
static gboolean
ev_sidebar_thumbnails_is_two_columns (EvSidebarThumbnails *sidebar)
{
	gint sidebar_width, two_columns_width, three_columns_width;

	ev_sidebar_thumbnails_get_column_widths (sidebar, NULL, &two_columns_width,
						 &three_columns_width);
	ev_sidebar_thumbnails_get_sidebar_width (sidebar, &sidebar_width);

	return sidebar_width >= two_columns_width &&
	       sidebar_width < three_columns_width;
}

/* Returns whether the thumbnail sidebar is currently showing
 * items in a one column layout */
static gboolean
ev_sidebar_thumbnails_is_one_column (EvSidebarThumbnails *sidebar)
{
	gint sidebar_width, one_column_width, two_columns_width;

	ev_sidebar_thumbnails_get_column_widths (sidebar, &one_column_width,
						 &two_columns_width, NULL);
	ev_sidebar_thumbnails_get_sidebar_width (sidebar, &sidebar_width);

	return sidebar_width >= one_column_width &&
	       sidebar_width < two_columns_width;
}

/* If thumbnail sidebar is currently being displayed then
 * it resizes it to be of one column width layout */
static void
ev_sidebar_thumbnails_to_one_column (EvSidebarThumbnails *sidebar)
{
	gint one_column_width;

	ev_sidebar_thumbnails_get_column_widths (sidebar, &one_column_width,
						 NULL, NULL);
	ev_sidebar_thumbnails_set_sidebar_width (sidebar, one_column_width);
}

/* If thumbnail sidebar is currently being displayed then
 * it resizes it to be of two columns width layout */
static void
ev_sidebar_thumbnails_to_two_columns (EvSidebarThumbnails *sidebar)
{
	gint two_columns_width;

	ev_sidebar_thumbnails_get_column_widths (sidebar, NULL,
						 &two_columns_width, NULL);
	ev_sidebar_thumbnails_set_sidebar_width (sidebar, two_columns_width);
}

/* This function checks whether the conditions to insert a blank first item
 * in dual mode are met and activates/deactivates the mode accordingly (that
 * is setting priv->blank_first_dual_mode on/off).
 *
 * Aditionally, we resize the sidebar when asked to do so by following
 * parameter:
 * @resize_sidebar: When true, we will resize sidebar to be one or
 * two columns width, according to whether dual mode is currently off/on.
 * Exception is when user has set sidebar to >=3 columns width, in that
 * case we won't do any resizing to not affect that custom setting */
static void
check_toggle_blank_first_dual_mode_real (EvSidebarThumbnails *sidebar_thumbnails,
					 gboolean resize_sidebar)
{
	EvSidebarThumbnailsPrivate *priv;
	GtkTreeModel *tree_model;
	EvSidebar *sidebar;
	GtkTreeIter first;
	gboolean should_be_enabled, is_two_columns, is_one_column, odd_pages_left, dual_mode;

	priv = sidebar_thumbnails->priv;

	dual_mode = ev_document_model_get_page_layout (priv->model) == EV_PAGE_LAYOUT_DUAL;
	odd_pages_left = ev_document_model_get_dual_page_odd_pages_left (priv->model);
	should_be_enabled = dual_mode && !odd_pages_left;

	is_two_columns = ev_sidebar_thumbnails_is_two_columns (sidebar_thumbnails);
	is_one_column = !is_two_columns && ev_sidebar_thumbnails_is_one_column (sidebar_thumbnails);
	if (should_be_enabled)
		should_be_enabled = is_two_columns || resize_sidebar;

	if (should_be_enabled && !priv->blank_first_dual_mode) {
		/* Do enable it */
		tree_model = GTK_TREE_MODEL (priv->list_store);

		if (!gtk_tree_model_get_iter_first (tree_model, &first))
			return;

		if (is_two_columns || is_one_column) {
			priv->blank_first_dual_mode = TRUE;
			if (iter_is_blank_thumbnail (tree_model, &first))
				return; /* extra check */

			gtk_list_store_insert_with_values (priv->list_store, &first, 0,
							   COLUMN_SURFACE, NULL,
							   COLUMN_THUMBNAIL_SET, TRUE,
							   COLUMN_JOB, NULL,
							   -1);
		}
		if (resize_sidebar && is_one_column) {
			sidebar = ev_sidebar_thumbnails_get_ev_sidebar (sidebar_thumbnails);
			/* If sidebar is set to show thumbnails */
			if (sidebar && ev_sidebar_get_current_page (sidebar) == GTK_WIDGET (sidebar_thumbnails)) {
				ev_sidebar_thumbnails_to_two_columns (sidebar_thumbnails);
			}
		}
	} else if (!should_be_enabled && priv->blank_first_dual_mode) {
		/* Do disable it */
		tree_model = GTK_TREE_MODEL (priv->list_store);

		if (!gtk_tree_model_get_iter_first (tree_model, &first))
			return;

		priv->blank_first_dual_mode = FALSE;
		if (!iter_is_blank_thumbnail (tree_model, &first))
			return; /* extra check */

		gtk_list_store_remove (priv->list_store, &first);

		if (resize_sidebar && is_two_columns) {
			sidebar = ev_sidebar_thumbnails_get_ev_sidebar (sidebar_thumbnails);
			/* If dual_mode disabled and is_two_cols and sidebar is set to show thumbnails */
			if (!dual_mode && sidebar && is_two_columns &&
			    ev_sidebar_get_current_page (sidebar) == GTK_WIDGET (sidebar_thumbnails)) {
				ev_sidebar_thumbnails_to_one_column (sidebar_thumbnails);
			}
		} else if (resize_sidebar && !is_one_column) {
			ev_sidebar_check_reset_current_page (sidebar_thumbnails);
		}
	} else if (resize_sidebar) {
		/* Match sidebar width with dual_mode when sidebar has currently a width of 1 or 2 columns */
		if (dual_mode && is_one_column) {
			sidebar = ev_sidebar_thumbnails_get_ev_sidebar (sidebar_thumbnails);
			if (sidebar && ev_sidebar_get_current_page (sidebar) == GTK_WIDGET (sidebar_thumbnails)) {
				ev_sidebar_thumbnails_to_two_columns (sidebar_thumbnails);
			}
		} else if (!dual_mode && is_two_columns) {
			sidebar = ev_sidebar_thumbnails_get_ev_sidebar (sidebar_thumbnails);
			if (sidebar && ev_sidebar_get_current_page (sidebar) == GTK_WIDGET (sidebar_thumbnails)) {
				ev_sidebar_thumbnails_to_one_column (sidebar_thumbnails);
			}
		} else {
			ev_sidebar_check_reset_current_page (sidebar_thumbnails);
		}
	} else {
		ev_sidebar_check_reset_current_page (sidebar_thumbnails);
	}
}

/* Callback when dual_mode or odd_left preferences are enabled/disabled by the user */
static void
check_toggle_blank_first_dual_mode (EvSidebarThumbnails *sidebar_thumbnails)
{
	check_toggle_blank_first_dual_mode_real (sidebar_thumbnails, TRUE);
}

static void
ev_sidebar_thumbnails_class_init (EvSidebarThumbnailsClass *ev_sidebar_thumbnails_class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_thumbnails_class);
	widget_class = GTK_WIDGET_CLASS (ev_sidebar_thumbnails_class);

	g_object_class->dispose = ev_sidebar_thumbnails_dispose;
	g_object_class->get_property = ev_sidebar_thumbnails_get_property;
	g_object_class->set_property = ev_sidebar_thumbnails_set_property;
	widget_class->map = ev_sidebar_thumbnails_map;
	widget_class->size_allocate = ev_sidebar_thumbnails_size_allocate;

	gtk_widget_class_set_css_name (widget_class, "evsidebarthumbnails");

	gtk_widget_class_set_template_from_resource (widget_class,
				"/org/gnome/evince/ui/sidebar-thumbnails.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarThumbnails, icon_view);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarThumbnails, list_store);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarThumbnails, swindow);

	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_icon_selection_changed);

	g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");
	g_object_class_override_property (g_object_class, PROP_DOCUMENT_MODEL, "document-model");
}
