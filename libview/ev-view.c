/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc
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

#include "config.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "ev-mapping-list.h"
#include "ev-document-forms.h"
#include "ev-document-images.h"
#include "ev-document-links.h"
#include "ev-document-layers.h"
#include "ev-document-media.h"
#include "ev-document-misc.h"

#include "ev-view.h"
#include "ev-view-private.h"

#include "ev-form-field-private.h"
#include "ev-pixbuf-cache.h"
#include "ev-page-cache.h"
#include "ev-view-marshal.h"
#include "ev-document-annotations.h"
#include "ev-annotation-window.h"
#include "ev-view-type-builtins.h"
#include "ev-debug.h"

enum {
	SIGNAL_SCROLL,
	SIGNAL_HANDLE_LINK,
	SIGNAL_EXTERNAL_LINK,
	SIGNAL_POPUP_MENU,
	SIGNAL_SELECTION_CHANGED,
	SIGNAL_SYNC_SOURCE,
	SIGNAL_ANNOT_ADDED,
	SIGNAL_ANNOT_CANCEL_ADD,
	SIGNAL_ANNOT_CHANGED,
	SIGNAL_ANNOT_REMOVED,
	SIGNAL_LAYERS_CHANGED,
	SIGNAL_MOVE_CURSOR,
	SIGNAL_CURSOR_MOVED,
	SIGNAL_ACTIVATE,
	N_SIGNALS
};

enum {
	PROP_0,
	PROP_IS_LOADING,
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY,
	PROP_CAN_ZOOM_IN,
	PROP_CAN_ZOOM_OUT
};

static guint signals[N_SIGNALS];

typedef enum {
	EV_VIEW_FIND_NEXT,
	EV_VIEW_FIND_PREV
} EvViewFindDirection;

typedef struct {
	/* View coords */
	gint        x;
	gint        y;

	/* Document */
	guint       page;
	EvRectangle doc_rect;
} EvViewChild;

#define MIN_SCALE 0.05409 /* large documents (comics) need a small value, see #702 */
#define ZOOM_IN_FACTOR  1.2
#define ZOOM_OUT_FACTOR (1.0/ZOOM_IN_FACTOR)

#define SCROLL_TIME 150
#define SCROLL_PAGE_THRESHOLD 0.7

#define DEFAULT_PIXBUF_CACHE_SIZE 52428800 /* 50MB */

#define EV_STYLE_CLASS_DOCUMENT_PAGE "document-page"
#define EV_STYLE_CLASS_INVERTED      "inverted"
#define EV_STYLE_CLASS_FIND_RESULTS  "find-results"

#define ANNOT_POPUP_WINDOW_DEFAULT_WIDTH  200
#define ANNOT_POPUP_WINDOW_DEFAULT_HEIGHT 150
#define ANNOTATION_ICON_SIZE 24

#define LINK_PREVIEW_PAGE_RATIO 1.0 / 3.0     /* Size of popover with respect to page size */
#define LINK_PREVIEW_HORIZONTAL_LINK_POS 0.5  /* as fraction of preview width */
#define LINK_PREVIEW_VERTICAL_LINK_POS 0.3    /* as fraction of preview height */
#define LINK_PREVIEW_DELAY_MS 300             /* Delay before showing preview in milliseconds */

#define SCROLL_SENSITIVITY_THRESHOLD 5

/*** Geometry computations ***/
static void       compute_border                             (EvView             *view,
							      GtkBorder          *border);
static void       get_page_y_offset                          (EvView             *view,
							      int                 page,
							      int                *y_offset,
							      GtkBorder          *border);
static void       find_page_at_location                      (EvView             *view,
							      gdouble             x,
							      gdouble             y,
							      gint               *page,
							      gint               *x_offset,
							      gint               *y_offset);
static gboolean   real_ev_view_get_page_extents              (EvView             *view,
							      gint                page,
							      GdkRectangle       *page_area,
							      GtkBorder          *border,
							      gboolean            use_passed_border);
/*** Hyperrefs ***/
static EvLink *   ev_view_get_link_at_location 		     (EvView             *view,
				  	         	      gdouble             x,
		            				      gdouble             y);
static char*      tip_from_link                              (EvView             *view,
							      EvLink             *link);
static void       ev_view_link_preview_popover_cleanup       (EvView             *view);
static void       get_link_area                              (EvView             *view,
							      gint                x,
							      gint                y,
							      EvLink             *link,
							      GdkRectangle       *area);
static void       link_preview_show_thumbnail                (GdkTexture    *page_surface,
							      EvView             *view);
static void       link_preview_job_finished_cb               (EvJobThumbnailCairo     *job,
							      EvView             *view);
static void       link_preview_delayed_show                  (EvView *view);
/*** Forms ***/
static EvFormField *ev_view_get_form_field_at_location       (EvView             *view,
							       gdouble            x,
							       gdouble            y);
/*** Media ***/
static EvMedia     *ev_view_get_media_at_location            (EvView             *view,
							      gdouble             x,
							      gdouble             y);
static gboolean     ev_view_find_player_for_media            (EvView             *view,
							      EvMedia            *media);
/*** Annotations ***/
static GtkWidget    *get_window_for_annot 		     (EvView 		 *view,
							      EvAnnotation	 *annot);
static void          map_annot_to_window		     (EvView		 *view,
							      EvAnnotation	 *annot,
							      GtkWidget		 *window);
static EvAnnotation *ev_view_get_annotation_at_location      (EvView             *view,
							      gdouble             x,
							      gdouble             y);
static void          show_annotation_windows                 (EvView             *view,
							      gint                page);
static void          hide_annotation_windows                 (EvView             *view,
							      gint                page);
static void	     ev_view_create_annotation_from_selection (EvView          *view,
							       EvViewSelection *selection);
/*** GtkWidget implementation ***/
static void       ev_view_size_request_continuous_dual_page  (EvView             *view,
							      GtkRequisition     *requisition);
static void       ev_view_size_request_continuous            (EvView             *view,
							      GtkRequisition     *requisition);
static void       ev_view_size_request_dual_page             (EvView             *view,
							      GtkRequisition     *requisition);
static void       ev_view_size_request_single_page           (EvView             *view,
							      GtkRequisition     *requisition);
static void       ev_view_size_request                       (GtkWidget          *widget,
							      GtkRequisition     *requisition);
static void       ev_view_size_allocate                      (GtkWidget          *widget,
							      int		  width,
							      int		  height,
							      int		  baseline);
static void       ev_view_remove_all                         (EvView             *view);
static void       ev_view_remove_all_form_fields             (EvView             *view);

/*** Drawing ***/
static void       highlight_find_results                     (EvView             *view,
                                                              GtkSnapshot        *snapshot,
							      int                 page);
static void       highlight_forward_search_results           (EvView             *view,
                                                              GtkSnapshot        *snapshot,
							      int                 page);
static void       draw_one_page                              (EvView             *view,
							      gint                page,
							      GtkSnapshot        *snapshot,
							      GdkRectangle       *page_area,
							      GtkBorder          *border,
							      GdkRectangle       *expose_area,
							      gboolean		 *page_ready);
static void       ev_view_reload_page                        (EvView             *view,
							      gint                page,
							      cairo_region_t     *region);
/*** Callbacks ***/
static void       ev_view_change_page                        (EvView             *view,
							      gint                new_page);
static void       job_finished_cb                            (EvPixbufCache      *pixbuf_cache,
							      cairo_region_t     *region,
							      EvView             *view);
static void       ev_view_page_changed_cb                    (EvDocumentModel    *model,
							      gint                old_page,
							      gint                new_page,
							      EvView             *view);
static void       adjustment_value_changed_cb                (GtkAdjustment      *adjustment,
							      EvView             *view);
/*** GObject ***/
static void       ev_view_finalize                           (GObject            *object);
static void       ev_view_dispose                            (GObject            *object);
static void       ev_view_class_init                         (EvViewClass        *class);
static void       ev_view_init                               (EvView             *view);

/*** Zoom and sizing ***/
static double   zoom_for_size_fit_width	 		     (gdouble doc_width,
							      gdouble doc_height,
	    						      int     target_width,
							      int     target_height);
static double   zoom_for_size_fit_height		     (gdouble doc_width,
			  				      gdouble doc_height,
							      int     target_width,
							      int     target_height);
static double	zoom_for_size_fit_page 			     (gdouble doc_width,
							      gdouble doc_height,
							      int     target_width,
							      int     target_height);
static double   zoom_for_size_automatic                      (GtkWidget *widget,
							      gdouble    doc_width,
							      gdouble    doc_height,
							      int        target_width,
							      int        target_height);
static gboolean ev_view_can_zoom                             (EvView *view,
                                                              gdouble factor);
static void     ev_view_zoom                                 (EvView *view,
                                                              gdouble factor);
static void     ev_view_zoom_for_size                        (EvView *view,
							      int     width,
							      int     height);
static void	ev_view_zoom_for_size_continuous_and_dual_page (EvView *view,
							        int     width,
						     	        int     height);
static void	ev_view_zoom_for_size_continuous	       (EvView *view,
					    		        int     width,
								int     height);
static void 	ev_view_zoom_for_size_dual_page 	       (EvView *view,
						    		int     width,
								int     height);
static void	ev_view_zoom_for_size_single_page 	       (EvView *view,
				    			        int     width,
					    			int     height);
static gboolean	ev_view_page_fits			       (EvView         *view,
								GtkOrientation  orientation);
/*** Cursors ***/
static void       ev_view_set_cursor                         (EvView             *view,
							      EvViewCursor        new_cursor);
static void       handle_cursor_over_link                    (EvView *view,
							      EvLink *link,
							      gint x,
							      gint y,
							      gboolean from_motion);
static void       ev_view_handle_cursor_over_xy              (EvView *view,
							      gint x,
							      gint y,
							      gboolean from_motion);

/*** Find ***/
static gint         ev_view_find_get_n_results               (EvView             *view,
							      gint                page);
static EvFindRectangle *ev_view_find_get_result              (EvView             *view,
							      gint                page,
							      gint                result);
static void       jump_to_find_result                        (EvView             *view);
static void       jump_to_find_page                          (EvView             *view,
							      EvViewFindDirection direction,
							      gint                shift);
/*** Selection ***/
static void       compute_selections                         (EvView             *view,
							      EvSelectionStyle    style,
							      GdkPoint           *start,
							      GdkPoint           *stop);
static void       extend_selection                           (EvView             *view,
							      GdkPoint           *start,
							      GdkPoint           *stop);
static void       clear_selection                            (EvView             *view);
static void       selection_free                             (EvViewSelection    *selection);
static char*      get_selected_text                          (EvView             *ev_view);

/*** Caret navigation ***/
static void       ev_view_check_cursor_blink                 (EvView             *ev_view);


G_DEFINE_TYPE_WITH_CODE (EvView, ev_view, GTK_TYPE_WIDGET,
			 G_ADD_PRIVATE (EvView)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

#define GET_PRIVATE(o) ev_view_get_instance_private (o)

/* HeightToPage cache */
#define EV_HEIGHT_TO_PAGE_CACHE_KEY "ev-height-to-page-cache"

static void
ev_view_build_height_to_page_cache (EvView		*view,
                                    EvHeightToPageCache *cache)
{
	gboolean swap, uniform;
	int i;
	double uniform_height, page_height, next_page_height;
	double saved_height;
	gdouble u_width, u_height;
	gint n_pages;
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvDocument *document = priv->document;

	swap = (priv->rotation == 90 || priv->rotation == 270);

	uniform = ev_document_is_page_size_uniform (document);
	n_pages = ev_document_get_n_pages (document);

	g_free (cache->height_to_page);
	g_free (cache->dual_height_to_page);

	cache->rotation = priv->rotation;
	cache->dual_even_left = priv->dual_even_left;
	cache->height_to_page = g_new0 (gdouble, n_pages + 1);
	cache->dual_height_to_page = g_new0 (gdouble, n_pages + 2);

	if (uniform)
		ev_document_get_page_size (document, 0, &u_width, &u_height);

	saved_height = 0;
	for (i = 0; i <= n_pages; i++) {
		if (uniform) {
			uniform_height = swap ? u_width : u_height;
			cache->height_to_page[i] = i * uniform_height;
		} else {
			if (i < n_pages) {
				gdouble w, h;

				ev_document_get_page_size (document, i, &w, &h);
				page_height = swap ? w : h;
			} else {
				page_height = 0;
			}
			cache->height_to_page[i] = saved_height;
			saved_height += page_height;
		}
	}

	if (cache->dual_even_left && !uniform) {
		gdouble w, h;

		ev_document_get_page_size (document, 0, &w, &h);
		saved_height = swap ? w : h;
	} else {
		saved_height = 0;
	}

	for (i = cache->dual_even_left; i < n_pages + 2; i += 2) {
    		if (uniform) {
			uniform_height = swap ? u_width : u_height;
			cache->dual_height_to_page[i] = ((i + cache->dual_even_left) / 2) * uniform_height;
			if (i + 1 < n_pages + 2)
				cache->dual_height_to_page[i + 1] = ((i + cache->dual_even_left) / 2) * uniform_height;
		} else {
			if (i + 1 < n_pages) {
				gdouble w, h;

				ev_document_get_page_size (document, i + 1, &w, &h);
				next_page_height = swap ? w : h;
			} else {
				next_page_height = 0;
			}

			if (i < n_pages) {
				gdouble w, h;

				ev_document_get_page_size (document, i, &w, &h);
				page_height = swap ? w : h;
			} else {
				page_height = 0;
			}

			if (i + 1 < n_pages + 2) {
				cache->dual_height_to_page[i] = saved_height;
				cache->dual_height_to_page[i + 1] = saved_height;
				saved_height += MAX(page_height, next_page_height);
			} else {
				cache->dual_height_to_page[i] = saved_height;
			}
		}
	}
}

static void
ev_height_to_page_cache_free (EvHeightToPageCache *cache)
{
	g_clear_pointer (&cache->height_to_page, g_free);
	g_clear_pointer (&cache->dual_height_to_page, g_free);
	g_free (cache);
}

static EvHeightToPageCache *
ev_view_get_height_to_page_cache (EvView *view)
{
	EvHeightToPageCache *cache;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return NULL;

	cache = g_object_get_data (G_OBJECT (priv->document), EV_HEIGHT_TO_PAGE_CACHE_KEY);
	if (!cache) {
		cache = g_new0 (EvHeightToPageCache, 1);
		ev_view_build_height_to_page_cache (view, cache);
		g_object_set_data_full (G_OBJECT (priv->document),
					EV_HEIGHT_TO_PAGE_CACHE_KEY,
					cache,
					(GDestroyNotify)ev_height_to_page_cache_free);
	}

	return cache;
}

static void
ev_view_get_height_to_page (EvView *view,
			    gint    page,
			    gint   *height,
			    gint   *dual_height)
{
	EvHeightToPageCache *cache = NULL;
	EvViewPrivate *priv = GET_PRIVATE (view);
	gdouble h, dh;

	if (!priv->height_to_page_cache)
		return;

	cache = priv->height_to_page_cache;
	if (cache->rotation != priv->rotation ||
	    cache->dual_even_left != priv->dual_even_left) {
		ev_view_build_height_to_page_cache (view, cache);
	}

	if (height) {
		h = cache->height_to_page[page];
		*height = (gint)(h * priv->scale + 0.5);
    }

	if (dual_height) {
		dh = cache->dual_height_to_page[page];
		*dual_height = (gint)(dh * priv->scale + 0.5);
	}
}

static gboolean
is_dual_page (EvView   *view,
	      gboolean *odd_left_out)
{
	gboolean dual = FALSE;
	gboolean odd_left = FALSE;
	EvViewPrivate *priv = GET_PRIVATE (view);

	switch (priv->page_layout) {
	case EV_PAGE_LAYOUT_AUTOMATIC: {
		double        scale;
		double        doc_width;
		double        doc_height;

		scale = ev_document_misc_get_widget_dpi (GTK_WIDGET (view)) / 72.0;

		ev_document_get_max_page_size (priv->document, &doc_width, &doc_height);

		/* If the width is ok and the height is pretty close, try to fit it in */
		if (ev_document_get_n_pages (priv->document) > 1 &&
		    doc_width < doc_height &&
		    gtk_widget_get_width (GTK_WIDGET (view)) > (2 * doc_width * scale) &&
		    gtk_widget_get_height (GTK_WIDGET (view)) > (doc_height * scale * 0.9)) {
			odd_left = !priv->dual_even_left;
			dual = TRUE;
		}
	}
		break;
	case EV_PAGE_LAYOUT_DUAL:
		odd_left = !priv->dual_even_left;
		if (ev_document_get_n_pages (priv->document) > 1)
			dual = TRUE;
		break;
	case EV_PAGE_LAYOUT_SINGLE:
		break;
	default:
		g_assert_not_reached ();
	}

	if (odd_left_out)
		*odd_left_out = odd_left;

	return dual;
}

static void
scroll_to_point (EvView        *view,
		 gdouble        x,
		 gdouble        y,
		 GtkOrientation orientation)
{
	gdouble page_size;
	gdouble upper, lower;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (orientation == GTK_ORIENTATION_VERTICAL) {
		page_size = gtk_adjustment_get_page_size (priv->vadjustment);
		upper = gtk_adjustment_get_upper (priv->vadjustment);
		lower = gtk_adjustment_get_lower (priv->vadjustment);

		if (priv->continuous) {
			gtk_adjustment_clamp_page (priv->vadjustment,
						   y, y + page_size);
		} else {
			gtk_adjustment_set_value (priv->vadjustment,
						  CLAMP (y, lower, upper - page_size));
		}
	} else {
		page_size = gtk_adjustment_get_page_size (priv->hadjustment);
		upper = gtk_adjustment_get_upper (priv->hadjustment);
		lower = gtk_adjustment_get_lower (priv->hadjustment);

		if (is_dual_page (view, NULL)) {
			gtk_adjustment_clamp_page (priv->hadjustment, x,
						   x + page_size);
		} else {
			gtk_adjustment_set_value (priv->hadjustment,
						  CLAMP (x, lower, upper - page_size));
		}
	}
}

static void
ev_view_scroll_to_page_position (EvView *view, GtkOrientation orientation)
{
	gdouble x, y;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return;

	if ((orientation == GTK_ORIENTATION_VERTICAL && priv->pending_point.y == 0) ||
	    (orientation == GTK_ORIENTATION_HORIZONTAL && priv->pending_point.x == 0)) {
		GdkRectangle page_area;
		GtkBorder    border;

		ev_view_get_page_extents (view, priv->current_page, &page_area, &border);
		x = page_area.x;
		y = page_area.y;

		if (priv->continuous && priv->sizing_mode == EV_SIZING_FIT_PAGE) {
			y -= priv->spacing + (border.top / 2);
		}

		if (priv->current_page == 0)
			y = 0.;
	} else {
		GdkPoint view_point;

		_ev_view_transform_doc_point_to_view_point (view, priv->current_page,
							    &priv->pending_point, &view_point);
		x = view_point.x;
		y = view_point.y;
	}

	scroll_to_point (view, x, y, orientation);
}

static void
ev_view_set_loading (EvView       *view,
		     gboolean      loading)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (priv->loading == loading)
		return;

	priv->loading = loading;
	g_object_notify (G_OBJECT (view), "is-loading");
}

static void
ev_view_set_adjustment_values (EvView         *view,
			       GtkOrientation  orientation,
			       int	       width,
			       int	       height)
{
	GtkAdjustment *adjustment;
	gint req_size, alloc_size, new_value;
	gdouble page_size, value, upper, factor, zoom_center;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (orientation == GTK_ORIENTATION_HORIZONTAL)  {
		req_size = priv->requisition.width;
		alloc_size = gtk_widget_get_width (GTK_WIDGET (view));
		adjustment = priv->hadjustment;
		zoom_center = priv->zoom_center_x;
	} else {
		req_size = priv->requisition.height;
		alloc_size = gtk_widget_get_height (GTK_WIDGET (view));
		adjustment = priv->vadjustment;
		zoom_center = priv->zoom_center_y;
	}

	if (!adjustment)
		return;

	factor = 1.0;
	value = gtk_adjustment_get_value (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);
	if (zoom_center < 0)
		zoom_center = page_size * 0.5;

	if (upper != .0) {
		switch (priv->pending_scroll) {
    	        case SCROLL_TO_KEEP_POSITION:
    	        case SCROLL_TO_FIND_LOCATION:
			factor = value / upper;
			break;
    	        case SCROLL_TO_PAGE_POSITION:
			break;
		case SCROLL_TO_CENTER:
			factor = (value + zoom_center) / upper;
			break;
		}
	}

	upper = MAX (alloc_size, req_size);
	page_size = alloc_size;

	gtk_adjustment_configure (adjustment, value, 0, upper,
			alloc_size * 0.1, alloc_size * 0.9, page_size);

	/*
	 * We add 0.5 to the values before to average out our rounding errors.
	 */
	switch (priv->pending_scroll) {
    	        case SCROLL_TO_KEEP_POSITION:
    	        case SCROLL_TO_FIND_LOCATION:
			new_value = CLAMP (upper * factor + 0.5, 0, upper - page_size);
			gtk_adjustment_set_value (adjustment, new_value);
			break;
    	        case SCROLL_TO_PAGE_POSITION:
			ev_view_scroll_to_page_position (view, orientation);
			break;
	        case SCROLL_TO_CENTER:
			new_value = CLAMP (upper * factor - zoom_center + 0.5, 0, upper - page_size);
			if (orientation == GTK_ORIENTATION_HORIZONTAL)
				priv->zoom_center_x = -1.0;
			else
				priv->zoom_center_y = -1.0;
			gtk_adjustment_set_value (adjustment, new_value);
			break;
	}
}

static void
view_update_range_and_current_page (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint start = priv->start_page;
	gint end = priv->end_page;
	gboolean odd_left;

	if (ev_document_get_n_pages (priv->document) <= 0 ||
	    !ev_document_check_dimensions (priv->document))
		return;

	if (priv->continuous) {
		GdkRectangle current_area, unused, page_area;
		GtkBorder border;
		gboolean found = FALSE;
		gint area_max = -1, area;
		gint best_current_page = -1;
		gint n_pages;
		int i, j = 0;

		if (!(priv->vadjustment && priv->hadjustment))
			return;

		current_area.x = gtk_adjustment_get_value (priv->hadjustment);
		current_area.width = gtk_adjustment_get_page_size (priv->hadjustment);
		current_area.y = gtk_adjustment_get_value (priv->vadjustment);
		current_area.height = gtk_adjustment_get_page_size (priv->vadjustment);

		n_pages = ev_document_get_n_pages (priv->document);
		compute_border (view, &border);
		for (i = 0; i < n_pages; i++) {

			ev_view_get_page_extents_for_border (view, i, &border, &page_area);

			if (gdk_rectangle_intersect (&current_area, &page_area, &unused)) {
				area = unused.width * unused.height;

				if (!found) {
					area_max = area;
					priv->start_page = i;
					found = TRUE;
					best_current_page = i;
				}
				if (area > area_max) {
					best_current_page = (area == area_max) ? MIN (i, best_current_page) : i;
					area_max = area;
				}

				priv->end_page = i;
				j = 0;
			} else if (found && priv->current_page <= priv->end_page) {
				if (is_dual_page (view, NULL) && j < 1) {
					/* In dual mode  we stop searching
					 * after two consecutive non-visible pages.
					 */
					j++;
					continue;
				}
				break;
			}
		}

		if (priv->pending_scroll == SCROLL_TO_KEEP_POSITION ||
		    priv->pending_scroll == SCROLL_TO_FIND_LOCATION) {
			best_current_page = MAX (best_current_page, priv->start_page);

			if (best_current_page >= 0 && priv->current_page != best_current_page) {
				priv->current_page = best_current_page;
				ev_view_set_loading (view, FALSE);
				ev_document_model_set_page (priv->model, best_current_page);
			}
		}
	} else if (is_dual_page (view, &odd_left)) {
		if (priv->current_page % 2 == !odd_left) {
			priv->start_page = priv->current_page;
			if (priv->current_page + 1 < ev_document_get_n_pages (priv->document))
				priv->end_page = priv->start_page + 1;
			else
				priv->end_page = priv->start_page;
		} else {
			if (priv->current_page < 1)
				priv->start_page = priv->current_page;
			else
				priv->start_page = priv->current_page - 1;
			priv->end_page = priv->current_page;
		}
	} else {
		priv->start_page = priv->current_page;
		priv->end_page = priv->current_page;
	}

	if (priv->start_page == -1 || priv->end_page == -1)
		return;

	if (start < priv->start_page || end > priv->end_page) {
		gint i;

		for (i = start; i < priv->start_page && start != -1; i++) {
			hide_annotation_windows (view, i);
		}

		for (i = end; i > priv->end_page && end != -1; i--) {
			hide_annotation_windows (view, i);
		}

		ev_view_check_cursor_blink (view);
	}

	ev_page_cache_set_page_range (priv->page_cache,
				      priv->start_page,
				      priv->end_page);
	ev_pixbuf_cache_set_page_range (priv->pixbuf_cache,
					priv->start_page,
					priv->end_page,
					priv->selection_info.selections);
#if 0
	if (priv->accessible)
		ev_view_accessible_set_page_range (EV_VIEW_ACCESSIBLE (priv->accessible),
						   priv->start_page,
						   priv->end_page);
#endif

	if (ev_pixbuf_cache_get_texture (priv->pixbuf_cache, priv->current_page))
		gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
ev_view_set_scroll_adjustment (EvView         *view,
			       GtkOrientation  orientation,
			       GtkAdjustment  *adjustment)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GtkAdjustment **to_set;
	const gchar    *prop_name;

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		to_set = &priv->hadjustment;
		prop_name = "hadjustment";
	} else {
		to_set = &priv->vadjustment;
		prop_name = "vadjustment";
	}

	if (adjustment && adjustment == *to_set)
		return;

	if (*to_set) {
		g_signal_handlers_disconnect_by_func (*to_set,
						      (gpointer) adjustment_value_changed_cb,
						      view);

		g_object_unref (*to_set);
	}

	if (!adjustment)
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	g_signal_connect (adjustment, "value-changed",
			  G_CALLBACK (adjustment_value_changed_cb),
			  view);
	*to_set = g_object_ref_sink (adjustment);

	g_object_notify (G_OBJECT (view), prop_name);
}

static void
add_scroll_binding_keypad (GtkWidgetClass *widget_class,
    			   guint           keyval,
    			   GdkModifierType modifiers,
    			   GtkScrollType   scroll,
			   GtkOrientation  orientation)
{
	guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;

	gtk_widget_class_add_binding_signal (widget_class, keyval, modifiers,
				      "scroll", "(ii)", scroll, orientation);
	gtk_widget_class_add_binding_signal (widget_class, keypad_keyval, modifiers,
				      "scroll", "(ii)", scroll, orientation);
}

static gdouble
compute_scroll_increment (EvView        *view,
			  GtkScrollType  scroll)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GtkAdjustment *adjustment = priv->vadjustment;
	cairo_region_t *text_region, *region;
	gint page;
	GdkRectangle rect;
	EvRectangle doc_rect;
	GdkRectangle page_area;
	GtkBorder border;
	gdouble fraction = 1.0;

	if (scroll != GTK_SCROLL_PAGE_BACKWARD && scroll != GTK_SCROLL_PAGE_FORWARD)
		return gtk_adjustment_get_page_size (adjustment);

	page = scroll == GTK_SCROLL_PAGE_BACKWARD ? priv->start_page : priv->end_page;

	text_region = ev_page_cache_get_text_mapping (priv->page_cache, page);
	if (!text_region || cairo_region_is_empty (text_region))
		return gtk_adjustment_get_page_size (adjustment);

	ev_view_get_page_extents (view, page, &page_area, &border);
	rect.x = page_area.x + priv->scroll_x;
	rect.y = priv->scroll_y + (scroll == GTK_SCROLL_PAGE_BACKWARD ? 5 : gtk_widget_get_height (GTK_WIDGET (view)) - 5);
	rect.width = page_area.width;
	rect.height = 1;
	_ev_view_transform_view_rect_to_doc_rect (view, &rect, &page_area, &border, &doc_rect);

	/* Convert the doc rectangle into a GdkRectangle */
	rect.x = doc_rect.x1;
	rect.y = doc_rect.y1;
	rect.width = doc_rect.x2 - doc_rect.x1;
	rect.height = MAX (1, doc_rect.y2 - doc_rect.y1);
	region = cairo_region_create_rectangle (&rect);

	cairo_region_intersect (region, text_region);
	if (cairo_region_num_rectangles (region)) {
		EvRenderContext *rc;
		EvPage  *ev_page;
		cairo_region_t *sel_region;

		cairo_region_get_rectangle (region, 0, &rect);
		ev_page = ev_document_get_page (priv->document, page);
		rc = ev_render_context_new (ev_page, priv->rotation, 0.);
		ev_render_context_set_target_size (rc,
						   page_area.width - (border.left + border.right),
						   page_area.height - (border.left + border.right));
		g_object_unref (ev_page);
		/* Get the selection region to know the height of the line */
		doc_rect.x1 = doc_rect.x2 = rect.x + 0.5;
		doc_rect.y1 = doc_rect.y2 = rect.y + 0.5;

		ev_document_doc_mutex_lock ();
		sel_region = ev_selection_get_selection_region (EV_SELECTION (priv->document),
								rc, EV_SELECTION_STYLE_LINE,
								&doc_rect);
		ev_document_doc_mutex_unlock ();

		g_object_unref (rc);

		if (cairo_region_num_rectangles (sel_region) > 0) {
			cairo_region_get_rectangle (sel_region, 0, &rect);
			fraction = 1 - (rect.height / gtk_adjustment_get_page_size (adjustment));
			/* jump the full page height if the line is too large a
			 * fraction of the page */
			if (fraction < SCROLL_PAGE_THRESHOLD)
				fraction = 1.0;
		}
		cairo_region_destroy (sel_region);
	}
	cairo_region_destroy (region);

	return gtk_adjustment_get_page_size (adjustment) * fraction;

}

static void
ev_view_first_page (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	ev_document_model_set_page (priv->model, 0);
}

static void
ev_view_last_page (EvView *view)
{
	gint n_pages;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return;

	n_pages = ev_document_get_n_pages (priv->document);
	if (n_pages <= 1)
		return;

	ev_document_model_set_page (priv->model, n_pages - 1);
}

static void
ev_view_scroll (EvView        *view,
		GtkScrollType  scroll,
		GtkOrientation orientation)
{
	GtkAdjustment *adjustment;
	gdouble value, increment, upper, lower, page_size, step_increment;
	gboolean first_page = FALSE, last_page = FALSE;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->key_binding_handled || priv->caret_enabled)
		return;

	priv->jump_to_find_result = FALSE;

	if (ev_view_page_fits (view, orientation)) {
		switch (scroll) {
			case GTK_SCROLL_PAGE_BACKWARD:
			case GTK_SCROLL_STEP_BACKWARD:
				ev_view_previous_page (view);
				break;
			case GTK_SCROLL_PAGE_FORWARD:
			case GTK_SCROLL_STEP_FORWARD:
				ev_view_next_page (view);
				break;
		        case GTK_SCROLL_START:
				ev_view_first_page (view);
				break;
		        case GTK_SCROLL_END:
				ev_view_last_page (view);
				break;
			default:
				break;
		}
		return;
	}

	/* Assign values for increment and vertical adjustment */
	adjustment = orientation == GTK_ORIENTATION_HORIZONTAL ?
			priv->hadjustment : priv->vadjustment;
	value = gtk_adjustment_get_value (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);
	step_increment = gtk_adjustment_get_step_increment (adjustment);

	/* Assign boolean for first and last page */
	if (priv->current_page == 0)
		first_page = TRUE;
	if (priv->current_page == ev_document_get_n_pages (priv->document) - 1)
		last_page = TRUE;

	switch (scroll) {
		case GTK_SCROLL_PAGE_BACKWARD:
			/* Do not jump backwards if at the first page */
			if (value == lower && first_page) {
				/* Do nothing */
				/* At the top of a page, assign the upper bound limit of previous page */
			} else if (value == lower) {
				value = upper - page_size;
				ev_view_previous_page (view);
				/* Jump to the top */
			} else {
				increment = compute_scroll_increment (view, GTK_SCROLL_PAGE_BACKWARD);
				value = MAX (value - increment, lower);
			}
			break;
		case GTK_SCROLL_PAGE_FORWARD:
			/* Do not jump forward if at the last page */
			if (value == (upper - page_size) && last_page) {
				/* Do nothing */
			/* At the bottom of a page, assign the lower bound limit of next page */
			} else if (value == (upper - page_size)) {
				value = 0;
				ev_view_next_page (view);
			/* Jump to the bottom */
			} else {
				increment = compute_scroll_increment (view, GTK_SCROLL_PAGE_FORWARD);
				value = MIN (value + increment, upper - page_size);
			}
			break;
	        case GTK_SCROLL_STEP_BACKWARD:
			value -= step_increment;
			break;
	        case GTK_SCROLL_STEP_FORWARD:
			value += step_increment;
			break;
        	case GTK_SCROLL_STEP_DOWN:
			value -= step_increment / 10;
			break;
        	case GTK_SCROLL_STEP_UP:
			value += step_increment / 10;
			break;
	        case GTK_SCROLL_START:
			value = lower;
			if (!first_page)
				ev_view_first_page (view);
			break;
	        case GTK_SCROLL_END:
			value = upper - page_size;
			if (!last_page)
				ev_view_last_page (view);
			/* Changing pages causes the top to be shown. Here we want the bottom shown. */
			priv->pending_point.y = value;
			break;
        	default:
			break;
	}

	value = CLAMP (value, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, value);
}

#define MARGIN 5

void
_ev_view_ensure_rectangle_is_visible (EvView *view, GdkRectangle *rect)
{
	GtkAdjustment *adjustment;
	gdouble adj_value;
	int value;
	EvViewPrivate *priv = GET_PRIVATE (view);
	int widget_width = gtk_widget_get_width (GTK_WIDGET (view));
	int widget_height = gtk_widget_get_height (GTK_WIDGET (view));

	priv->pending_scroll = SCROLL_TO_FIND_LOCATION;

	adjustment = priv->vadjustment;
	adj_value = gtk_adjustment_get_value (adjustment);

	if (rect->y < adj_value) {
		value = MAX (gtk_adjustment_get_lower (adjustment), rect->y - MARGIN);
		gtk_adjustment_set_value (priv->vadjustment, value);
	} else if (rect->y + rect->height > adj_value + widget_height) {
		value = MIN (gtk_adjustment_get_upper (adjustment), rect->y + rect->height -
			     widget_height + MARGIN);
		gtk_adjustment_set_value (priv->vadjustment, value);
	}

	adjustment = priv->hadjustment;
	adj_value = gtk_adjustment_get_value (adjustment);

	if (rect->x < adj_value) {
		value = MAX (gtk_adjustment_get_lower (adjustment), rect->x - MARGIN);
		gtk_adjustment_set_value (priv->hadjustment, value);
	} else if (rect->x + rect->height > adj_value + widget_width) {
		value = MIN (gtk_adjustment_get_upper (adjustment), rect->x + rect->width -
			     widget_width + MARGIN);
		gtk_adjustment_set_value (priv->hadjustment, value);
	}
}

/*** Geometry computations ***/

static void
compute_border (EvView *view, GtkBorder *border)
{
	GtkWidget       *widget = GTK_WIDGET (view);
	GtkStyleContext *context = gtk_widget_get_style_context (widget);

	gtk_style_context_save (context);
	gtk_style_context_add_class (context, EV_STYLE_CLASS_DOCUMENT_PAGE);
	gtk_style_context_get_border (context, border);
	gtk_style_context_restore (context);
}

void
_get_page_size_for_scale_and_rotation (EvDocument *document,
				       gint        page,
				       gdouble     scale,
				       gint        rotation,
				       gint       *page_width,
				       gint       *page_height)
{
	gdouble w, h;
	gint    width, height;

	ev_document_get_page_size (document, page, &w, &h);

	width = (gint)(w * scale + 0.5);
	height = (gint)(h * scale + 0.5);

	if (page_width)
		*page_width = (rotation == 0 || rotation == 180) ? width : height;
	if (page_height)
		*page_height = (rotation == 0 || rotation == 180) ? height : width;
}

static void
ev_view_get_page_size (EvView *view,
		       gint    page,
		       gint   *page_width,
		       gint   *page_height)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	_get_page_size_for_scale_and_rotation (priv->document,
					       page,
					       priv->scale,
					       priv->rotation,
					       page_width,
					       page_height);
}

static void
ev_view_get_max_page_size (EvView *view,
			   gint   *max_width,
			   gint   *max_height)
{
	double w, h;
	gint   width, height;
	EvViewPrivate *priv = GET_PRIVATE (view);

	ev_document_get_max_page_size (priv->document, &w, &h);

	width = (gint)(w * priv->scale + 0.5);
	height = (gint)(h * priv->scale + 0.5);

	if (max_width)
		*max_width = (priv->rotation == 0 || priv->rotation == 180) ? width : height;
	if (max_height)
		*max_height = (priv->rotation == 0 || priv->rotation == 180) ? height : width;
}

static void
get_page_y_offset (EvView *view, int page, int *y_offset, GtkBorder *border)
{
	int offset = 0;
	gboolean odd_left;
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_return_if_fail (y_offset != NULL);

	if (is_dual_page (view, &odd_left)) {
		ev_view_get_height_to_page (view, page, NULL, &offset);
		offset += ((page + !odd_left) / 2 + 1) * priv->spacing +
			((page + !odd_left) / 2 ) * (border->top + border->bottom);
	} else {
		ev_view_get_height_to_page (view, page, &offset, NULL);
		offset += (page + 1) * priv->spacing + page * (border->top + border->bottom);
	}

	*y_offset = offset;
	return;
}

gboolean
ev_view_get_page_extents_for_border (EvView       *view,
				     gint          page,
				     GtkBorder    *border,
				     GdkRectangle *page_area)
{
	return real_ev_view_get_page_extents (view, page, page_area, border, TRUE);
}

gboolean
ev_view_get_page_extents (EvView       *view,
			  gint          page,
			  GdkRectangle *page_area,
			  GtkBorder    *border)
{
	return real_ev_view_get_page_extents (view, page, page_area, border, FALSE);
}

static gboolean
real_ev_view_get_page_extents (EvView       *view,
			       gint          page,
			       GdkRectangle *page_area,
			       GtkBorder    *border,
			       gboolean      use_passed_border)
{
	GtkWidget *widget = GTK_WIDGET (view);
	EvViewPrivate *priv = GET_PRIVATE (view);
	int width, height, widget_width, widget_height;

	widget_width = gtk_widget_get_width (widget);
	widget_height = gtk_widget_get_height (widget);

	/* Get the size of the page */
	ev_view_get_page_size (view, page, &width, &height);
	if (!use_passed_border)
		compute_border (view, border);
	page_area->width = width + border->left + border->right;
	page_area->height = height + border->top + border->bottom;

	if (priv->continuous) {
		gint max_width;
		gint x, y;
		gboolean odd_left;

		ev_view_get_max_page_size (view, &max_width, NULL);
		max_width = max_width + border->left + border->right;
		/* Get the location of the bounding box */
		if (is_dual_page (view, &odd_left)) {
			gboolean right_page;

			right_page = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR && page % 2 == !odd_left) ||
			             (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL && page % 2 == odd_left);

			x = priv->spacing + (right_page ? 0 : 1) * (max_width + priv->spacing);
			x = x + MAX (0, widget_width - (max_width * 2 + priv->spacing * 3)) / 2;
			if (right_page)
				x = x + (max_width - width - border->left - border->right);
		} else {
			x = priv->spacing;
			x = x + MAX (0, widget_width - (width + border->left + border->right + priv->spacing * 2)) / 2;
		}

		get_page_y_offset (view, page, &y, border);

		page_area->x = x;
		page_area->y = y;
	} else {
		gint x, y;
		gboolean odd_left;

		if (is_dual_page (view, &odd_left)) {
			gint width_2, height_2;
			gint max_width = width;
			gint max_height = height;
			GtkBorder overall_border;
			gint other_page;

			other_page = (page % 2 == !odd_left) ? page + 1: page - 1;

			/* First, we get the bounding box of the two pages */
			if (other_page < ev_document_get_n_pages (priv->document)
			    && (0 <= other_page)) {
				ev_view_get_page_size (view, other_page,
						       &width_2, &height_2);
				if (width_2 > width)
					max_width = width_2;
				if (height_2 > height)
					max_height = height_2;
			}
			if (!use_passed_border)
				compute_border (view, &overall_border);
			else
				overall_border = *border;

			/* Find the offsets */
			x = priv->spacing;
			y = priv->spacing;

			/* Adjust for being the left or right page */
			if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR && page % 2 == !odd_left) ||
			    (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL && page % 2 == odd_left))
				x = x + max_width - width;
			else
				x = x + (max_width + overall_border.left + overall_border.right) + priv->spacing;

			y = y + (max_height - height)/2;

			/* Adjust for extra allocation */
			x = x + MAX (0, widget_width -
				     ((max_width + overall_border.left + overall_border.right) * 2 + priv->spacing * 3))/2;
			y = y + MAX (0, widget_height - (height + priv->spacing * 2))/2;
		} else {
			x = priv->spacing;
			y = priv->spacing;

			/* Adjust for extra allocation */
			x = x + MAX (0, widget_width - (width + border->left + border->right + priv->spacing * 2))/2;
			y = y + MAX (0, widget_height - (height + border->top + border->bottom +  priv->spacing * 2))/2;
		}

		page_area->x = x;
		page_area->y = y;
	}

	return TRUE;
}

static void
get_doc_page_size (EvView  *view,
		   gint     page,
		   gdouble *width,
		   gdouble *height)
{
	double w, h;
	EvViewPrivate *priv = GET_PRIVATE (view);

	ev_document_get_page_size (priv->document, page, &w, &h);
	if (priv->rotation == 0 || priv->rotation == 180) {
		if (width) *width = w;
		if (height) *height = h;
	} else {
		if (width) *width = h;
		if (height) *height = w;
	}
}

void
_ev_view_transform_view_point_to_doc_point (EvView       *view,
					    GdkPoint     *view_point,
					    GdkRectangle *page_area,
					    GtkBorder    *border,
					    double       *doc_point_x,
					    double       *doc_point_y)
{
	double x, y, width, height, doc_x, doc_y;
	EvViewPrivate *priv = GET_PRIVATE (view);

	x = doc_x = MAX ((double) (view_point->x - page_area->x - border->left) / priv->scale, 0);
	y = doc_y = MAX ((double) (view_point->y - page_area->y - border->top) / priv->scale, 0);

	ev_document_get_page_size (priv->document, priv->current_page, &width, &height);

	switch (priv->rotation) {
	case 0:
		x = doc_x;
		y = doc_y;
		break;
	case 90:
		x = doc_y;
		y = height - doc_x;
		break;
	case 180:
		x = width - doc_x;
		y = height - doc_y;
		break;
	case 270:
		x = width - doc_y;
		y = doc_x;
		break;
	default:
		g_assert_not_reached ();
	}

	*doc_point_x = x;
	*doc_point_y = y;
}

void
_ev_view_transform_view_rect_to_doc_rect (EvView       *view,
					  GdkRectangle *view_rect,
					  GdkRectangle *page_area,
					  GtkBorder    *border,
					  EvRectangle  *doc_rect)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	doc_rect->x1 = MAX ((double) (view_rect->x - page_area->x - border->left) / priv->scale, 0);
	doc_rect->y1 = MAX ((double) (view_rect->y - page_area->y - border->top) / priv->scale, 0);
	doc_rect->x2 = doc_rect->x1 + (double) view_rect->width / priv->scale;
	doc_rect->y2 = doc_rect->y1 + (double) view_rect->height / priv->scale;
}

void
_ev_view_transform_doc_point_by_rotation_scale (EvView   *view,
					    int       page,
					    EvPoint  *doc_point,
					    GdkPoint *view_point)
{
	GdkRectangle page_area;
	GtkBorder border;
	double x, y, view_x, view_y;
	EvViewPrivate *priv = GET_PRIVATE (view);

	switch (priv->rotation) {
	case 0:
		x = doc_point->x;
		y = doc_point->y;

		break;
	case 90: {
		gdouble width;

		get_doc_page_size (view, page, &width, NULL);
		x = width - doc_point->y;
		y = doc_point->x;
	}
		break;
	case 180: {
		gdouble width, height;

		get_doc_page_size (view, page, &width, &height);
		x = width - doc_point->x;
		y = height - doc_point->y;
	}
		break;
	case 270: {
		gdouble height;

		get_doc_page_size (view, page, NULL, &height);
		x = doc_point->y;
		y = height - doc_point->x;
	}
		break;
	default:
		g_assert_not_reached ();
	}

	ev_view_get_page_extents (view, page, &page_area, &border);

	view_x = CLAMP ((gint)(x * priv->scale + 0.5), 0, page_area.width);
	view_y = CLAMP ((gint)(y * priv->scale + 0.5), 0, page_area.height);

	view_point->x = view_x;
	view_point->y = view_y;
}

void
_ev_view_transform_doc_point_to_view_point (EvView   *view,
					    int       page,
					    EvPoint  *doc_point,
					    GdkPoint *view_point)
{
	GdkRectangle page_area;
	GtkBorder border;
	_ev_view_transform_doc_point_by_rotation_scale (view, page, doc_point, view_point);

	ev_view_get_page_extents (view, page, &page_area, &border);

	view_point->x = view_point->x + page_area.x + border.left;
	view_point->y = view_point->y + page_area.y + border.top;
}

void
_ev_view_transform_doc_rect_to_view_rect (EvView       *view,
					  int           page,
					  EvRectangle  *doc_rect,
					  GdkRectangle *view_rect)
{
	GdkRectangle page_area;
	GtkBorder border;
	double x, y, w, h;
	EvViewPrivate *priv = GET_PRIVATE (view);

	switch (priv->rotation) {
	case 0:
		x = doc_rect->x1;
		y = doc_rect->y1;
		w = doc_rect->x2 - doc_rect->x1;
		h = doc_rect->y2 - doc_rect->y1;

		break;
	case 90: {
		gdouble width;

		get_doc_page_size (view, page, &width, NULL);
		x = width - doc_rect->y2;
		y = doc_rect->x1;
		w = doc_rect->y2 - doc_rect->y1;
		h = doc_rect->x2 - doc_rect->x1;
	}
		break;
	case 180: {
		gdouble width, height;

		get_doc_page_size (view, page, &width, &height);
		x = width - doc_rect->x2;
		y = height - doc_rect->y2;
		w = doc_rect->x2 - doc_rect->x1;
		h = doc_rect->y2 - doc_rect->y1;
	}
		break;
	case 270: {
		gdouble height;

		get_doc_page_size (view, page, NULL, &height);
		x = doc_rect->y1;
		y = height - doc_rect->x2;
		w = doc_rect->y2 - doc_rect->y1;
		h = doc_rect->x2 - doc_rect->x1;
	}
		break;
	default:
		g_assert_not_reached ();
	}

	ev_view_get_page_extents (view, page, &page_area, &border);

	view_rect->x = (gint)(x * priv->scale + 0.5) + page_area.x + border.left;
	view_rect->y = (gint)(y * priv->scale + 0.5) + page_area.y + border.top;
	view_rect->width = (gint)(w * priv->scale + 0.5);
	view_rect->height = (gint)(h * priv->scale + 0.5);
}

static void
find_page_at_location (EvView  *view,
		       gdouble  x,
		       gdouble  y,
		       gint    *page,
		       gint    *x_offset,
		       gint    *y_offset)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	int i;
	GtkBorder border;

	if (priv->document == NULL)
		return;

	g_assert (page);
	g_assert (x_offset);
	g_assert (y_offset);

	compute_border (view, &border);
	for (i = priv->start_page; i >= 0 && i <= priv->end_page; i++) {
		GdkRectangle page_area;

		if (! ev_view_get_page_extents_for_border (view, i, &border, &page_area))
			continue;

		if ((x >= page_area.x + border.left) &&
		    (x < page_area.x + page_area.width - border.right) &&
		    (y >= page_area.y + border.top) &&
		    (y < page_area.y + page_area.height - border.bottom)) {
			*page = i;
			*x_offset = x - (page_area.x + border.left);
			*y_offset = y - (page_area.y + border.top);
			return;
		}
	}

	*page = -1;
}

static gboolean
location_in_text (EvView  *view,
		  gdouble  x,
		  gdouble  y)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	cairo_region_t *region;
	gint page = -1;
	gint x_offset = 0, y_offset = 0;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	if (page == -1)
		return FALSE;

	region = ev_page_cache_get_text_mapping (priv->page_cache, page);

	if (region)
		return cairo_region_contains_point (region, x_offset / priv->scale, y_offset / priv->scale);
	else
		return FALSE;
}

static gboolean
location_in_selected_text (EvView  *view,
			   gdouble  x,
			   gdouble  y)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	cairo_region_t *region;
	gint page = -1;
	gint x_offset = 0, y_offset = 0;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	if (page == -1)
		return FALSE;

	region = ev_pixbuf_cache_get_selection_region (priv->pixbuf_cache, page, priv->scale);

	if (region)
		return cairo_region_contains_point (region, x_offset, y_offset);
	else
		return FALSE;
}

static gboolean
get_doc_point_from_offset (EvView *view,
			   gint    page,
			   gint    x_offset,
			   gint    y_offset,
			   gint   *x_new,
			   gint   *y_new)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
        gdouble width, height;
	double x, y;

	get_doc_page_size (view, page, &width, &height);

	x_offset = x_offset / priv->scale;
	y_offset = y_offset / priv->scale;

        if (priv->rotation == 0) {
                x = x_offset;
                y = y_offset;
        } else if (priv->rotation == 90) {
                x = y_offset;
                y = width - x_offset;
        } else if (priv->rotation == 180) {
                x = width - x_offset;
                y = height - y_offset;
        } else if (priv->rotation == 270) {
                x = height - y_offset;
                y = x_offset;
        } else {
                g_assert_not_reached ();
        }

	*x_new = x;
	*y_new = y;

	return TRUE;
}

static gboolean
get_doc_point_from_location (EvView  *view,
			     gdouble  x,
			     gdouble  y,
			     gint    *page,
			     gint    *x_new,
			     gint    *y_new)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint x_offset = 0, y_offset = 0;

	x += priv->scroll_x;
	y += priv->scroll_y;
	find_page_at_location (view, x, y, page, &x_offset, &y_offset);
	if (*page == -1)
		return FALSE;

	return get_doc_point_from_offset (view, *page, x_offset, y_offset, x_new, y_new);
}

static void
ev_view_get_area_from_mapping (EvView        *view,
			       guint          page,
			       EvMappingList *mapping_list,
			       gconstpointer  data,
			       GdkRectangle  *area)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	EvMapping *mapping;

	mapping = ev_mapping_list_find (mapping_list, data);
	_ev_view_transform_doc_rect_to_view_rect (view, page, &mapping->area, area);
	area->x -= priv->scroll_x;
	area->y -= priv->scroll_y;
}

static void
ev_child_free (EvViewChild *child)
{
	g_slice_free (EvViewChild, child);
}

static void
ev_view_put (EvView      *view,
	     GtkWidget   *child_widget,
	     gint         x,
	     gint         y,
	     guint        page,
	     EvRectangle *doc_rect)
{
	EvViewChild *child;

	child = g_slice_new (EvViewChild);

	child->x = x;
	child->y = y;
	child->page = page;
	child->doc_rect = *doc_rect;

	g_object_set_data_full (G_OBJECT (child_widget), "ev-child",
			child, (GDestroyNotify)ev_child_free);

	gtk_widget_set_parent (child_widget, GTK_WIDGET (view));
}

static void
ev_view_put_to_doc_rect (EvView      *view,
			 GtkWidget   *child_widget,
			 guint        page,
			 EvRectangle *doc_rect)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkRectangle area;

	_ev_view_transform_doc_rect_to_view_rect (view, page, doc_rect, &area);
	area.x -= priv->scroll_x;
	area.y -= priv->scroll_y;
	ev_view_put (view, child_widget, area.x, area.y, page, doc_rect);
}

/*** Hyperref ***/
static EvMapping *
get_link_mapping_at_location (EvView  *view,
			      gdouble  x,
			      gdouble  y,
			      gint    *page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint x_new = 0, y_new = 0;
	EvMappingList *link_mapping;

	if (!EV_IS_DOCUMENT_LINKS (priv->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, page, &x_new, &y_new))
		return NULL;

	link_mapping = ev_page_cache_get_link_mapping (priv->page_cache, *page);
	if (link_mapping)
		return ev_mapping_list_get (link_mapping, x_new, y_new);

	return NULL;
}

static EvLink *
ev_view_get_link_at_location (EvView  *view,
			      gdouble  x,
			      gdouble  y)
{
	EvMapping *mapping;
	gint page;

	mapping = get_link_mapping_at_location (view, x, y, &page);

	return mapping ? mapping->data : NULL;
}

static void
goto_fitr_dest (EvView *view, EvLinkDest *dest)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvPoint doc_point;
	gdouble left, top;
	gboolean change_left, change_top;
	int widget_width = gtk_widget_get_width (GTK_WIDGET (view));
	int widget_height = gtk_widget_get_height (GTK_WIDGET (view));

	left = ev_link_dest_get_left (dest, &change_left);
	top = ev_link_dest_get_top (dest, &change_top);

	if (priv->allow_links_change_zoom) {
		gdouble doc_width, doc_height;
		gdouble zoom;

		doc_width = ev_link_dest_get_right (dest) - left;
		doc_height = ev_link_dest_get_bottom (dest) - top;

		zoom = zoom_for_size_fit_page (doc_width,
					       doc_height,
					       widget_width,
					       widget_height);

		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
		ev_document_model_set_scale (priv->model, zoom);

		/* center the target box within the view */
		left -= (widget_width / zoom - doc_width) / 2;
		top -= (widget_height / zoom - doc_height) / 2;
	}

	doc_point.x = change_left ? left : 0;
	doc_point.y = change_top ? top : 0;
	priv->pending_point = doc_point;

	ev_view_change_page (view, ev_link_dest_get_page (dest));
}

static void
goto_fitv_dest (EvView *view, EvLinkDest *dest)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvPoint doc_point;
	gint page;
	double left;
	gboolean change_left;

	page = ev_link_dest_get_page (dest);

	left = ev_link_dest_get_left (dest, &change_left);
	doc_point.x = change_left ? left : 0;
	doc_point.y = 0;

	if (priv->allow_links_change_zoom) {
		gdouble doc_width, doc_height;
		double zoom;

		ev_document_get_page_size (priv->document, page, &doc_width, &doc_height);

		zoom = zoom_for_size_fit_height (doc_width - doc_point.x, doc_height,
						 gtk_widget_get_width (GTK_WIDGET (view)),
						 gtk_widget_get_height (GTK_WIDGET (view)));

		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
		ev_document_model_set_scale (priv->model, zoom);
	}

	priv->pending_point = doc_point;

	ev_view_change_page (view, page);
}

static void
goto_fith_dest (EvView *view, EvLinkDest *dest)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvPoint doc_point;
	gint page;
	gdouble top;
	gboolean change_top;

	page = ev_link_dest_get_page (dest);

	top = ev_link_dest_get_top (dest, &change_top);
	doc_point.x = 0;
	doc_point.y = change_top ? top : 0;

	if (priv->allow_links_change_zoom) {
		gdouble doc_width;
		gdouble zoom;

		ev_document_get_page_size (priv->document, page, &doc_width, NULL);

		zoom = zoom_for_size_fit_width (doc_width, top,
						gtk_widget_get_width (GTK_WIDGET (view)),
						gtk_widget_get_height (GTK_WIDGET (view)));

		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FIT_WIDTH);
		ev_document_model_set_scale (priv->model, zoom);
	}

	priv->pending_point = doc_point;

	ev_view_change_page (view, page);
}

static void
goto_fit_dest (EvView *view, EvLinkDest *dest)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	int page;

	page = ev_link_dest_get_page (dest);

	if (priv->allow_links_change_zoom) {
		double zoom;
		gdouble doc_width, doc_height;

		ev_document_get_page_size (priv->document, page, &doc_width, &doc_height);

		zoom = zoom_for_size_fit_page (doc_width, doc_height,
					       gtk_widget_get_width (GTK_WIDGET (view)),
					       gtk_widget_get_height (GTK_WIDGET (view)));

		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FIT_PAGE);
		ev_document_model_set_scale (priv->model, zoom);
	}

	ev_view_change_page (view, page);
}

static void
goto_xyz_dest (EvView *view, EvLinkDest *dest)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvPoint doc_point;
	gint page;
	gdouble zoom, left, top;
	gboolean change_zoom, change_left, change_top;

	zoom = ev_link_dest_get_zoom (dest, &change_zoom);
	page = ev_link_dest_get_page (dest);

	if (priv->allow_links_change_zoom && change_zoom && zoom > 1) {
		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
		ev_document_model_set_scale (priv->model, zoom);
	}

	left = ev_link_dest_get_left (dest, &change_left);
	top = ev_link_dest_get_top (dest, &change_top);

	doc_point.x = change_left ? left : 0;
	doc_point.y = change_top ? top : 0;
	priv->pending_point = doc_point;

	ev_view_change_page (view, page);
}

static void
goto_dest (EvView *view, EvLinkDest *dest)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvLinkDestType type;
	int page, n_pages, current_page;

	page = ev_link_dest_get_page (dest);
	n_pages = ev_document_get_n_pages (priv->document);

	if (page < 0 || page >= n_pages)
		return;

	current_page = priv->current_page;

	type = ev_link_dest_get_dest_type (dest);

	switch (type) {
		case EV_LINK_DEST_TYPE_PAGE:
			ev_document_model_set_page (priv->model, page);
			break;
		case EV_LINK_DEST_TYPE_FIT:
			goto_fit_dest (view, dest);
			break;
		case EV_LINK_DEST_TYPE_FITH:
			goto_fith_dest (view, dest);
			break;
		case EV_LINK_DEST_TYPE_FITV:
			goto_fitv_dest (view, dest);
			break;
		case EV_LINK_DEST_TYPE_FITR:
			goto_fitr_dest (view, dest);
			break;
		case EV_LINK_DEST_TYPE_XYZ:
			goto_xyz_dest (view, dest);
			break;
		case EV_LINK_DEST_TYPE_PAGE_LABEL:
			ev_document_model_set_page_by_label (priv->model, ev_link_dest_get_page_label (dest));
			break;
		default:
			g_assert_not_reached ();
 	}

	if (current_page != priv->current_page)
		ev_document_model_set_page (priv->model, priv->current_page);
}

static void
ev_view_goto_dest (EvView *view, EvLinkDest *dest)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvLinkDestType type;

	type = ev_link_dest_get_dest_type (dest);

	if (type == EV_LINK_DEST_TYPE_NAMED) {
		EvLinkDest  *dest2;
		const gchar *named_dest;

		named_dest = ev_link_dest_get_named_dest (dest);
		dest2 = ev_document_links_find_link_dest (EV_DOCUMENT_LINKS (priv->document),
							  named_dest);
		if (dest2) {
			goto_dest (view, dest2);
			g_object_unref (dest2);
		}

		return;
	}

	goto_dest (view, dest);
}

static void
ev_view_link_to_current_view (EvView *view, EvLink **backlink)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvLinkDest   *backlink_dest = NULL;
	EvLinkAction *backlink_action = NULL;

	gint          backlink_page = priv->start_page;
	gdouble       zoom = ev_document_model_get_scale (priv->model);

	GtkBorder     border;
	GdkRectangle  backlink_page_area;

	gboolean is_dual = ev_document_model_get_page_layout (priv->model) == EV_PAGE_LAYOUT_DUAL;
	gint x_offset;
	gint y_offset;

	ev_view_get_page_extents (view, backlink_page, &backlink_page_area, &border);
	x_offset = backlink_page_area.x;
	y_offset = backlink_page_area.y;

	if (!priv->continuous && is_dual && priv->scroll_x > backlink_page_area.width + border.left) {
		/* For dual-column, non-continuous mode, priv->start_page is always
		 * the page in the left-hand column, even if that page isn't visible.
		 * We adjust for that here when we know the page can't be visible due
		 * to horizontal scroll. */
		backlink_page = backlink_page + 1;

		/* get right-hand page extents (no need to recompute border) */
		ev_view_get_page_extents_for_border (view, backlink_page,
						     &border, &backlink_page_area);
		x_offset = backlink_page_area.x;
	}

	gdouble backlink_dest_x = (priv->scroll_x - x_offset - border.left) / priv->scale;
	gdouble backlink_dest_y = (priv->scroll_y - y_offset - border.top) / priv->scale;

	backlink_dest = ev_link_dest_new_xyz (backlink_page, backlink_dest_x,
					      backlink_dest_y, zoom, TRUE,
					      TRUE, TRUE);

	backlink_action = ev_link_action_new_dest (backlink_dest);
	g_object_unref (backlink_dest);

	*backlink = ev_link_new ("Backlink", backlink_action);
	g_object_unref (backlink_action);
}

void
ev_view_handle_link (EvView *view, EvLink *link)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvLinkAction    *action = NULL;
	EvLinkActionType type;

	action = ev_link_get_action (link);
	if (!action)
		return;

	type = ev_link_action_get_action_type (action);

	switch (type) {
	        case EV_LINK_ACTION_TYPE_GOTO_DEST: {
			/* Build a synthetic Link representing our current view into the
			 * document. */

			EvLinkDest *dest;
			EvLink     *backlink = NULL;

			ev_view_link_to_current_view (view, &backlink);

			g_signal_emit (view, signals[SIGNAL_HANDLE_LINK], 0, link, backlink);

			dest = ev_link_action_get_dest (action);
			ev_view_goto_dest (view, dest);
		}
			break;
	        case EV_LINK_ACTION_TYPE_LAYERS_STATE: {
			GList            *show, *hide, *toggle;
			GList            *l;
			EvDocumentLayers *document_layers;

			document_layers = EV_DOCUMENT_LAYERS (priv->document);

			show = ev_link_action_get_show_list (action);
			for (l = show; l; l = g_list_next (l)) {
				ev_document_layers_show_layer (document_layers, EV_LAYER (l->data));
			}

			hide = ev_link_action_get_hide_list (action);
			for (l = hide; l; l = g_list_next (l)) {
				ev_document_layers_hide_layer (document_layers, EV_LAYER (l->data));
			}

			toggle = ev_link_action_get_toggle_list (action);
			for (l = toggle; l; l = g_list_next (l)) {
				EvLayer *layer = EV_LAYER (l->data);

				if (ev_document_layers_layer_is_visible (document_layers, layer)) {
					ev_document_layers_hide_layer (document_layers, layer);
				} else {
					ev_document_layers_show_layer (document_layers, layer);
				}
			}

			g_signal_emit (view, signals[SIGNAL_LAYERS_CHANGED], 0);
			ev_view_reload (view);
		}
			break;
	        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
	        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
	        case EV_LINK_ACTION_TYPE_LAUNCH:
	        case EV_LINK_ACTION_TYPE_NAMED:
	        case EV_LINK_ACTION_TYPE_RESET_FORM:
			g_signal_emit (view, signals[SIGNAL_EXTERNAL_LINK], 0, action);
			break;
	}
}

static char *
tip_from_action_named (EvLinkAction *action)
{
	const gchar *name = ev_link_action_get_name (action);

	if (g_ascii_strcasecmp (name, "FirstPage") == 0) {
		return g_strdup (_("Go to first page"));
	} else if (g_ascii_strcasecmp (name, "PrevPage") == 0) {
		return g_strdup (_("Go to previous page"));
	} else if (g_ascii_strcasecmp (name, "NextPage") == 0) {
		return g_strdup (_("Go to next page"));
	} else if (g_ascii_strcasecmp (name, "LastPage") == 0) {
		return g_strdup (_("Go to last page"));
	} else if (g_ascii_strcasecmp (name, "GoToPage") == 0) {
		return g_strdup (_("Go to page"));
	} else if (g_ascii_strcasecmp (name, "Find") == 0) {
		return g_strdup (_("Find"));
	}

	return NULL;
}

static char *
tip_from_link (EvView *view, EvLink *link)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvLinkAction *action;
	EvLinkActionType type;
	char *msg = NULL;
	char *page_label;
	const char *title;

	action = ev_link_get_action (link);
	title = ev_link_get_title (link);

	if (!action)
		return title ? g_strdup (title) : NULL;

	type = ev_link_action_get_action_type (action);

	switch (type) {
	        case EV_LINK_ACTION_TYPE_GOTO_DEST:
			page_label = ev_document_links_get_dest_page_label (EV_DOCUMENT_LINKS (priv->document),
									    ev_link_action_get_dest (action));
			if (page_label) {
    				msg = g_strdup_printf (_("Go to page %s"), page_label);
				g_free (page_label);
			}
			break;
	        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
			if (title) {
				msg = g_strdup_printf (_("Go to %s on file “%s”"), title,
						       ev_link_action_get_filename (action));
			} else {
				msg = g_strdup_printf (_("Go to file “%s”"),
						       ev_link_action_get_filename (action));
			}
			break;
	        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
			msg = g_strdup (ev_link_action_get_uri (action));
			break;
	        case EV_LINK_ACTION_TYPE_LAUNCH:
			msg = g_strdup_printf (_("Launch %s"),
					       ev_link_action_get_filename (action));
			break;
	        case EV_LINK_ACTION_TYPE_NAMED:
			msg = tip_from_action_named (action);
			break;
	        case EV_LINK_ACTION_TYPE_RESET_FORM:
			msg = g_strdup_printf (_("Reset form"));
			break;
	        default:
			if (title)
				msg = g_strdup (title);
			break;
	}

	return msg;
}

static gboolean
link_preview_popover_motion_notify (GtkEventControllerMotion	*self,
				    gdouble			 x,
				    gdouble			 y,
				    EvView			*view)
{
	ev_view_link_preview_popover_cleanup (view);
	return TRUE;
}

static void
handle_cursor_over_link (EvView *view, EvLink *link, gint x, gint y, gboolean from_motion)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkRectangle     link_area;
	EvLinkAction    *action;
	EvLinkDest      *dest;
	EvLinkDestType   type;
	GtkWidget       *popover, *spinner;
	GdkTexture      *page_texture = NULL;
	guint            link_dest_page;
	EvPoint          link_dest_doc;
	GdkPoint         link_dest_view;
	gint             device_scale = 1;
	GtkEventController * controller;

	ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);

	if (link == priv->link_preview.link)
		return;

	/* Display thumbnail, if applicable */
	action = ev_link_get_action (link);
	if (!action)
		return;

	dest = ev_link_action_get_dest (action);
	if (!dest)
		return;

	/* Show preview popups only for motion events - Issue #1666 */
	if (!from_motion)
		return;

	type = ev_link_dest_get_dest_type (dest);
	if (type == EV_LINK_DEST_TYPE_NAMED) {
		dest = ev_document_links_find_link_dest (EV_DOCUMENT_LINKS (priv->document),
							 ev_link_dest_get_named_dest (dest));
	}

	ev_view_link_preview_popover_cleanup (view);

	/* Init popover */
	priv->link_preview.popover = popover = gtk_popover_new ();
	gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_TOP);
	gtk_widget_set_parent (popover, GTK_WIDGET (view));
	get_link_area (view, x, y, link, &link_area);
	gtk_popover_set_pointing_to (GTK_POPOVER (popover), &link_area);

	controller = GTK_EVENT_CONTROLLER (gtk_event_controller_motion_new ());
	g_signal_connect (controller, "motion",
				  G_CALLBACK (link_preview_popover_motion_notify),
				  view);
	gtk_widget_add_controller (popover, controller);

	spinner = gtk_spinner_new ();
	gtk_spinner_start (GTK_SPINNER (spinner));
	gtk_popover_set_child (GTK_POPOVER (popover) , spinner);

	/* Start thumbnailing job async */
	link_dest_page = ev_link_dest_get_page (dest);
	device_scale = gtk_widget_get_scale_factor (GTK_WIDGET (view));
	priv->link_preview.job = ev_job_thumbnail_cairo_new (priv->document,
							     link_dest_page,
							     priv->rotation,
							     priv->scale * device_scale);

	link_dest_doc.x = ev_link_dest_get_left (dest, NULL);
	link_dest_doc.y = ev_link_dest_get_top (dest, NULL);
	_ev_view_transform_doc_point_by_rotation_scale (view, link_dest_page,
							&link_dest_doc, &link_dest_view);
	priv->link_preview.left = link_dest_view.x;
	priv->link_preview.top = link_dest_view.y;
	priv->link_preview.link = link;

	page_texture = ev_pixbuf_cache_get_texture (priv->pixbuf_cache, link_dest_page);

	if (page_texture)
		link_preview_show_thumbnail (page_texture, view);
	else {
		g_signal_connect (priv->link_preview.job, "finished",
				  G_CALLBACK (link_preview_job_finished_cb),
				  view);
		ev_job_scheduler_push_job (priv->link_preview.job, EV_JOB_PRIORITY_LOW);
	}

	if (type == EV_LINK_DEST_TYPE_NAMED)
		g_object_unref (dest);

	priv->link_preview.delay_timeout_id =
		g_timeout_add_once (LINK_PREVIEW_DELAY_MS,
				    (GSourceOnceFunc)link_preview_delayed_show,
				    view);
	g_source_set_name_by_id (priv->link_preview.delay_timeout_id,
				 "[evince] link_preview_timeout");
}

static void
ev_view_handle_cursor_over_xy (EvView *view, gint x, gint y, gboolean from_motion)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvLink       *link;
	EvFormField  *field;
	EvAnnotation *annot = NULL;
	EvMedia      *media;

	if (priv->cursor == EV_VIEW_CURSOR_HIDDEN)
		return;

	if (priv->adding_annot_info.adding_annot) {
		if (priv->adding_annot_info.type == EV_ANNOTATION_TYPE_TEXT_MARKUP) {
			ev_view_set_cursor (view, EV_VIEW_CURSOR_IBEAM);
		} else if (!priv->adding_annot_info.annot) {
			ev_view_set_cursor (view, EV_VIEW_CURSOR_ADD);
		}
		return;
	}

	if (priv->drag_info.in_drag) {
		if (priv->cursor != EV_VIEW_CURSOR_DRAG)
			ev_view_set_cursor (view, EV_VIEW_CURSOR_DRAG);
		return;
	}

	if (priv->scroll_info.autoscrolling) {
		if (priv->cursor != EV_VIEW_CURSOR_AUTOSCROLL)
			ev_view_set_cursor (view, EV_VIEW_CURSOR_AUTOSCROLL);
		return;
	}

	link = ev_view_get_link_at_location (view, x, y);
	if (link) {
		handle_cursor_over_link (view, link, x, y, from_motion);
	} else {
		ev_view_link_preview_popover_cleanup (view);
		priv->link_preview.link = NULL;

		if ((field = ev_view_get_form_field_at_location (view, x, y))) {
			if (field->is_read_only) {
				if (priv->cursor == EV_VIEW_CURSOR_LINK ||
				    priv->cursor == EV_VIEW_CURSOR_IBEAM ||
				    priv->cursor == EV_VIEW_CURSOR_DRAG)
					ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
			} else if (EV_IS_FORM_FIELD_TEXT (field)) {
				ev_view_set_cursor (view, EV_VIEW_CURSOR_IBEAM);
			} else {
				ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);
			}
		} else if ((media = ev_view_get_media_at_location (view, x, y))) {
			if (!ev_view_find_player_for_media (view, media))
				ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);
			else
				ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
		} else if ((annot = ev_view_get_annotation_at_location (view, x, y))) {
			ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);
		} else if (location_in_text (view, x + priv->scroll_x, y + priv->scroll_y)) {
			ev_view_set_cursor (view, EV_VIEW_CURSOR_IBEAM);
		} else {
			if (priv->cursor == EV_VIEW_CURSOR_LINK ||
			    priv->cursor == EV_VIEW_CURSOR_IBEAM ||
			    priv->cursor == EV_VIEW_CURSOR_DRAG ||
			    priv->cursor == EV_VIEW_CURSOR_AUTOSCROLL ||
			    priv->cursor == EV_VIEW_CURSOR_ADD)
				ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
		}
	}
}

/*** Images ***/
static EvImage *
ev_view_get_image_at_location (EvView  *view,
			       gdouble  x,
			       gdouble  y)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint page = -1;
	gint x_new = 0, y_new = 0;
	EvMappingList *image_mapping;

	if (!EV_IS_DOCUMENT_IMAGES (priv->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, &page, &x_new, &y_new))
		return NULL;

	image_mapping = ev_page_cache_get_image_mapping (priv->page_cache, page);

	if (image_mapping)
		return ev_mapping_list_get_data (image_mapping, x_new, y_new);
	else
		return NULL;
}

/*** Focus ***/
static gboolean
ev_view_get_focused_area (EvView       *view,
			  GdkRectangle *area)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->focused_element)
		return FALSE;

	_ev_view_transform_doc_rect_to_view_rect (view,
						  priv->focused_element_page,
						  &priv->focused_element->area,
						  area);
	area->x -= priv->scroll_x + 1;
	area->y -= priv->scroll_y + 1;
	area->width += 1;
	area->height += 1;

	return TRUE;
}

void
_ev_view_set_focused_element (EvView *view,
			     EvMapping *element_mapping,
			     gint page)
{
	GdkRectangle    view_rect;
	cairo_region_t *region = NULL;
	EvViewPrivate *priv = GET_PRIVATE (view);

#if 0
	if (priv->accessible)
		ev_view_accessible_set_focused_element (EV_VIEW_ACCESSIBLE (priv->accessible), element_mapping, page);
#endif

	if (ev_view_get_focused_area (view, &view_rect))
		region = cairo_region_create_rectangle (&view_rect);

	priv->focused_element = element_mapping;
	priv->focused_element_page = page;

	if (ev_view_get_focused_area (view, &view_rect)) {
		if (!region)
			region = cairo_region_create_rectangle (&view_rect);
		else
			cairo_region_union_rectangle (region, &view_rect);

		ev_document_model_set_page (priv->model, page);
		view_rect.x += priv->scroll_x;
		view_rect.y += priv->scroll_y;
		_ev_view_ensure_rectangle_is_visible (view, &view_rect);
	}

	g_clear_pointer (&region, cairo_region_destroy);
}

/*** Forms ***/
static EvMapping *
get_form_field_mapping_at_location (EvView  *view,
				    gdouble  x,
				    gdouble  y,
				    gint    *page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint x_new = 0, y_new = 0;
	EvMappingList *forms_mapping;

	if (!EV_IS_DOCUMENT_FORMS (priv->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, page, &x_new, &y_new))
		return NULL;

	forms_mapping = ev_page_cache_get_form_field_mapping (priv->page_cache, *page);

	if (forms_mapping)
		return ev_mapping_list_get (forms_mapping, x_new, y_new);

	return NULL;
}

static EvFormField *
ev_view_get_form_field_at_location (EvView  *view,
				    gdouble  x,
				    gdouble  y)
{
	EvMapping *field_mapping;
	gint page;

	field_mapping = get_form_field_mapping_at_location (view, x, y, &page);

	return field_mapping ? field_mapping->data : NULL;
}

static cairo_region_t *
ev_view_form_field_get_region (EvView      *view,
			       EvFormField *field)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkRectangle   view_area;
	EvMappingList *forms_mapping;

	forms_mapping = ev_page_cache_get_form_field_mapping (priv->page_cache,
							      field->page->index);
	ev_view_get_area_from_mapping (view, field->page->index,
				       forms_mapping,
				       field, &view_area);

	return cairo_region_create_rectangle (&view_area);
}

static void
ev_view_form_field_destroy (GtkWidget *widget,
			    EvView    *view)
{
	g_idle_add_once ((GSourceOnceFunc)ev_view_remove_all_form_fields, view);
}

static void
ev_view_form_field_button_toggle (EvView      *view,
				  EvFormField *field)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvMappingList     *forms_mapping;
	cairo_region_t    *region;
	gboolean           state;
	GList             *l;
	EvFormFieldButton *field_button = EV_FORM_FIELD_BUTTON (field);

	if (field_button->type == EV_FORM_FIELD_BUTTON_PUSH)
		return;

	state = ev_document_forms_form_field_button_get_state (EV_DOCUMENT_FORMS (priv->document),
							       field);

	/* FIXME: it actually depends on NoToggleToOff flags */
	if (field_button->type == EV_FORM_FIELD_BUTTON_RADIO && state && field_button->state)
		return;

	region = ev_view_form_field_get_region (view, field);

	/* For radio buttons and checkbox buttons that are in a set
	 * we need to update also the region for the current selected item
	 */
	forms_mapping = ev_page_cache_get_form_field_mapping (priv->page_cache,
							      field->page->index);

	for (l = ev_mapping_list_get_list (forms_mapping); l; l = g_list_next (l)) {
		EvFormField *button = ((EvMapping *)(l->data))->data;
		cairo_region_t *button_region;

		if (button->id == field->id)
			continue;

		/* FIXME: only buttons in the same group should be updated */
		if (!EV_IS_FORM_FIELD_BUTTON (button) ||
		    EV_FORM_FIELD_BUTTON (button)->type != field_button->type ||
		    EV_FORM_FIELD_BUTTON (button)->state != TRUE)
			continue;

		button_region = ev_view_form_field_get_region (view, button);
		cairo_region_union (region, button_region);
		cairo_region_destroy (button_region);
	}

	/* Update state */
	ev_document_forms_form_field_button_set_state (EV_DOCUMENT_FORMS (priv->document),
						       field,
						       !state);
	field_button->state = !state;

#if 0
	if (priv->accessible)
		ev_view_accessible_update_element_state (EV_VIEW_ACCESSIBLE (priv->accessible),
							 ev_mapping_list_find (forms_mapping, field),
							 field->page->index);
#endif

	ev_view_reload_page (view, field->page->index, region);
	cairo_region_destroy (region);
}

static GtkWidget *
ev_view_form_field_button_create_widget (EvView      *view,
					 EvFormField *field)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvMappingList *form_mapping;
	EvMapping     *mapping;

	/* We need to do this focus grab prior to setting the focused element for accessibility */
	if (!gtk_widget_has_focus (GTK_WIDGET (view)))
		gtk_widget_grab_focus (GTK_WIDGET (view));

	form_mapping = ev_page_cache_get_form_field_mapping (priv->page_cache,
							     field->page->index);
	mapping = ev_mapping_list_find (form_mapping, field);
	_ev_view_set_focused_element (view, mapping, field->page->index);

	return NULL;
}

static void
ev_view_form_field_text_save (EvView    *view,
			      GtkWidget *widget)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvFormField *field;

	if (!priv->document)
		return;

	field = g_object_get_data (G_OBJECT (widget), "form-field");

	if (field->changed) {
		EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (field);
		cairo_region_t  *field_region;

		field_region = ev_view_form_field_get_region (view, field);

		ev_document_forms_form_field_text_set_text (EV_DOCUMENT_FORMS (priv->document),
							    field, field_text->text);
		field->changed = FALSE;
		ev_view_reload_page (view, field->page->index, field_region);
		cairo_region_destroy (field_region);
	}
}

static void
ev_view_form_field_text_changed (GtkWidget   *widget,
				 EvFormField *field)
{
	EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (field);
	gchar           *text = NULL;

	if (GTK_IS_ENTRY (widget)) {
		text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (widget)));
	} else if (GTK_IS_TEXT_BUFFER (widget)) {
		GtkTextIter start, end;

		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (widget), &start, &end);
		text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (widget),
						 &start, &end, FALSE);
	}

	if (!field_text->text ||
	    (field_text->text && g_ascii_strcasecmp (field_text->text, text) != 0)) {
		g_free (field_text->text);
		field_text->text = text;
		field->changed = TRUE;
	}
}

static void
ev_view_form_field_text_focus_out (GtkEventControllerFocus	*self,
				   EvView			*view)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	ev_view_form_field_text_save (view, widget);
}

static void
ev_view_form_field_text_button_pressed (GtkGestureClick	*self,
					gint n_press,
					gdouble x,
					gdouble y,
					gpointer user_data)
{
	gtk_gesture_set_state (GTK_GESTURE (self), GTK_EVENT_SEQUENCE_CLAIMED);
}

static GtkWidget *
ev_view_form_field_text_create_widget (EvView      *view,
				       EvFormField *field)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (field);
	GtkWidget       *text = NULL;
	gchar           *txt;
	GtkStyleContext *context;
	GtkEventController *controller;

	txt = ev_document_forms_form_field_text_get_text (EV_DOCUMENT_FORMS (priv->document),
							  field);

	switch (field_text->type) {
	        case EV_FORM_FIELD_TEXT_FILE_SELECT:
			/* TODO */
			return NULL;
	        case EV_FORM_FIELD_TEXT_NORMAL:
			text = gtk_entry_new ();
			gtk_entry_set_has_frame (GTK_ENTRY (text), FALSE);
			/* Remove '.flat' style added by previous call
			 * gtk_entry_set_has_frame(FALSE) which caused bug #687 */
			context = gtk_widget_get_style_context (text);
			gtk_style_context_remove_class (context, "flat");
			gtk_entry_set_max_length (GTK_ENTRY (text), field_text->max_len);
			gtk_entry_set_visibility (GTK_ENTRY (text), !field_text->is_password);

			g_signal_connect_after (text, "activate",
						G_CALLBACK (ev_view_form_field_destroy),
						view);
			break;
	        case EV_FORM_FIELD_TEXT_MULTILINE: {
			text = gtk_text_view_new ();
		}
			break;
	}

	if (txt) {
		gtk_editable_set_text (GTK_EDITABLE (text), txt);
		g_free (txt);
	}

	g_signal_connect (text, "changed",
				G_CALLBACK (ev_view_form_field_text_changed),
				field);

	controller = GTK_EVENT_CONTROLLER (gtk_event_controller_focus_new ());
	g_signal_connect (controller, "leave",
				G_CALLBACK (ev_view_form_field_text_focus_out),
				view);
	gtk_widget_add_controller (text, controller);

	controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
	g_signal_connect (controller, "pressed",
				G_CALLBACK (ev_view_form_field_text_button_pressed), NULL);
	gtk_widget_add_controller (text, controller);

	g_object_weak_ref (G_OBJECT (text),
			   (GWeakNotify)ev_view_form_field_text_save,
			   view);

	return text;
}

static void
ev_view_form_field_choice_save (EvView    *view,
				GtkWidget *widget)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvFormField *field;

	if (!priv->document)
		return;

	field = g_object_get_data (G_OBJECT (widget), "form-field");

	if (field->changed) {
		GList             *l;
		EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (field);
		cairo_region_t    *field_region;

		field_region = ev_view_form_field_get_region (view, field);

		if (field_choice->is_editable) {
			ev_document_forms_form_field_choice_set_text (EV_DOCUMENT_FORMS (priv->document),
								      field, field_choice->text);
		} else {
			ev_document_forms_form_field_choice_unselect_all (EV_DOCUMENT_FORMS (priv->document), field);
			for (l = field_choice->selected_items; l; l = g_list_next (l)) {
				ev_document_forms_form_field_choice_select_item (EV_DOCUMENT_FORMS (priv->document),
										 field,
										 GPOINTER_TO_INT (l->data));
			}
		}
		field->changed = FALSE;
		ev_view_reload_page (view, field->page->index, field_region);
		cairo_region_destroy (field_region);
	}
}

static void
ev_view_form_field_choice_changed (GtkWidget   *widget,
				   EvFormField *field)
{
	EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (field);

	if (GTK_IS_COMBO_BOX (widget)) {
		gint item;

		item = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
		if (item != -1 && (!field_choice->selected_items ||
		    GPOINTER_TO_INT (field_choice->selected_items->data) != item)) {
			g_clear_pointer (&field_choice->selected_items, g_list_free);
			field_choice->selected_items = g_list_prepend (field_choice->selected_items,
								       GINT_TO_POINTER (item));
			field->changed = TRUE;
		}

		if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (widget))) {
			const gchar *text;

			text = gtk_editable_get_text (GTK_EDITABLE (gtk_combo_box_get_child (GTK_COMBO_BOX (widget))));
			if (!field_choice->text ||
			    (field_choice->text && g_ascii_strcasecmp (field_choice->text, text) != 0)) {
				g_free (field_choice->text);
				field_choice->text = g_strdup (text);
				field->changed = TRUE;
			}
		}
	} else if (GTK_IS_TREE_SELECTION (widget)) {
		GtkTreeSelection *selection = GTK_TREE_SELECTION (widget);
		GtkTreeModel     *model;
		GList            *items, *l;

		items = gtk_tree_selection_get_selected_rows (selection, &model);
		g_clear_pointer (&field_choice->selected_items, g_list_free);

		for (l = items; l && l->data; l = g_list_next (l)) {
			GtkTreeIter  iter;
			GtkTreePath *path = (GtkTreePath *)l->data;
			gint         item;

			gtk_tree_model_get_iter (model, &iter, path);
			gtk_tree_model_get (model, &iter, 1, &item, -1);

			field_choice->selected_items = g_list_prepend (field_choice->selected_items,
								       GINT_TO_POINTER (item));

			gtk_tree_path_free (path);
		}

		g_list_free (items);

		field->changed = TRUE;
	}
}

typedef struct _PopupShownData {
	GtkWidget   *choice;
	EvFormField *field;
	EvView      *view;
} PopupShownData;

static void
ev_view_form_field_choice_popup_shown_real (PopupShownData *data)
{
	ev_view_form_field_choice_changed (data->choice, data->field);
	ev_view_form_field_destroy (data->choice, data->view);

	g_object_unref (data->choice);
	g_object_unref (data->field);
	g_free (data);
}

static void
ev_view_form_field_choice_popup_shown_cb (GObject    *self,
					  GParamSpec *pspec,
					  EvView     *view)
{
	EvFormField *field;
	GtkWidget *choice;
	gboolean shown;
	PopupShownData *data;

	g_object_get (self, "popup-shown", &shown, NULL);
	if (shown)
		return; /* popup is already opened */

	/* Popup has been closed */
	field = g_object_get_data (self, "form-field");
	choice = GTK_WIDGET (self);

	data = g_new0 (PopupShownData, 1);
	data->choice = g_object_ref (choice);
	data->field = g_object_ref (field);
	data->view = view;
	/* We need to use an idle here because combobox "active" item is not updated yet */
	g_idle_add_once ((GSourceOnceFunc) ev_view_form_field_choice_popup_shown_real,
			 (gpointer) data);
}

static GtkWidget *
ev_view_form_field_choice_create_widget (EvView      *view,
					 EvFormField *field)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (field);
	GtkWidget         *choice;
	GtkTreeModel      *model;
	gint               n_items, i;
	gint               selected_item = -1;

	n_items = ev_document_forms_form_field_choice_get_n_items (EV_DOCUMENT_FORMS (priv->document),
								   field);
	model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT));
	for (i = 0; i < n_items; i++) {
		GtkTreeIter iter;
		gchar      *item;

		item = ev_document_forms_form_field_choice_get_item (EV_DOCUMENT_FORMS (priv->document),
								     field, i);
		if (ev_document_forms_form_field_choice_is_item_selected (
			    EV_DOCUMENT_FORMS (priv->document), field, i)) {
			selected_item = i;
			/* FIXME: we need a get_selected_items function in poppler */
			field_choice->selected_items = g_list_prepend (field_choice->selected_items,
								       GINT_TO_POINTER (i));
		}

		if (item) {
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    0, item,
					    1, i,
					    -1);
			g_free (item);
		}
	}

	if (field_choice->type == EV_FORM_FIELD_CHOICE_LIST) {
		GtkCellRenderer  *renderer;
		GtkWidget        *tree_view;
		GtkTreeSelection *selection;

		tree_view = gtk_tree_view_new_with_model (model);
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
		if (field_choice->multi_select) {
			gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
		}

		/* TODO: set selected items */

		renderer = gtk_cell_renderer_text_new ();
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
							     0,
							     "choix", renderer,
							     "text", 0,
							     NULL);

		choice = gtk_scrolled_window_new ();
		gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (choice), tree_view);

		g_signal_connect (selection, "changed",
				  G_CALLBACK (ev_view_form_field_choice_changed),
				  field);
		g_signal_connect_after (selection, "changed",
					G_CALLBACK (ev_view_form_field_destroy),
					view);
	} else if (field_choice->is_editable) { /* ComboBoxEntry */
		GtkEntry *combo_entry;
		gchar *text;

		choice = gtk_combo_box_new_with_model_and_entry (model);
		combo_entry = GTK_ENTRY (gtk_combo_box_get_child (GTK_COMBO_BOX (choice)));
		/* This sets GtkEntry's minimum-width to be 1 char long, short enough
		 * to workaround gtk issue gtk#1422 . Evince issue #1002 */
		gtk_editable_set_width_chars (GTK_EDITABLE (combo_entry), 1);
		gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (choice), 0);

		text = ev_document_forms_form_field_choice_get_text (EV_DOCUMENT_FORMS (priv->document), field);
		if (text) {
			gtk_editable_set_text (GTK_EDITABLE (combo_entry), text);
			g_free (text);
		}

		g_signal_connect (choice, "changed",
				  G_CALLBACK (ev_view_form_field_choice_changed),
				  field);
		g_signal_connect_after (gtk_combo_box_get_child (GTK_COMBO_BOX (choice)),
					"activate",
					G_CALLBACK (ev_view_form_field_destroy),
					view);
	} else { /* ComboBoxText */
		GtkCellRenderer *renderer;

		choice = gtk_combo_box_new_with_model (model);
		renderer = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (choice),
					    renderer, TRUE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (choice),
						renderer,
						"text", 0,
						NULL);
		gtk_combo_box_set_active (GTK_COMBO_BOX (choice), selected_item);
		gtk_combo_box_popup (GTK_COMBO_BOX (choice));

		/* See issue #294 for why we use this instead of "changed" signal */
		g_signal_connect (choice, "notify::popup-shown",
				  G_CALLBACK (ev_view_form_field_choice_popup_shown_cb),
				  view);
	}

	g_object_unref (model);

	g_object_weak_ref (G_OBJECT (choice),
			   (GWeakNotify)ev_view_form_field_choice_save,
			   view);

	return choice;
}

void
_ev_view_focus_form_field (EvView      *view,
			  EvFormField *field)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GtkWidget     *field_widget = NULL;
	EvMappingList *form_field_mapping;
	EvMapping     *mapping;

	_ev_view_set_focused_element (view, NULL, -1);

	if (field->is_read_only)
		return;

	if (EV_IS_FORM_FIELD_BUTTON (field)) {
		field_widget = ev_view_form_field_button_create_widget (view, field);
	} else if (EV_IS_FORM_FIELD_TEXT (field)) {
		field_widget = ev_view_form_field_text_create_widget (view, field);
	} else if (EV_IS_FORM_FIELD_CHOICE (field)) {
		field_widget = ev_view_form_field_choice_create_widget (view, field);
	} else if (EV_IS_FORM_FIELD_SIGNATURE (field)) {
		/* TODO */
	}

	/* Form field doesn't require a widget */
	if (!field_widget) {
		if (!gtk_widget_has_focus (GTK_WIDGET (view)))
			gtk_widget_grab_focus (GTK_WIDGET (view));
		return;
	}

	g_object_set_data_full (G_OBJECT (field_widget), "form-field",
				g_object_ref (field),
				(GDestroyNotify)g_object_unref);

	form_field_mapping = ev_page_cache_get_form_field_mapping (priv->page_cache,
								   field->page->index);
	mapping = ev_mapping_list_find (form_field_mapping, field);
	_ev_view_set_focused_element (view, mapping, field->page->index);
	ev_view_put_to_doc_rect (view, field_widget, field->page->index, &mapping->area);
	gtk_widget_set_visible (field_widget, TRUE);
	gtk_widget_grab_focus (field_widget);
}

static void
ev_view_handle_form_field (EvView      *view,
			   EvFormField *field)
{
	if (field->is_read_only)
		return;

	_ev_view_focus_form_field (view, field);

	if (field->activation_link)
		ev_view_handle_link (view, field->activation_link);

	if (EV_IS_FORM_FIELD_BUTTON (field))
		ev_view_form_field_button_toggle (view, field);

}

/* Media */
static EvMapping *
get_media_mapping_at_location (EvView *view,
			       gdouble x,
			       gdouble y,
			       gint *page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint x_new = 0, y_new = 0;
	EvMappingList *media_mapping;

	if (!EV_IS_DOCUMENT_MEDIA (priv->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, page, &x_new, &y_new))
		return NULL;

	media_mapping = ev_page_cache_get_media_mapping (priv->page_cache, *page);

	return media_mapping ? ev_mapping_list_get (media_mapping, x_new, y_new) : NULL;
}

static EvMedia *
ev_view_get_media_at_location (EvView  *view,
			       gdouble  x,
			       gdouble  y)
{
	EvMapping *media_mapping;
	gint       page;

	media_mapping = get_media_mapping_at_location (view, x, y, &page);

	return media_mapping ? media_mapping->data : NULL;
}

static gboolean
ev_view_find_player_for_media (EvView  *view,
			       EvMedia *media)
{
	for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (view));
		child != NULL;
		child = gtk_widget_get_next_sibling (child))
	{
		if (!GTK_IS_VIDEO (child))
			continue;

		if (g_object_get_data (G_OBJECT(child), "media") == media)
			return TRUE;
	}

	return FALSE;
}

static void
ev_view_handle_media (EvView  *view,
		      EvMedia *media)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GtkWidget     *player;
	EvMappingList *media_mapping;
	EvMapping     *mapping;
	GdkRectangle   render_area;
	guint          page;
	GFile	      *uri;

	page = ev_media_get_page_index (media);
	media_mapping = ev_page_cache_get_media_mapping (priv->page_cache, page);

	/* TODO: focus? */

	if (ev_view_find_player_for_media (view, media))
		return;

	uri = g_file_new_for_uri (ev_media_get_uri (media));
	player = gtk_video_new_for_file (uri);
	gtk_video_set_autoplay (GTK_VIDEO (player), TRUE);
	g_object_unref (uri);

	g_object_set_data_full (G_OBJECT (player), "media",
				g_object_ref (media),
				(GDestroyNotify)g_object_unref);

	mapping = ev_mapping_list_find (media_mapping, media);
	_ev_view_transform_doc_rect_to_view_rect (view, page, &mapping->area, &render_area);
	render_area.x -= priv->scroll_x;
	render_area.y -= priv->scroll_y;

	ev_view_put (view, player, render_area.x, render_area.y, page, &mapping->area);
}

/* Annotations */
static GtkWidget *
get_window_for_annot (EvView       *view,
		      EvAnnotation *annot)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (priv->annot_window_map == NULL)
		return NULL;

	return g_hash_table_lookup (priv->annot_window_map, annot);
}

static void
map_annot_to_window (EvView       *view,
                     EvAnnotation *annot,
		     GtkWidget    *window)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (priv->annot_window_map == NULL)
		priv->annot_window_map = g_hash_table_new (g_direct_hash, NULL);

	g_hash_table_insert (priv->annot_window_map, annot, window);
}

static EvViewWindowChild *
ev_view_get_window_child (EvView    *view,
			  GtkWidget *window)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GList *children = priv->window_children;

	while (children) {
		EvViewWindowChild *child;

		child = (EvViewWindowChild *)children->data;
		children = children->next;

		if (child->window == window)
			return child;
	}

	return NULL;
}

static void
ev_view_window_child_put (EvView    *view,
			  GtkWidget *window,
			  guint      page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvViewWindowChild *child;

	child = g_new0 (EvViewWindowChild, 1);
	child->window = window;
	child->page = page;
	child->visible = ev_annotation_window_is_open (EV_ANNOTATION_WINDOW (window));

	gtk_widget_set_visible (window, child->visible);

	priv->window_children = g_list_append (priv->window_children, child);
}

static void
ev_view_remove_window_child_for_annot (EvView       *view,
				       guint         page,
				       EvAnnotation *annot)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GList *children = priv->window_children;

	while (children) {
		EvViewWindowChild *child;
		EvAnnotation      *wannot;

		child = (EvViewWindowChild *)children->data;

		if (child->page != page) {
			children = children->next;
			continue;
		}
		wannot = ev_annotation_window_get_annotation (EV_ANNOTATION_WINDOW (child->window));
		if (ev_annotation_equal (wannot, annot)) {
			gtk_window_destroy (GTK_WINDOW (child->window));
			priv->window_children = g_list_delete_link (priv->window_children, children);
			break;
		}
		children = children->next;
	}
}

static void
ev_view_window_children_free (EvView *view)
{
	GList *l;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->window_children)
		return;

	for (l = priv->window_children; l && l->data; l = g_list_next (l)) {
		EvViewWindowChild *child;

		child = (EvViewWindowChild *)l->data;
		gtk_window_destroy (GTK_WINDOW (child->window));
		g_free (child);
	}
	g_clear_pointer (&priv->window_children, g_list_free);
}

static void
annotation_window_closed (EvAnnotationWindow *window,
			  EvView             *view)
{
	EvViewWindowChild *child;

	child = ev_view_get_window_child (view, GTK_WIDGET (window));
	child->visible = FALSE;
}

static void
ev_view_annotation_save_contents (EvView       *view,
				  GParamSpec   *pspec,
				  EvAnnotation *annot)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->document)
		return;

	ev_document_doc_mutex_lock ();
	ev_document_annotations_save_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
						 annot, EV_ANNOTATIONS_SAVE_CONTENTS);
	ev_document_doc_mutex_unlock ();
	g_signal_emit (view, signals[SIGNAL_ANNOT_CHANGED], 0, annot);
}

static GtkWidget *
ev_view_create_annotation_window (EvView       *view,
				  EvAnnotation *annot,
				  GtkWindow    *parent)
{
	GtkWidget   *window;
	guint        page;

	window = ev_annotation_window_new (annot, parent);
	g_signal_connect (window, "closed",
			  G_CALLBACK (annotation_window_closed),
			  view);
	g_signal_connect_swapped (annot, "notify::contents",
				  G_CALLBACK (ev_view_annotation_save_contents),
				  view);
	map_annot_to_window (view, annot, window);

	page = ev_annotation_get_page_index (annot);

	ev_view_window_child_put (view, window, page);

        ev_annotation_window_set_enable_spellchecking (EV_ANNOTATION_WINDOW (window), ev_view_get_enable_spellchecking (view));

	return window;
}

static void
show_annotation_windows (EvView *view,
			 gint    page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvMappingList *annots;
	GList         *l;
	GtkWindow     *parent;

	parent = GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (view)));

	annots = ev_page_cache_get_annot_mapping (priv->page_cache, page);

	for (l = ev_mapping_list_get_list (annots); l && l->data; l = g_list_next (l)) {
		EvAnnotation      *annot;
		GtkWidget         *window;

		annot = ((EvMapping *)(l->data))->data;

		if (!EV_IS_ANNOTATION_MARKUP (annot))
			continue;

		if (!ev_annotation_markup_has_popup (EV_ANNOTATION_MARKUP (annot)))
			continue;

		window = get_window_for_annot (view, annot);
		if (window) {
			EvViewWindowChild *child;
			child = ev_view_get_window_child (view, window);
			gtk_widget_set_visible (window, child->visible);
		} else {
			ev_view_create_annotation_window (view, annot, parent);
		}
	}
}

static void
hide_annotation_windows (EvView *view,
			 gint    page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvMappingList *annots;
	GList         *l;

	annots = ev_page_cache_get_annot_mapping (priv->page_cache, page);

	for (l = ev_mapping_list_get_list (annots); l && l->data; l = g_list_next (l)) {
		EvAnnotation *annot;
		GtkWidget    *window;

		annot = ((EvMapping *)(l->data))->data;

		if (!EV_IS_ANNOTATION_MARKUP (annot))
			continue;

		window = get_window_for_annot (view, annot);
		if (window)
			gtk_widget_set_visible (window, FALSE);
	}
}

static int
cmp_mapping_area_size (EvMapping *a,
		       EvMapping *b)
{
	gdouble wa, ha, wb, hb;

	wa = a->area.x2 - a->area.x1;
	ha = a->area.y2 - a->area.y1;
	wb = b->area.x2 - b->area.x1;
	hb = b->area.y2 - b->area.y1;

	if (wa == wb) {
		if (ha == hb)
			return 0;
		return (ha < hb) ? -1 : 1;
	}

	if (ha == hb) {
		return (wa < wb) ? -1 : 1;
	}

	return (wa * ha < wb * hb) ? -1 : 1;
}

static EvMapping *
get_annotation_mapping_at_location (EvView *view,
				    gdouble x,
				    gdouble y,
				    gint *page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint x_new = 0, y_new = 0;
	EvMappingList *annotations_mapping;
	EvDocumentAnnotations *doc_annots;
	EvAnnotation *annot;
	EvMapping *best;
	GList *list;

	if (!EV_IS_DOCUMENT_ANNOTATIONS (priv->document))
		return NULL;

	doc_annots = EV_DOCUMENT_ANNOTATIONS (priv->document);

	if (!doc_annots)
		return NULL;

	if (!get_doc_point_from_location (view, x, y, page, &x_new, &y_new))
		return NULL;

	annotations_mapping = ev_page_cache_get_annot_mapping (priv->page_cache, *page);

	if (!annotations_mapping)
		return NULL;

	best = NULL;
	for (list = ev_mapping_list_get_list (annotations_mapping); list; list = list->next) {
		EvMapping *mapping = list->data;

		if ((x_new >= mapping->area.x1) &&
		    (y_new >= mapping->area.y1) &&
		    (x_new <= mapping->area.x2) &&
		    (y_new <= mapping->area.y2)) {

			annot = EV_ANNOTATION (mapping->data);

			if (ev_annotation_get_annotation_type (annot) == EV_ANNOTATION_TYPE_TEXT_MARKUP &&
			    ev_document_annotations_over_markup (doc_annots, annot, (gdouble) x_new, (gdouble) y_new)
								== EV_ANNOTATION_OVER_MARKUP_NOT)
				continue; /* ignore markup annots clicked outside the markup text */

			/* In case of only one match choose that. Otherwise
			 * compare the area of the bounding boxes and return the
			 * smallest element */
			if (best == NULL || cmp_mapping_area_size (mapping, best) < 0)
				best = mapping;
		}
	}
	return best;
}

static EvAnnotation *
ev_view_get_annotation_at_location (EvView  *view,
				    gdouble  x,
				    gdouble  y)
{
	EvMapping *annotation_mapping;
	gint page;

	annotation_mapping = get_annotation_mapping_at_location (view, x, y, &page);

	return annotation_mapping ? annotation_mapping->data : NULL;
}

static void
ev_view_annotation_show_popup_window (EvView    *view,
				      GtkWidget *window)
{
	EvViewWindowChild *child;

	if (!window)
		return;

	child = ev_view_get_window_child (view, window);
	if (!child->visible) {
		child->visible = TRUE;
		gtk_widget_set_visible (window, TRUE);
	}
}

static void
ev_view_annotation_create_show_popup_window (EvView       *view,
					     EvAnnotation *annot)
{
	GtkWindow  *parent;
	/* the annotation window might already exist */
	GtkWidget  *window = get_window_for_annot (view, annot);

	if (!window) {
		parent = GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (view)));
		window = ev_view_create_annotation_window (view, annot, parent);
	}

	ev_view_annotation_show_popup_window (view, window);
}

static void
ev_view_handle_annotation (EvView       *view,
			   EvAnnotation *annot,
			   gdouble       x,
			   gdouble       y,
			   guint32       timestamp)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (EV_IS_ANNOTATION_MARKUP (annot)) {
		GtkWidget *window;

		window = get_window_for_annot (view, annot);
		if (!window && ev_annotation_markup_can_have_popup (EV_ANNOTATION_MARKUP (annot))) {
			EvRectangle    popup_rect;
			GtkWindow     *parent;
			EvMappingList *annots;
			EvMapping     *mapping;

			annots = ev_page_cache_get_annot_mapping (priv->page_cache,
								  ev_annotation_get_page_index (annot));
			mapping = ev_mapping_list_find (annots, annot);

			popup_rect.x1 = mapping->area.x2;
			popup_rect.y1 = mapping->area.y2;
			popup_rect.x2 = popup_rect.x1 + ANNOT_POPUP_WINDOW_DEFAULT_WIDTH;
			popup_rect.y2 = popup_rect.y1 + ANNOT_POPUP_WINDOW_DEFAULT_HEIGHT;
			g_object_set (annot,
				      "rectangle", &popup_rect,
				      "has_popup", TRUE,
				      "popup_is_open", TRUE,
				      NULL);

			parent = GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (view)));
			window = ev_view_create_annotation_window (view, annot, parent);
		} else if (window && ev_annotation_markup_has_popup (EV_ANNOTATION_MARKUP (annot))) {
			EvMappingList     *annots;
			EvRectangle        popup_rect;
			EvMapping         *mapping;

			annots = ev_page_cache_get_annot_mapping (priv->page_cache,
								  ev_annotation_get_page_index (annot));
			mapping = ev_mapping_list_find (annots, annot);
			ev_annotation_markup_get_rectangle (EV_ANNOTATION_MARKUP (annot),
							    &popup_rect);

			popup_rect.x2 = mapping->area.x2 + popup_rect.x2 - popup_rect.x1;
			popup_rect.y2 = mapping->area.y2 + popup_rect.y2 - popup_rect.y1;
			popup_rect.x1 = mapping->area.x2;
			popup_rect.y1 = mapping->area.y2;
			g_object_set (annot,
				      "rectangle", &popup_rect,
				      "popup_is_open", TRUE,
				      NULL);
		}
		ev_view_annotation_show_popup_window (view, window);
	}

	if (EV_IS_ANNOTATION_ATTACHMENT (annot)) {
		EvAttachment *attachment;

		attachment = ev_annotation_attachment_get_attachment (EV_ANNOTATION_ATTACHMENT (annot));
		if (attachment) {
			GError *error = NULL;
			GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (view));
			GdkAppLaunchContext *context = gdk_display_get_app_launch_context (display);
			gdk_app_launch_context_set_timestamp (context, timestamp);

			ev_attachment_open (attachment,
					    G_APP_LAUNCH_CONTEXT (context),
					    &error);

			if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}

			g_clear_object (&context);
		}
	}
}

static void
ev_view_create_annotation_real (EvView *view,
				gint    annot_page,
				EvPoint start,
				EvPoint end)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvAnnotation   *annot;
	EvRectangle     doc_rect, popup_rect;
	EvPage         *page;
	GdkRGBA         color = EV_ANNOTATION_DEFAULT_COLOR;
	GdkRectangle    view_rect;
	cairo_region_t *region;

	ev_document_doc_mutex_lock ();
	page = ev_document_get_page (priv->document, annot_page);
        switch (priv->adding_annot_info.type) {
        case EV_ANNOTATION_TYPE_TEXT:
                doc_rect.x1 = end.x;
                doc_rect.y1 = end.y;
                doc_rect.x2 = doc_rect.x1 + ANNOTATION_ICON_SIZE;
                doc_rect.y2 = doc_rect.y1 + ANNOTATION_ICON_SIZE;
                annot = ev_annotation_text_new (page);
                break;
	case EV_ANNOTATION_TYPE_TEXT_MARKUP:
		doc_rect.x1 = start.x;
		doc_rect.y1 = start.y;
		doc_rect.x2 = end.x;
		doc_rect.y2 = end.y;
		annot = ev_annotation_text_markup_highlight_new (page);
		break;
	case EV_ANNOTATION_TYPE_ATTACHMENT:
		/* TODO */
		g_object_unref (page);
		ev_document_doc_mutex_unlock ();
		return;
	default:
		g_assert_not_reached ();
	}
	g_object_unref (page);

	ev_annotation_set_area (annot, &doc_rect);
	ev_annotation_set_rgba (annot, &color);

	if (EV_IS_ANNOTATION_MARKUP (annot)) {
		popup_rect.x1 = doc_rect.x2;
		popup_rect.x2 = popup_rect.x1 + ANNOT_POPUP_WINDOW_DEFAULT_WIDTH;
		popup_rect.y1 = doc_rect.y2;
		popup_rect.y2 = popup_rect.y1 + ANNOT_POPUP_WINDOW_DEFAULT_HEIGHT;
		g_object_set (annot,
			      "rectangle", &popup_rect,
			      "can-have-popup", TRUE,
			      "has_popup", TRUE,
			      "popup_is_open", FALSE,
			      "label", g_get_real_name (),
			      "opacity", 1.0,
			      NULL);
	}
	ev_document_annotations_add_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
						annot, &doc_rect);
	/* Re-fetch area as eg. adding Text Markup annots updates area for its bounding box */
	ev_annotation_get_area (annot, &doc_rect);
	ev_document_doc_mutex_unlock ();

	/* If the page didn't have annots, mark the cache as dirty */
	if (!ev_page_cache_get_annot_mapping (priv->page_cache, annot_page))
		ev_page_cache_mark_dirty (priv->page_cache, annot_page, EV_PAGE_DATA_INCLUDE_ANNOTS);

	_ev_view_transform_doc_rect_to_view_rect (view, annot_page, &doc_rect, &view_rect);
	view_rect.x -= priv->scroll_x;
	view_rect.y -= priv->scroll_y;
	region = cairo_region_create_rectangle (&view_rect);
	ev_view_reload_page (view, annot_page, region);
	cairo_region_destroy (region);

	priv->adding_annot_info.annot = annot;
}

static void
ev_view_create_annotation (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvPoint         start;
	EvPoint         end;
	gint            annot_page;
	gint            offset;
	GdkRectangle    page_area;
	GtkBorder       border;

	find_page_at_location (view, priv->adding_annot_info.start.x, priv->adding_annot_info.start.y, &annot_page, &offset, &offset);
	if (annot_page == -1) {
		ev_view_cancel_add_annotation (view);
		return;
	}

	ev_view_get_page_extents (view, annot_page, &page_area, &border);
	_ev_view_transform_view_point_to_doc_point (view, &priv->adding_annot_info.start, &page_area, &border,
						    &start.x, &start.y);
	_ev_view_transform_view_point_to_doc_point (view, &priv->adding_annot_info.stop, &page_area, &border,
						    &end.x, &end.y);

	ev_view_create_annotation_real (view, annot_page, start, end);
}

static gboolean
ev_view_get_doc_points_from_selection_region (EvView  *view,
					      gint     page,
					      EvPoint *begin,
					      EvPoint *end)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	cairo_rectangle_int_t first, last;
	GdkPoint start, stop;
	cairo_region_t *region = NULL;

	if (!priv->pixbuf_cache)
		return FALSE;

	region = ev_pixbuf_cache_get_selection_region (priv->pixbuf_cache, page, priv->scale);

	if (!region)
		return FALSE;

	cairo_region_get_rectangle (region, 0, &first);
	cairo_region_get_rectangle (region, cairo_region_num_rectangles(region) - 1, &last);

	if (!get_doc_point_from_offset (view, page, first.x, first.y + (first.height / 2),
					&(start.x), &(start.y)))
		return FALSE;

	if (!get_doc_point_from_offset (view, page, last.x + last.width, last.y + (last.height / 2),
					&(stop.x), &(stop.y)))
		return FALSE;

	begin->x = start.x;
	begin->y = start.y;
	end->x = stop.x;
	end->y = stop.y;

	return TRUE;
}

static void
ev_view_create_annotation_from_selection (EvView          *view,
					  EvViewSelection *selection)
{
	EvPoint doc_point_start;
	EvPoint doc_point_end;

	/* Check if selection is of double/triple click type (STYLE_WORD and STYLE_LINE) and in that
	 * case get the start/end points from the selection region of pixbuf cache. Issue #1119 */
	if (selection->style == EV_SELECTION_STYLE_WORD || selection->style == EV_SELECTION_STYLE_LINE) {

		if (!ev_view_get_doc_points_from_selection_region (view, selection->page,
								   &doc_point_start, &doc_point_end))
			return;
	} else {
		doc_point_start.x = selection->rect.x1;
		doc_point_start.y = selection->rect.y1;
		doc_point_end.x = selection->rect.x2;
		doc_point_end.y = selection->rect.y2;
	}

	ev_view_create_annotation_real (view, selection->page, doc_point_start, doc_point_end);
}
void
ev_view_focus_annotation (EvView    *view,
			  EvMapping *annot_mapping)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!EV_IS_DOCUMENT_ANNOTATIONS (priv->document))
		return;

	_ev_view_set_focused_element (view, annot_mapping,
				     ev_annotation_get_page_index (EV_ANNOTATION (annot_mapping->data)));
}

void
ev_view_begin_add_annotation (EvView          *view,
			      EvAnnotationType annot_type)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (annot_type == EV_ANNOTATION_TYPE_UNKNOWN)
		return;

	if (priv->adding_annot_info.adding_annot)
		return;

	priv->adding_annot_info.adding_annot = TRUE;
	priv->adding_annot_info.type = annot_type;
	ev_view_set_cursor (view, EV_VIEW_CURSOR_ADD);
}

void
ev_view_cancel_add_annotation (EvView *view)
{
	gint x, y;
	guint annot_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->adding_annot_info.adding_annot)
		return;

	if (priv->adding_annot_info.annot && priv->pressed_button == GDK_BUTTON_PRIMARY) {
		annot_page = ev_annotation_get_page_index (priv->adding_annot_info.annot);
		ev_document_doc_mutex_lock ();
		ev_document_annotations_remove_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
							   priv->adding_annot_info.annot);
		ev_document_doc_mutex_unlock ();
		ev_page_cache_mark_dirty (priv->page_cache, annot_page, EV_PAGE_DATA_INCLUDE_ANNOTS);
		priv->adding_annot_info.annot = NULL;
		priv->pressed_button = -1;
		ev_view_reload_page (view, annot_page, NULL);
	}

	priv->adding_annot_info.adding_annot = FALSE;
	g_assert(!priv->adding_annot_info.annot);
	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y, FALSE);
}

void
ev_view_remove_annotation (EvView       *view,
                           EvAnnotation *annot)
{
        guint page;
	EvViewPrivate *priv = GET_PRIVATE (view);

        g_return_if_fail (EV_IS_VIEW (view));
        g_return_if_fail (EV_IS_ANNOTATION (annot));

	g_object_ref (annot);

        page = ev_annotation_get_page_index (annot);

        if (EV_IS_ANNOTATION_MARKUP (annot))
		ev_view_remove_window_child_for_annot (view, page, annot);
	if (priv->annot_window_map != NULL)
		g_hash_table_remove (priv->annot_window_map, annot);

        _ev_view_set_focused_element (view, NULL, -1);

        ev_document_doc_mutex_lock ();
        ev_document_annotations_remove_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
                                                   annot);
        ev_document_doc_mutex_unlock ();

        ev_page_cache_mark_dirty (priv->page_cache, page, EV_PAGE_DATA_INCLUDE_ANNOTS);

	/* FIXME: only redraw the annot area */
        ev_view_reload_page (view, page, NULL);

	g_signal_emit (view, signals[SIGNAL_ANNOT_REMOVED], 0, annot);
	g_object_unref (annot);
}

static gboolean
ev_view_synctex_backward_search (EvView *view,
				 gdouble x,
				 gdouble y)
{
	gint page = -1;
	gint x_new = 0, y_new = 0;
	EvSourceLink *link;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!ev_document_has_synctex (priv->document))
		return FALSE;

	if (!get_doc_point_from_location (view, x, y, &page, &x_new, &y_new))
		return FALSE;

	link = ev_document_synctex_backward_search (priv->document, page, x_new, y_new);
	if (link) {
		g_signal_emit (view, signals[SIGNAL_SYNC_SOURCE], 0, link);
		ev_source_link_free (link);

		return TRUE;
	}

	return FALSE;
}

/* Caret navigation */
#define CURSOR_ON_MULTIPLIER 2
#define CURSOR_OFF_MULTIPLIER 1
#define CURSOR_PEND_MULTIPLIER 3
#define CURSOR_DIVIDER 3

static inline gboolean
cursor_is_in_visible_page (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	return (priv->cursor_page == priv->current_page ||
		(priv->cursor_page >= priv->start_page &&
		 priv->cursor_page <= priv->end_page));
}

static gboolean
cursor_should_blink (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (priv->caret_enabled &&
	    priv->rotation == 0 &&
	    cursor_is_in_visible_page (view) &&
	    gtk_widget_has_focus (GTK_WIDGET (view)) &&
	    priv->pixbuf_cache &&
	    !ev_pixbuf_cache_get_selection_region (priv->pixbuf_cache, priv->cursor_page, priv->scale)) {
		GtkSettings *settings;
		gboolean blink;

		settings = gtk_widget_get_settings (GTK_WIDGET (view));
		g_object_get (settings, "gtk-cursor-blink", &blink, NULL);

		return blink;
	}

	return FALSE;
}

static gint
get_cursor_blink_time (EvView *view)
{
	GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (view));
	gint time;

	g_object_get (settings, "gtk-cursor-blink-time", &time, NULL);

	return time;
}

static gint
get_cursor_blink_timeout_id (EvView *view)
{
	GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (view));
	gint timeout;

	g_object_get (settings, "gtk-cursor-blink-timeout", &timeout, NULL);

	return timeout;
}

static gboolean
get_caret_cursor_area (EvView       *view,
		       gint          page,
		       gint          offset,
		       GdkRectangle *area)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvRectangle *areas = NULL;
	EvRectangle *doc_rect;
	guint        n_areas = 0;
	gdouble      cursor_aspect_ratio;
	gint         stem_width;

	if (!priv->caret_enabled || priv->rotation != 0)
		return FALSE;

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_layout (priv->page_cache, page, &areas, &n_areas);
	if (!areas)
		return FALSE;

	if (offset > n_areas)
		return FALSE;

	doc_rect = areas + offset;
	if (offset == n_areas ||
	    ((doc_rect->x1 == doc_rect->x2 || doc_rect->y1 == doc_rect->y2) && offset > 0)) {
		EvRectangle *prev;
		EvRectangle  last_rect;

		/* Special characters like \n have an empty bounding box
		 * and the end of a page doesn't have any bounding box,
		 * use the size of the previous area.
		 */
		prev = areas + offset - 1;
		last_rect.x1 = prev->x2;
		last_rect.y1 = prev->y1;
		last_rect.x2 = prev->x2 + (prev->x2 - prev->x1);
		last_rect.y2 = prev->y2;

		_ev_view_transform_doc_rect_to_view_rect (view, page, &last_rect, area);
	} else {
		_ev_view_transform_doc_rect_to_view_rect (view, page, doc_rect, area);
	}

	area->x -= priv->scroll_x;
	area->y -= priv->scroll_y;

	g_object_get (gtk_settings_get_for_display (
		gtk_style_context_get_display (gtk_widget_get_style_context (GTK_WIDGET (view)))),
                "gtk-cursor-aspect-ratio", &cursor_aspect_ratio,
                NULL);

	stem_width = area->height * cursor_aspect_ratio + 1;
	area->x -= (stem_width / 2);
	area->width = stem_width;

	return TRUE;
}

static void
show_cursor (EvView *view)
{
	GtkWidget   *widget;
	GdkRectangle view_rect;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->cursor_visible)
		return;

	widget = GTK_WIDGET (view);
	priv->cursor_visible = TRUE;
	if (gtk_widget_has_focus (widget) &&
	    get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &view_rect)) {
		gtk_widget_queue_draw (widget);
	}
}

static void
hide_cursor (EvView *view)
{
	GtkWidget   *widget;
	GdkRectangle view_rect;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->cursor_visible)
		return;

	widget = GTK_WIDGET (view);
	priv->cursor_visible = FALSE;
	if (gtk_widget_has_focus (widget) &&
	    get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &view_rect)) {
		gtk_widget_queue_draw (widget);
	}
}

static gboolean
blink_cb (EvView *view)
{
	gint blink_timeout;
	guint blink_time;
	EvViewPrivate *priv = GET_PRIVATE (view);

	blink_timeout = get_cursor_blink_timeout_id (view);
	if (priv->cursor_blink_time > 1000 * blink_timeout && blink_timeout < G_MAXINT / 1000) {
		/* We've blinked enough without the user doing anything, stop blinking */
		show_cursor (view);
		priv->cursor_blink_timeout_id = 0;

		return G_SOURCE_REMOVE;
	}

	blink_time = get_cursor_blink_time (view);
	if (priv->cursor_visible) {
		hide_cursor (view);
		blink_time *= CURSOR_OFF_MULTIPLIER;
	} else {
		show_cursor (view);
		priv->cursor_blink_time += blink_time;
		blink_time *= CURSOR_ON_MULTIPLIER;
	}

	priv->cursor_blink_timeout_id = g_timeout_add (blink_time / CURSOR_DIVIDER, (GSourceFunc)blink_cb, view);

	return G_SOURCE_REMOVE;
}

static void
ev_view_check_cursor_blink (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (cursor_should_blink (view))	{
		if (priv->cursor_blink_timeout_id == 0) {
			show_cursor (view);
			priv->cursor_blink_timeout_id = g_timeout_add (get_cursor_blink_time (view) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
										 (GSourceFunc)blink_cb, view);
		}

		return;
	}

	g_clear_handle_id (&priv->cursor_blink_timeout_id, g_source_remove);

	priv->cursor_visible = TRUE;
	priv->cursor_blink_time = 0;
}

static void
ev_view_pend_cursor_blink (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!cursor_should_blink (view))
		return;

	g_clear_handle_id (&priv->cursor_blink_timeout_id, g_source_remove);

	show_cursor (view);
	priv->cursor_blink_timeout_id = g_timeout_add (get_cursor_blink_time (view) * CURSOR_PEND_MULTIPLIER / CURSOR_DIVIDER,
								 (GSourceFunc)blink_cb, view);
}

static void
preload_pages_for_caret_navigation (EvView *view)
{
	gint n_pages;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return;

	/* Upload to the cache the first and last pages,
	 * this information is needed to position the cursor
	 * in the beginning/end of the document, for example
	 * when pressing <Ctr>Home/End
	 */
	n_pages = ev_document_get_n_pages (priv->document);

	/* For documents with at least 3 pages, those are already cached anyway */
	if (n_pages > 0 && n_pages <= 3)
		return;

	ev_page_cache_ensure_page (priv->page_cache, 0);
	ev_page_cache_ensure_page (priv->page_cache, n_pages - 1);
}

/**
 * ev_view_supports_caret_navigation:
 * @view: a #EvView
 *
 * Returns: whether the document supports caret navigation
 *
 * Since: 3.10
 */
gboolean
ev_view_supports_caret_navigation (EvView *view)
{
	EvDocumentTextInterface *iface;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document || !EV_IS_DOCUMENT_TEXT (priv->document))
		return FALSE;

	iface = EV_DOCUMENT_TEXT_GET_IFACE (priv->document);
	if (!iface->get_text_layout || !iface->get_text)
		return FALSE;

	return TRUE;
}

/**
 * ev_view_set_caret_navigation_enabled:
 * @view: a #EvView
 * @enabled: whether to enable caret navigation mode
 *
 * Enables or disables caret navigation mode for the document.
 *
 * Since: 3.10
 */
void
ev_view_set_caret_navigation_enabled (EvView   *view,
				      gboolean enabled)
{
	g_return_if_fail (EV_IS_VIEW (view));
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->caret_enabled != enabled) {
		priv->caret_enabled = enabled;
		if (priv->caret_enabled)
			preload_pages_for_caret_navigation (view);

		ev_view_check_cursor_blink (view);

		if (cursor_is_in_visible_page (view))
			gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}

/**
 * ev_view_get_caret_navigation_enabled:
 * @view: a #EvView
 *
 * Returns: whether caret navigation mode is enabled for the document
 *
 * Since: 3.10
 */
gboolean
ev_view_is_caret_navigation_enabled (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);
	EvViewPrivate *priv = GET_PRIVATE (view);

	return priv->caret_enabled;
}

/**
 * ev_view_set_caret_cursor_position:
 * @view: a #EvView
 * @page:
 * @offset:
 *
 * Since: 3.10
 */
void
ev_view_set_caret_cursor_position (EvView *view,
				   guint   page,
				   guint   offset)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	g_return_if_fail (EV_IS_VIEW (view));
	g_return_if_fail (EV_IS_DOCUMENT (priv->document));
	g_return_if_fail (page < ev_document_get_n_pages (priv->document));

	if (priv->cursor_page != page || priv->cursor_offset != offset) {
		priv->cursor_page = page;
		priv->cursor_offset = offset;

		g_signal_emit (view, signals[SIGNAL_CURSOR_MOVED], 0,
			       priv->cursor_page, priv->cursor_offset);

		if (priv->caret_enabled && cursor_is_in_visible_page (view))
			gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}
/*** GtkWidget implementation ***/

static void
ev_view_size_request_continuous_dual_page (EvView         *view,
			     	           GtkRequisition *requisition)
{
	gint n_pages;
	GtkBorder border;
	EvViewPrivate *priv = GET_PRIVATE (view);

	n_pages = ev_document_get_n_pages (priv->document) + 1;
	compute_border (view, &border);
	get_page_y_offset (view, n_pages, &requisition->height, &border);

	switch (priv->sizing_mode) {
	        case EV_SIZING_FIT_WIDTH:
	        case EV_SIZING_FIT_PAGE:
	        case EV_SIZING_AUTOMATIC:
			requisition->width = 1;

			break;
	        case EV_SIZING_FREE: {
			gint max_width;

			ev_view_get_max_page_size (view, &max_width, NULL);
			requisition->width = (max_width + border.left + border.right) * 2 + (priv->spacing * 3);
		}
			break;
	        default:
			g_assert_not_reached ();
	}
}

static void
ev_view_size_request_continuous (EvView         *view,
				 GtkRequisition *requisition)
{
	gint n_pages;
	GtkBorder border;
	EvViewPrivate *priv = GET_PRIVATE (view);

	n_pages = ev_document_get_n_pages (priv->document);
	compute_border (view, &border);
	get_page_y_offset (view, n_pages, &requisition->height, &border);

	switch (priv->sizing_mode) {
	        case EV_SIZING_FIT_WIDTH:
	        case EV_SIZING_FIT_PAGE:
	        case EV_SIZING_AUTOMATIC:
			requisition->width = 1;

			break;
	        case EV_SIZING_FREE: {
			gint max_width;

			ev_view_get_max_page_size (view, &max_width, NULL);
			requisition->width = max_width + (priv->spacing * 2) + border.left + border.right;
		}
			break;
	        default:
			g_assert_not_reached ();
	}
}

static void
ev_view_size_request_dual_page (EvView         *view,
				GtkRequisition *requisition)
{
	GtkBorder border;
	gint width, height;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->sizing_mode == EV_SIZING_FIT_PAGE) {
		requisition->width = 1;
		requisition->height = 1;

		return;
	}

	/* Find the largest of the two. */
	ev_view_get_page_size (view,
			       priv->current_page,
			       &width, &height);
	if (priv->current_page + 1 < ev_document_get_n_pages (priv->document)) {
		gint width_2, height_2;
		ev_view_get_page_size (view,
				       priv->current_page + 1,
				       &width_2, &height_2);
		if (width_2 > width) {
			width = width_2;
			height = height_2;
		}
	}
	compute_border (view, &border);

	requisition->width = priv->sizing_mode == EV_SIZING_FIT_WIDTH ? 1 :
		((width + border.left + border.right) * 2) + (priv->spacing * 3);
	requisition->height = (height + border.top + border.bottom) + (priv->spacing * 2);
}

static void
ev_view_size_request_single_page (EvView         *view,
				  GtkRequisition *requisition)
{
	GtkBorder border;
	gint width, height;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->sizing_mode == EV_SIZING_FIT_PAGE) {
		requisition->width = 1;
		requisition->height = 1;

		return;
	}

	ev_view_get_page_size (view, priv->current_page, &width, &height);
	compute_border (view, &border);

	requisition->width = priv->sizing_mode == EV_SIZING_FIT_WIDTH ? 1 :
		width + border.left + border.right + (2 * priv->spacing);
	requisition->height = height + border.top + border.bottom + (2 * priv->spacing);
}

static void
ev_view_size_request (GtkWidget      *widget,
		      GtkRequisition *requisition)
{
	EvView *view = EV_VIEW (widget);
	gboolean dual_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->document == NULL) {
		priv->requisition.width = 1;
		priv->requisition.height = 1;
	} else {
		dual_page = is_dual_page(view, NULL);
		if (priv->continuous && dual_page)
			ev_view_size_request_continuous_dual_page(view, &priv->requisition);
		else if (priv->continuous)
			ev_view_size_request_continuous(view, &priv->requisition);
		else if (dual_page)
			ev_view_size_request_dual_page(view, &priv->requisition);
		else
			ev_view_size_request_single_page(view, &priv->requisition);
	}

	if (requisition)
		*requisition = priv->requisition;
}

static void
ev_view_measure (GtkWidget* widget,
		 GtkOrientation orientation,
		 int for_size,
		 int* minimum,
		 int* natural,
		 int* minimum_baseline,
		 int* natural_baseline)
{
	GtkRequisition requisition;

	ev_view_size_request (widget, &requisition);

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		*minimum = *natural = requisition.width;

	if (orientation == GTK_ORIENTATION_VERTICAL)
		*minimum = *natural = requisition.height;
}

static void
ev_view_size_allocate (GtkWidget      *widget,
		       int             width,
		       int	       height,
		       int	       baseline)
{
	EvView *view = EV_VIEW (widget);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document || ev_view_is_loading (view))
		return;

	if (priv->sizing_mode == EV_SIZING_FIT_WIDTH ||
	    priv->sizing_mode == EV_SIZING_FIT_PAGE ||
	    priv->sizing_mode == EV_SIZING_AUTOMATIC) {
		ev_view_zoom_for_size (view, width, height);
		ev_view_size_request (widget, NULL);
	}

	ev_view_set_adjustment_values (view, GTK_ORIENTATION_HORIZONTAL, width, height);
	ev_view_set_adjustment_values (view, GTK_ORIENTATION_VERTICAL, width, height);

	view_update_range_and_current_page (view);

	priv->pending_scroll = SCROLL_TO_KEEP_POSITION;
	priv->pending_resize = FALSE;
	priv->pending_point.x = 0;
	priv->pending_point.y = 0;

	for (GtkWidget *child = gtk_widget_get_first_child (widget);
		child != NULL;
		child = gtk_widget_get_next_sibling (child)) {
		EvViewChild *data = g_object_get_data (G_OBJECT (child), "ev-child");
		GdkRectangle view_area;

		if (!data || !gtk_widget_get_visible (child))
			continue;

		_ev_view_transform_doc_rect_to_view_rect (view, data->page, &data->doc_rect, &view_area);
		view_area.x -= priv->scroll_x;
		view_area.y -= priv->scroll_y;

		gtk_widget_set_size_request (child, view_area.width, view_area.height);
		// TODO: this is a temporary solution to eliminate the warning
		gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, view_area.width, NULL, NULL, NULL, NULL);
		gtk_widget_size_allocate (child, &view_area, baseline);
	}

	if (priv->link_preview.popover)
		gtk_popover_present (GTK_POPOVER (priv->link_preview.popover));
}

static gboolean
ev_view_scroll_event (GtkEventControllerScroll *self, gdouble dx, gdouble dy, GtkWidget *widget)
{
	EvView *view = EV_VIEW (widget);
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkEvent *event;
	guint state;
	GdkScrollDirection direction;
	double x, y;

	ev_view_link_preview_popover_cleanup (view);

	event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (self));
	state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (self))
			 & gtk_accelerator_get_default_mod_mask ();
	direction = gdk_scroll_event_get_direction (event);
	gdk_event_get_axis (GDK_EVENT (event), GDK_AXIS_X, &x);
	gdk_event_get_axis (GDK_EVENT (event), GDK_AXIS_Y, &y);


	if (state == GDK_CONTROL_MASK) {
		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
		priv->zoom_center_x = x;
		priv->zoom_center_y = y;

		switch (direction) {
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			if (ev_view_can_zoom_out (view))
				ev_view_zoom_out (view);
			break;
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			if (ev_view_can_zoom_in (view))
				ev_view_zoom_in (view);
			break;
		case GDK_SCROLL_SMOOTH: {
			gdouble delta = dx + dy;
			gdouble factor = pow (delta < 0 ? ZOOM_IN_FACTOR : ZOOM_OUT_FACTOR, fabs (delta));

			if (ev_view_can_zoom (view, factor))
				ev_view_zoom (view, factor);
		}
			break;
		}

		return TRUE;
	}

	priv->jump_to_find_result = FALSE;

#if 0
	/* TODO: implement this in GTK4 */
	/* Shift+Wheel scrolls the in the perpendicular direction */
	if (state & GDK_SHIFT_MASK) {
		if (event->direction == GDK_SCROLL_UP)
			event->direction = GDK_SCROLL_LEFT;
		else if (event->direction == GDK_SCROLL_LEFT)
			event->direction = GDK_SCROLL_UP;
		else if (event->direction == GDK_SCROLL_DOWN)
			event->direction = GDK_SCROLL_RIGHT;
		else if (event->direction == GDK_SCROLL_RIGHT)
			event->direction = GDK_SCROLL_DOWN;
		else if (event->direction == GDK_SCROLL_SMOOTH) {
			/* Swap the deltas for perpendicular direction */
			gdouble tmp_delta = event->delta_x;

			event->delta_x = event->delta_y;
			event->delta_y = tmp_delta;
		}

		event->state &= ~GDK_SHIFT_MASK;
		state &= ~GDK_SHIFT_MASK;
	}

	fit_width = ev_view_page_fits (view, GTK_ORIENTATION_HORIZONTAL);
	fit_height = ev_view_page_fits (view, GTK_ORIENTATION_VERTICAL);
	if (state == 0 && !priv->continuous && (fit_width || fit_height)) {
		switch (event->direction) {
		case GDK_SCROLL_DOWN:
			if (fit_height) {
				ev_view_next_page (view);
				return TRUE;
			}
			break;
		case GDK_SCROLL_RIGHT:
			if (fit_width) {
				ev_view_next_page (view);
				return TRUE;
			}
			break;
		case GDK_SCROLL_UP:
			if (fit_height) {
				ev_view_previous_page (view);
				return TRUE;
			}
			break;
		case GDK_SCROLL_LEFT:
			if (fit_width) {
				ev_view_previous_page (view);
				return TRUE;
			}
			break;
		case GDK_SCROLL_SMOOTH: {
			gdouble decrement;
			if ((fit_width && fit_height) ||
			    ((fit_height && event->delta_x == 0.0) ||
			     (fit_width && event->delta_y == 0.0))) {
				/* Emulate normal scrolling by summing the deltas */
				priv->total_delta += event->delta_x + event->delta_y;

				decrement = priv->total_delta < 0 ? -1.0 : 1.0;
				for (; fabs (priv->total_delta) >= 1.0; priv->total_delta -= decrement) {
					if (decrement < 0)
						ev_view_previous_page (view);
					else
						ev_view_next_page (view);
				}

				return TRUE;
			}
		}
			break;
		}

		return FALSE;
	}

	/* Do scroll only on one axis at a time. Issue #866 */
	if (event->direction == GDK_SCROLL_SMOOTH &&
	    event->delta_x != 0.0 && event->delta_y != 0.0) {
		gdouble abs_x, abs_y;
		abs_x = fabs (event->delta_x);
		abs_y = fabs (event->delta_y);

		if (abs_y > abs_x)
			event->delta_x = 0.0;
		else if (abs_x > abs_y)
			event->delta_y = 0.0;
	}
#endif

	return FALSE;
}

static EvViewSelection *
find_selection_for_page (EvView *view,
			 gint    page)
{
	GList *list;
	EvViewPrivate *priv = GET_PRIVATE (view);

	for (list = priv->selection_info.selections; list != NULL; list = list->next) {
		EvViewSelection *selection;

		selection = (EvViewSelection *) list->data;

		if (selection->page == page)
			return selection;
	}

	return NULL;
}

/* This is based on the deprecated function gtk_draw_insertion_cursor. */
static void
draw_caret_cursor (EvView	*view,
		   GtkSnapshot	*snapshot)
{
	GdkRectangle view_rect;
	GdkRGBA      cursor_color;
	GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (view));
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &view_rect))
		return;

	gtk_style_context_get_color (context, &cursor_color);

	gtk_snapshot_save (snapshot);

	gtk_snapshot_append_color (snapshot, &cursor_color,
			&GRAPHENE_RECT_INIT (
				view_rect.x,
				view_rect.y,
				view_rect.width,
				view_rect.height));

	gtk_snapshot_restore (snapshot);
}

static gboolean
should_draw_caret_cursor (EvView  *view,
			  gint     page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	return (priv->caret_enabled &&
		priv->cursor_page == page &&
		priv->cursor_visible &&
		gtk_widget_has_focus (GTK_WIDGET (view)) &&
		!ev_pixbuf_cache_get_selection_region (priv->pixbuf_cache, page, priv->scale));
}

static void
draw_focus (EvView       *view,
	    GtkSnapshot  *snapshot,
	    gint          page,
	    GdkRectangle *clip)
{
	GtkWidget   *widget = GTK_WIDGET (view);
	GdkRectangle rect;
	GdkRectangle intersect;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->focused_element_page != page)
		return;

	if (!gtk_widget_has_focus (widget))
		return;

	if (!ev_view_get_focused_area (view, &rect))
		return;

	if (gdk_rectangle_intersect (&rect, clip, &intersect)) {
		gtk_snapshot_render_focus (snapshot,
				gtk_widget_get_style_context (widget),
				  intersect.x,
				  intersect.y,
				  intersect.width,
				  intersect.height);
	}
}

#ifdef EV_ENABLE_DEBUG
static void
stroke_view_rect (GtkSnapshot  *snapshot,
		  GdkRectangle *clip,
		  GdkRGBA      *color,
		  GdkRectangle *view_rect)
{
	GdkRectangle intersect;
	GdkRGBA border_color[4] = { *color, *color, *color, *color };
	float border_width[4] = { 1, 1, 1, 1 };

	if (gdk_rectangle_intersect (view_rect, clip, &intersect)) {
		gtk_snapshot_append_border (snapshot,
			&GSK_ROUNDED_RECT_INIT (intersect.x, intersect.y,
				 intersect.width, intersect.height),
			border_width, border_color);
	}
}

static void
stroke_doc_rect (EvView       *view,
		 GtkSnapshot  *snapshot,
		 GdkRGBA      *color,
		 gint          page,
		 GdkRectangle *clip,
		 EvRectangle  *doc_rect)
{
	GdkRectangle view_rect;
	EvViewPrivate *priv = GET_PRIVATE (view);

	_ev_view_transform_doc_rect_to_view_rect (view, page, doc_rect, &view_rect);
	view_rect.x -= priv->scroll_x;
	view_rect.y -= priv->scroll_y;
	stroke_view_rect (snapshot, clip, color, &view_rect);
}

static void
show_chars_border (EvView       *view,
		   GtkSnapshot  *snapshot,
		   gint          page,
		   GdkRectangle *clip)
{
	EvRectangle *areas = NULL;
	guint        n_areas = 0;
	guint        i;
	GdkRGBA      color = { 1, 0, 0, 1 };
	EvViewPrivate *priv = GET_PRIVATE (view);

	ev_page_cache_get_text_layout (priv->page_cache, page, &areas, &n_areas);
	if (!areas)
		return;

	for (i = 0; i < n_areas; i++) {
		EvRectangle  *doc_rect = areas + i;

		stroke_doc_rect (view, snapshot, &color, page, clip, doc_rect);
	}
}

static void
show_mapping_list_border (EvView        *view,
			  GtkSnapshot   *snapshot,
			  GdkRGBA       *color,
			  gint           page,
			  GdkRectangle  *clip,
			  EvMappingList *mapping_list)
{
	GList *l;

	for (l = ev_mapping_list_get_list (mapping_list); l; l = g_list_next (l)) {
		EvMapping *mapping = (EvMapping *)l->data;

		stroke_doc_rect (view, snapshot, color, page, clip, &mapping->area);
	}
}

static void
show_links_border (EvView       *view,
		   GtkSnapshot  *snapshot,
		   gint          page,
		   GdkRectangle *clip)
{
	GdkRGBA color = { 0, 0, 1, 1 };
	EvViewPrivate *priv = GET_PRIVATE (view);
	show_mapping_list_border (view, snapshot, &color, page, clip,
				  ev_page_cache_get_link_mapping (priv->page_cache, page));
}

static void
show_forms_border (EvView       *view,
		   GtkSnapshot  *snapshot,
		   gint          page,
		   GdkRectangle *clip)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkRGBA color = { 0, 1, 0, 1 };
	show_mapping_list_border (view, snapshot, &color, page, clip,
				  ev_page_cache_get_form_field_mapping (priv->page_cache, page));
}

static void
show_annots_border (EvView       *view,
		    GtkSnapshot  *snapshot,
		    gint          page,
		    GdkRectangle *clip)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkRGBA color = { 0, 1, 1, 1 };
	show_mapping_list_border (view, snapshot, &color, page, clip,
				  ev_page_cache_get_annot_mapping (priv->page_cache, page));
}

static void
show_images_border (EvView       *view,
		    GtkSnapshot  *snapshot,
		    gint          page,
		    GdkRectangle *clip)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkRGBA color = { 1, 0, 1, 1 };
	show_mapping_list_border (view, snapshot, &color, page, clip,
				  ev_page_cache_get_image_mapping (priv->page_cache, page));
}

static void
show_media_border (EvView       *view,
		   GtkSnapshot  *snapshot,
		   gint          page,
		   GdkRectangle *clip)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GdkRGBA color = { 1, 1, 0, 1 };
	show_mapping_list_border (view, snapshot, &color, page, clip,
				  ev_page_cache_get_media_mapping (priv->page_cache, page));
}


static void
show_selections_border (EvView       *view,
			GtkSnapshot  *snapshot,
			gint          page,
			GdkRectangle *clip)
{
	cairo_region_t *region;
	guint           i, n_rects;
	GdkRGBA color = { 0.75, 0.50, 0.25, 1 };
	EvViewPrivate *priv = GET_PRIVATE (view);

	region = ev_page_cache_get_text_mapping (priv->page_cache, page);
	if (!region)
		return;

	region = cairo_region_copy (region);
	n_rects = cairo_region_num_rectangles (region);
	for (i = 0; i < n_rects; i++) {
		GdkRectangle doc_rect_int;
		EvRectangle doc_rect_float;

		cairo_region_get_rectangle (region, i, &doc_rect_int);

		/* Convert the doc rect to a EvRectangle */
		doc_rect_float.x1 = doc_rect_int.x;
		doc_rect_float.y1 = doc_rect_int.y;
		doc_rect_float.x2 = doc_rect_int.x + doc_rect_int.width;
		doc_rect_float.y2 = doc_rect_int.y + doc_rect_int.height;

		stroke_doc_rect (view, snapshot, &color, page, clip, &doc_rect_float);
	}
	cairo_region_destroy (region);
}

static void
draw_debug_borders (EvView       *view,
		    GtkSnapshot  *snapshot,
		    gint          page,
		    GdkRectangle *clip)
{
	EvDebugBorders borders = ev_debug_get_debug_borders();

	if (borders & EV_DEBUG_BORDER_CHARS)
		show_chars_border (view, snapshot, page, clip);
	if (borders & EV_DEBUG_BORDER_LINKS)
		show_links_border (view, snapshot, page, clip);
	if (borders & EV_DEBUG_BORDER_FORMS)
		show_forms_border (view, snapshot, page, clip);
	if (borders & EV_DEBUG_BORDER_ANNOTS)
		show_annots_border (view, snapshot, page, clip);
	if (borders & EV_DEBUG_BORDER_IMAGES)
		show_images_border (view, snapshot, page, clip);
	if (borders & EV_DEBUG_BORDER_MEDIA)
		show_media_border (view, snapshot, page, clip);
	if (borders & EV_DEBUG_BORDER_SELECTIONS)
		show_selections_border (view, snapshot, page, clip);
}
#endif

static void ev_view_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
	int width, height;
	EvView *view = EV_VIEW (widget);
	gint         i;
	GdkRectangle clip_rect;
	GtkBorder border;
	EvViewPrivate *priv = GET_PRIVATE (view);

	width = gtk_widget_get_width(widget);
	height = gtk_widget_get_height(widget);

	gtk_snapshot_render_background (snapshot, gtk_widget_get_style_context (widget),
			       0, 0, width, height);

	clip_rect.x = 0;
	clip_rect.y = 0;
	clip_rect.width = width;
	clip_rect.height = height;

	if (priv->document == NULL)
		return;

	gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));

	compute_border (view, &border);
	for (i = priv->start_page; i >= 0 && i <= priv->end_page; i++) {
		GdkRectangle page_area;
		gboolean page_ready = TRUE;

		if (!ev_view_get_page_extents_for_border (view, i, &border, &page_area))
			continue;

		page_area.x -= priv->scroll_x;
		page_area.y -= priv->scroll_y;

		draw_one_page (view, i, snapshot, &page_area, &border, &clip_rect, &page_ready);

		if (page_ready && should_draw_caret_cursor (view, i))
			draw_caret_cursor (view, snapshot);
		if (page_ready && priv->find_pages && priv->highlight_find_results)
			highlight_find_results (view, snapshot, i);
		if (page_ready && EV_IS_DOCUMENT_ANNOTATIONS (priv->document))
			show_annotation_windows (view, i);
		if (page_ready && priv->focused_element)
			draw_focus (view, snapshot, i, &clip_rect);
		if (page_ready && priv->synctex_result)
			highlight_forward_search_results (view, snapshot, i);
#ifdef EV_ENABLE_DEBUG
		if (page_ready)
			draw_debug_borders (view, snapshot, i, &clip_rect);
#endif
	}

	/* snapshot child widgets */
	GTK_WIDGET_CLASS (ev_view_parent_class)->snapshot (widget, snapshot);

	gtk_snapshot_pop (snapshot);
}

static void
ev_view_set_focused_element_at_location (EvView *view,
					 gdouble x,
					 gdouble y)
{
	EvMapping *mapping;
	EvFormField *field;
	gint page;

	mapping = get_annotation_mapping_at_location (view, x, y, &page);
	if (mapping) {
		_ev_view_set_focused_element (view, mapping, page);
		return;
	}

	mapping = get_link_mapping_at_location (view, x, y, &page);
	if (mapping) {
		_ev_view_set_focused_element (view, mapping, page);
		return;
	}

	if ((field = ev_view_get_form_field_at_location (view, x, y))) {
		ev_view_remove_all_form_fields (view);
		_ev_view_focus_form_field (view, field);
		return;
	}

        _ev_view_set_focused_element (view, NULL, -1);
}

static gboolean
ev_view_do_popup_menu (EvView *view,
		       gdouble x,
		       gdouble y)
{
	GList        *items = NULL;
	EvLink       *link;
	EvImage      *image;
	EvAnnotation *annot;

	image = ev_view_get_image_at_location (view, x, y);
	if (image)
		items = g_list_prepend (items, image);

	link = ev_view_get_link_at_location (view, x, y);
	if (link)
		items = g_list_prepend (items, link);

	annot = ev_view_get_annotation_at_location (view, x, y);
	if (annot)
		items = g_list_prepend (items, annot);

	g_signal_emit (view, signals[SIGNAL_POPUP_MENU], 0, items, x, y);

	g_list_free (items);

	return TRUE;
}

static void
get_link_area (EvView       *view,
	       gint          x,
	       gint          y,
	       EvLink       *link,
	       GdkRectangle *area)
{
	EvMappingList *link_mapping;
	gint           page;
	gint           x_offset = 0, y_offset = 0;
	EvViewPrivate *priv = GET_PRIVATE (view);

	x += priv->scroll_x;
	y += priv->scroll_y;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	link_mapping = ev_page_cache_get_link_mapping (priv->page_cache, page);
	ev_view_get_area_from_mapping (view, page,
				       link_mapping,
				       link, area);
}

static void
get_annot_area (EvView       *view,
	       gint          x,
	       gint          y,
	       EvAnnotation *annot,
	       GdkRectangle *area)
{
	EvMappingList *annot_mapping;
	gint           page;
	gint           x_offset = 0, y_offset = 0;
	EvViewPrivate *priv = GET_PRIVATE (view);

	x += priv->scroll_x;
	y += priv->scroll_y;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	annot_mapping = ev_page_cache_get_annot_mapping (priv->page_cache, page);
	ev_view_get_area_from_mapping (view, page,
				       annot_mapping,
				       annot, area);
}

static void
get_field_area (EvView       *view,
	        gint          x,
	        gint          y,
	        EvFormField  *field,
	        GdkRectangle *area)
{
	EvMappingList *field_mapping;
	gint           page;
	gint           x_offset = 0, y_offset = 0;
	EvViewPrivate *priv = GET_PRIVATE (view);

	x += priv->scroll_x;
	y += priv->scroll_y;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	field_mapping = ev_page_cache_get_form_field_mapping (priv->page_cache, page);
	ev_view_get_area_from_mapping (view, page, field_mapping, field, area);
}


static void
link_preview_show_thumbnail (GdkTexture *page_texture,
			     EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GtkWidget       *popover = priv->link_preview.popover;
	GtkWidget       *picture;
	GtkSnapshot	*snapshot;
	gdouble          x, y;   /* position of the link on destination page */
	gint             pwidth, pheight;  /* dimensions of destination page */
	gint             vwidth, vheight;  /* dimensions of main view */
	gint             width, height;    /* dimensions of popup */
	gint             left, top;

	x = priv->link_preview.left;
	y = priv->link_preview.top;

	pwidth = gdk_texture_get_width (page_texture);
	pheight = gdk_texture_get_height (page_texture);

	vwidth = gtk_widget_get_width (GTK_WIDGET (view));
	vheight = gtk_widget_get_height (GTK_WIDGET (view));

	/* Horizontally, we try to display the full width of the destination
	 * page. This is needed to make the popup useful for two-column papers.
	 * Vertically, we limit the height to maximally LINK_PREVIEW_PAGE_RATIO
	 * of the main view. The idea is avoid the popup dominte the main view,
	 * and the reader can see context both in the popup and the main page.
	 */
	width = MIN (pwidth, vwidth);
	height = MIN (pheight, (int)(vheight * LINK_PREVIEW_PAGE_RATIO));

	/* Position on the destination page that will be in the top left
	 * corner of the popup. We choose the link destination to be centered
	 * horizontally, and slightly above the center vertically. This is a
	 * compromise given that a link contains only (x,y) information for a
	 * single point, and some links have their (x,y) point to the top left
	 * of their main content (e.g. section headers, bibliographic
	 * references, footnotes, and tables), while other links have their
	 * (x,y) point to the center right of the main contents (e.g.
	 * equations). Also, figures usually have their (x,y) point to the
	 * caption below the figure, so seeing a little of the figure above is
	 * often enough to remind the reader of the rest of the figure.
	 */
	left = x - width * LINK_PREVIEW_HORIZONTAL_LINK_POS;
	top = y - height * LINK_PREVIEW_VERTICAL_LINK_POS;

	/* link preview destination should stay within the destination page: */
	left = MIN (MAX (0, left), pwidth - width);
	top = MIN (MAX (0, top), pheight - height);

	snapshot = gtk_snapshot_new ();
	gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
	gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-left, -top));
	gtk_snapshot_append_texture (snapshot, page_texture, &GRAPHENE_RECT_INIT (0, 0, pwidth, pheight));
	gtk_snapshot_pop (snapshot);

	picture = gtk_picture_new_for_paintable (gtk_snapshot_free_to_paintable (snapshot, NULL));
	gtk_widget_set_size_request (popover, width, height);
	gtk_popover_set_child (GTK_POPOVER (popover), picture);
}

static void
link_preview_delayed_show (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	GtkWidget *popover = priv->link_preview.popover;

	gtk_popover_present (GTK_POPOVER (popover));
	gtk_popover_popup (GTK_POPOVER (popover));

	priv->link_preview.delay_timeout_id = 0;
}

static GdkTexture *
gdk_texture_new_for_surface(cairo_surface_t *surface)
{
	GdkTexture *texture;
	GBytes *bytes;

	g_return_val_if_fail(surface != NULL, NULL);
	g_return_val_if_fail(cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
	g_return_val_if_fail(cairo_image_surface_get_width(surface) > 0, NULL);
	g_return_val_if_fail(cairo_image_surface_get_height(surface) > 0, NULL);

	bytes = g_bytes_new_with_free_func(cairo_image_surface_get_data(surface),
					   cairo_image_surface_get_height(surface) * cairo_image_surface_get_stride(surface),
					   (GDestroyNotify)cairo_surface_destroy,
					   cairo_surface_reference(surface));

	texture = gdk_memory_texture_new(cairo_image_surface_get_width(surface),
					 cairo_image_surface_get_height(surface),
					 GDK_MEMORY_DEFAULT,
					 bytes,
					 cairo_image_surface_get_stride(surface));

	g_bytes_unref(bytes);

	return texture;
}

static void
link_preview_job_finished_cb (EvJobThumbnailCairo *job,
			      EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (ev_job_is_failed (EV_JOB (job))) {
		gtk_widget_unparent (priv->link_preview.popover);
		priv->link_preview.popover = NULL;
		g_object_unref (job);
		priv->link_preview.job = NULL;
		return;
	}

	link_preview_show_thumbnail (gdk_texture_new_for_surface (job->thumbnail_surface), view);

	g_object_unref (job);
	priv->link_preview.job = NULL;
}

static void
ev_view_link_preview_popover_cleanup (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (priv->link_preview.job) {
		ev_job_cancel (priv->link_preview.job);
		g_clear_object (&priv->link_preview.job);
	}

	if (priv->link_preview.popover) {
		gtk_popover_popdown (GTK_POPOVER (priv->link_preview.popover));
		g_clear_pointer (&priv->link_preview.popover, gtk_widget_unparent);
	}

	g_clear_handle_id (&priv->link_preview.delay_timeout_id, g_source_remove);
}

static gboolean
ev_view_query_tooltip (GtkWidget  *widget,
		       gint        x,
		       gint        y,
		       gboolean    keyboard_tip,
		       GtkTooltip *tooltip)
{
	EvView       *view = EV_VIEW (widget);
	EvFormField  *field;
	EvLink       *link;
	EvAnnotation *annot;
	gchar        *text;
	if (ev_view_is_loading (view))
		return FALSE;

	annot = ev_view_get_annotation_at_location (view, x, y);
	if (annot) {
		const gchar *contents;

		contents = ev_annotation_get_contents (annot);
		if (contents && *contents != '\0') {
			GdkRectangle annot_area;

			get_annot_area (view, x, y, annot, &annot_area);
			gtk_tooltip_set_text (tooltip, contents);
			gtk_tooltip_set_tip_area (tooltip, &annot_area);

			return TRUE;
		}
	}

	field = ev_view_get_form_field_at_location (view, x, y);
	if (field != NULL) {
		gchar *alt_ui_name = ev_form_field_get_alternate_name (field);

		if (alt_ui_name && *(alt_ui_name) != '\0') {
			GdkRectangle field_area;

			get_field_area (view, x, y, field, &field_area);
			gtk_tooltip_set_text (tooltip, alt_ui_name);
			gtk_tooltip_set_tip_area (tooltip, &field_area);

			return TRUE;
		}
	}

	link = ev_view_get_link_at_location (view, x, y);
	if (!link)
		return FALSE;

	text = tip_from_link (view, link);
	if (text && g_utf8_validate (text, -1, NULL)) {
		GdkRectangle link_area;

		get_link_area (view, x, y, link, &link_area);
		gtk_tooltip_set_text (tooltip, text);
		gtk_tooltip_set_tip_area (tooltip, &link_area);
		g_free (text);

		return TRUE;
	}
	g_free (text);

	return FALSE;
}

static void
start_selection_for_event (EvView	*view,
			   gdouble	 x,
			   gdouble	 y,
			   gint		 n_press)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	clear_selection (view);

	priv->selection_info.in_select = TRUE;
	priv->selection_info.start.x = x + priv->scroll_x;
	priv->selection_info.start.y = y + priv->scroll_y;

	switch (n_press) {
	        case 2:
			priv->selection_info.style = EV_SELECTION_STYLE_WORD;
			break;
	        case 3:
			priv->selection_info.style = EV_SELECTION_STYLE_LINE;
			break;
	        default:
			priv->selection_info.style = EV_SELECTION_STYLE_GLYPH;
			return;
	}

	/* In case of WORD or LINE, compute selections now */
	compute_selections (view,
			    priv->selection_info.style,
			    &(priv->selection_info.start),
			    &(priv->selection_info.start));
}

gint
_ev_view_get_caret_cursor_offset_at_doc_point (EvView *view,
					       gint    page,
					       gdouble doc_x,
					       gdouble doc_y)
{
	EvRectangle *areas = NULL;
	guint        n_areas = 0;
	gint         offset = -1;
	gint         first_line_offset;
	gint         last_line_offset = -1;
	EvRectangle *rect;
	guint        i;
	EvViewPrivate *priv = GET_PRIVATE (view);

	ev_page_cache_get_text_layout (priv->page_cache, page, &areas, &n_areas);
	if (!areas)
		return -1;

	i = 0;
	while (i < n_areas && offset == -1) {
		rect = areas + i;

		first_line_offset = -1;
		while (doc_y >= rect->y1 && doc_y <= rect->y2) {
			if (first_line_offset == -1) {
				if (doc_x <= rect->x1) {
					/* Location is before the start of the line */
					if (last_line_offset != -1) {
						EvRectangle *last = areas + last_line_offset;
						gint         dx1, dx2;

						/* If there's a previous line, check distances */

						dx1 = doc_x - last->x2;
						dx2 = rect->x1 - doc_x;

						if (dx1 < dx2)
							offset = last_line_offset;
						else
							offset = i;
					} else {
						offset = i;
					}

					last_line_offset = i + 1;
					break;
				}
				first_line_offset = i;
			}
			last_line_offset = i + 1;

			if (doc_x >= rect->x1 && doc_x <= rect->x2) {
				/* Location is inside the line. Position the caret before
				 * or after the character, depending on whether the point
				 * falls within the left or right half of the bounding box.
				 */
				if (doc_x <= rect->x1 + (rect->x2 - rect->x1) / 2)
					offset = i;
				else
					offset = i + 1;
				break;
			}

			i++;
			rect = areas + i;
		}

		if (first_line_offset == -1)
			i++;
	}

	if (last_line_offset == -1)
		return -1;

	if (offset == -1)
		offset = last_line_offset;

	return offset;
}

static gboolean
position_caret_cursor_at_doc_point (EvView *view,
				    gint    page,
				    gdouble doc_x,
				    gdouble doc_y)
{
	gint offset;
	EvViewPrivate *priv = GET_PRIVATE (view);

	offset = _ev_view_get_caret_cursor_offset_at_doc_point (view, page, doc_x, doc_y);
	if (offset == -1)
		return FALSE;

	if (priv->cursor_offset != offset || priv->cursor_page != page) {
		priv->cursor_offset = offset;
		priv->cursor_page = page;

		return TRUE;
	}

	return FALSE;
}

static gboolean
position_caret_cursor_at_location (EvView *view,
				   gdouble x,
				   gdouble y)
{
	gint page;
	gint doc_x, doc_y;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->caret_enabled || priv->rotation != 0)
		return FALSE;

	if (!priv->page_cache)
		return FALSE;

	/* Get the offset from the doc point */
	if (!get_doc_point_from_location (view, x, y, &page, &doc_x, &doc_y))
		return FALSE;

	return position_caret_cursor_at_doc_point (view, page, doc_x, doc_y);
}

static gboolean
position_caret_cursor_for_event (EvView         *view,
				 gdouble	 x,
				 gdouble	 y,
				 gboolean        redraw)
{
	GdkRectangle area;
	GdkRectangle prev_area = { 0, 0, 0, 0 };
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (redraw)
		get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &prev_area);

	if (!position_caret_cursor_at_location (view, x, y))
		return FALSE;

	if (!get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &area))
		return FALSE;

	priv->cursor_line_offset = area.x;

	g_signal_emit (view, signals[SIGNAL_CURSOR_MOVED], 0, priv->cursor_page, priv->cursor_offset);

	if (redraw) {
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}

	return TRUE;
}

static void
ev_view_button_press_event (GtkGestureClick	*gesture,
			    int			 n_press,
			    double		 x,
			    double		 y,
			    gpointer		 user_data)
{
	GtkEventController *controller = GTK_EVENT_CONTROLLER (gesture);
	GtkWidget *widget = gtk_event_controller_get_widget (controller);
	GdkEvent *event = gtk_event_controller_get_current_event (controller);
	guint button;
	GdkModifierType state = gtk_event_controller_get_current_event_state (controller);

	EvView *view = EV_VIEW (widget);
	EvViewPrivate *priv = GET_PRIVATE (view);

	ev_view_link_preview_popover_cleanup (view);

	if (!priv->document || ev_document_get_n_pages (priv->document) <= 0)
		return;

	if (gtk_gesture_is_recognized (priv->zoom_gesture))
		return;

	if (gtk_gesture_is_recognized (priv->pan_gesture))
		return;

	if (!gtk_widget_has_focus (widget)) {
		gtk_widget_grab_focus (widget);
	}

	button = gdk_button_event_get_button (event);

	priv->pressed_button = button;
	priv->selection_info.in_drag = FALSE;
	priv->selection_info.in_select = FALSE;

	if (priv->scroll_info.autoscrolling)
		return;

	if (priv->adding_annot_info.adding_annot && !priv->adding_annot_info.annot) {
		if (button != GDK_BUTTON_PRIMARY)
			return;

		priv->adding_annot_info.start.x = x + priv->scroll_x;
		priv->adding_annot_info.start.y = y + priv->scroll_y;
		priv->adding_annot_info.stop = priv->adding_annot_info.start;
		ev_view_create_annotation (view);

		return;
	}

	switch (button) {
	        case GDK_BUTTON_PRIMARY: {
			EvImage *image;
			EvAnnotation *annot;
			EvFormField *field;
			EvMapping *link;
			EvMedia *media;
			gint page;

			if (state & GDK_CONTROL_MASK) {
				ev_view_synctex_backward_search (view, x , y);
				return;
			}

			if (EV_IS_SELECTION (priv->document) && priv->selection_info.selections) {
				if (n_press == 3) {
					start_selection_for_event (view, x, y, n_press);
				} else if (state & GDK_SHIFT_MASK) {
					GdkPoint end_point;

					end_point.x = x + priv->scroll_x;
					end_point.y = y + priv->scroll_y;
					extend_selection (view, &priv->selection_info.start, &end_point);
				} else if (location_in_selected_text (view,
							       x + priv->scroll_x,
							       y + priv->scroll_y)) {
					priv->selection_info.in_drag = TRUE;
				} else {
					start_selection_for_event (view, x, y, n_press);
					if (position_caret_cursor_for_event (view, x, y, TRUE)) {
						priv->cursor_blink_time = 0;
						ev_view_pend_cursor_blink (view);
					}
				}
			} else if ((media = ev_view_get_media_at_location (view, x, y))) {
				ev_view_handle_media (view, media);
			} else if ((annot = ev_view_get_annotation_at_location (view, x, y))) {
				if (EV_IS_ANNOTATION_TEXT (annot)) {
					EvRectangle  current_area;
					GdkPoint     view_point;
					EvPoint      doc_point;
					GdkRectangle page_area;
					GtkBorder    border;
					guint        annot_page;

					/* annot_clicked remembers that we clicked
					 * on an annotation. We need moving_annot
					 * to distinguish moving an annotation from
					 * showing its popup upon button release. */
					priv->moving_annot_info.annot_clicked = TRUE;
					priv->moving_annot_info.moving_annot = FALSE;
					priv->moving_annot_info.annot = annot;
					ev_annotation_get_area (annot, &current_area);

					view_point.x = x + priv->scroll_x;
					view_point.y = y + priv->scroll_y;

					/* Remember the coordinates of the button press event
					 * in order to implement a minimum threshold for moving
					 * annotations. */
					priv->moving_annot_info.start = view_point;
					annot_page = ev_annotation_get_page_index (annot);
					ev_view_get_page_extents (view, annot_page, &page_area, &border);
					_ev_view_transform_view_point_to_doc_point (view, &view_point,
										    &page_area, &border,
										    &doc_point.x, &doc_point.y);

					/* Remember the offset of the cursor with respect to
					 * the annotation area in order to prevent the annotation from
					 * jumping under the cursor while moving it. */
					priv->moving_annot_info.cursor_offset.x = doc_point.x - current_area.x1;
					priv->moving_annot_info.cursor_offset.y = doc_point.y - current_area.y1;
				}
			} else if ((field = ev_view_get_form_field_at_location (view, x, y))) {
				ev_view_remove_all_form_fields (view);
				ev_view_handle_form_field (view, field);
			} else if ((link = get_link_mapping_at_location (view, x, y, &page))){
				_ev_view_set_focused_element (view, link, page);
			} else if (!location_in_text (view, x + priv->scroll_x, y + priv->scroll_y) &&
				   (image = ev_view_get_image_at_location (view, x, y))) {
				if (priv->image_dnd_info.image)
					g_object_unref (priv->image_dnd_info.image);
				priv->image_dnd_info.image = g_object_ref (image);
				priv->image_dnd_info.in_drag = TRUE;

				priv->image_dnd_info.start.x = x + priv->scroll_x;
				priv->image_dnd_info.start.y = y + priv->scroll_y;
			} else {
				ev_view_remove_all_form_fields (view);
				_ev_view_set_focused_element (view, NULL, -1);

				if (priv->synctex_result) {
					g_clear_pointer (&priv->synctex_result, g_free);
					gtk_widget_queue_draw (widget);
				}

				if (EV_IS_SELECTION (priv->document))
					start_selection_for_event (view, x, y, n_press);

				if (position_caret_cursor_for_event (view, x, y, TRUE)) {
					priv->cursor_blink_time = 0;
					ev_view_pend_cursor_blink (view);
				}
			}
		}
			return;
		case GDK_BUTTON_MIDDLE:
			ev_view_set_focused_element_at_location (view, x, y);
			return;
		case GDK_BUTTON_SECONDARY:
			priv->scroll_info.start_y = y;
			ev_view_set_focused_element_at_location (view, x, y);
			ev_view_do_popup_menu (view, x, y);
	}
}

static gboolean
ev_view_drag_update_momentum (EvView *view)
{
	int i;
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->drag_info.in_drag)
		return G_SOURCE_REMOVE;

	for (i = DRAG_HISTORY - 1; i > 0; i--) {
		priv->drag_info.buffer[i].x = priv->drag_info.buffer[i-1].x;
		priv->drag_info.buffer[i].y = priv->drag_info.buffer[i-1].y;
	}

	/* Momentum is a moving average of 10ms granularity over
	 * the last 100ms with each 10ms stored in buffer.
	 */

	priv->drag_info.momentum.x = (priv->drag_info.buffer[DRAG_HISTORY - 1].x - priv->drag_info.buffer[0].x);
	priv->drag_info.momentum.y = (priv->drag_info.buffer[DRAG_HISTORY - 1].y - priv->drag_info.buffer[0].y);

	return G_SOURCE_CONTINUE;
}

static gboolean
ev_view_scroll_drag_release (EvView *view)
{
	gdouble dhadj_value, dvadj_value;
	gdouble oldhadjustment, oldvadjustment;
	gdouble h_page_size, v_page_size;
	gdouble h_upper, v_upper;
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->drag_info.momentum.x /= 1.2;
	priv->drag_info.momentum.y /= 1.2; /* Alter these constants to change "friction" */

	h_page_size = gtk_adjustment_get_page_size (priv->hadjustment);
	v_page_size = gtk_adjustment_get_page_size (priv->vadjustment);

	dhadj_value = h_page_size *
		      (gdouble)priv->drag_info.momentum.x / gtk_widget_get_width (GTK_WIDGET (view));
	dvadj_value = v_page_size *
		      (gdouble)priv->drag_info.momentum.y / gtk_widget_get_height (GTK_WIDGET (view));

	oldhadjustment = gtk_adjustment_get_value (priv->hadjustment);
	oldvadjustment = gtk_adjustment_get_value (priv->vadjustment);

	h_upper = gtk_adjustment_get_upper (priv->hadjustment);
	v_upper = gtk_adjustment_get_upper (priv->vadjustment);

	/* When we reach the edges, we need either to absorb some momentum and bounce by
	 * multiplying it on -0.5 or stop scrolling by setting momentum to 0. */
	if (((oldhadjustment + dhadj_value) > (h_upper - h_page_size)) ||
	    ((oldhadjustment + dhadj_value) < 0))
		priv->drag_info.momentum.x = 0;
	if (((oldvadjustment + dvadj_value) > (v_upper - v_page_size)) ||
	    ((oldvadjustment + dvadj_value) < 0))
		priv->drag_info.momentum.y = 0;

	gtk_adjustment_set_value (priv->hadjustment,
				  MIN (oldhadjustment + dhadj_value,
				       h_upper - h_page_size));
	gtk_adjustment_set_value (priv->vadjustment,
				  MIN (oldvadjustment + dvadj_value,
				       v_upper - v_page_size));

	if (((priv->drag_info.momentum.x < 1) && (priv->drag_info.momentum.x > -1)) &&
	    ((priv->drag_info.momentum.y < 1) && (priv->drag_info.momentum.y > -1)))
		return G_SOURCE_REMOVE;
	else
		return G_SOURCE_CONTINUE;
}

static void
middle_clicked_drag_begin_cb (GtkGestureDrag	*self,
			      gdouble		 start_x,
			      gdouble		 start_y,
			      EvView		*view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	/* use root coordinates as reference point because
	 * scrolling changes window relative coordinates */
	priv->drag_info.hadj = gtk_adjustment_get_value (priv->hadjustment);
	priv->drag_info.vadj = gtk_adjustment_get_value (priv->vadjustment);

	ev_view_set_cursor (view, EV_VIEW_CURSOR_DRAG);

	priv->drag_info.drag_timeout_id = g_timeout_add (10,
				(GSourceFunc)ev_view_drag_update_momentum, view);
	/* Set 100 to choose how long it takes to build up momentum */
	/* Clear out previous momentum info: */
	for (int i = 0; i < DRAG_HISTORY; i++) {
		priv->drag_info.buffer[i].x = start_x;
		priv->drag_info.buffer[i].y = start_y;
	}
	priv->drag_info.momentum.x = 0;
	priv->drag_info.momentum.y = 0;

	priv->drag_info.in_drag = TRUE;
}

static void
middle_clicked_drag_end_cb (GtkGestureDrag	*self,
			    gdouble		 offset_x,
			    gdouble		 offset_y,
			    EvView		*view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	priv->drag_info.release_timeout_id =
			g_timeout_add (20, (GSourceFunc)ev_view_scroll_drag_release, view);

	priv->drag_info.in_drag = FALSE;
}

static void
middle_clicked_drag_update_cb (GtkGestureDrag	*self,
			       gdouble		 offset_x,
			       gdouble		 offset_y,
			       EvView		*view)
{
	GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (self));
	gdouble dhadj_value, dvadj_value;
	EvViewPrivate *priv = GET_PRIVATE (view);

	dhadj_value = gtk_adjustment_get_page_size (priv->hadjustment) *
				      offset_x / gtk_widget_get_width (GTK_WIDGET (view));
	dvadj_value = gtk_adjustment_get_page_size (priv->vadjustment) *
				      offset_y / gtk_widget_get_height (GTK_WIDGET (view));

	/* We will update the drag event's start position if
	 * the adjustment value is changed, but only if the
	 * change was not caused by this function. */

	priv->drag_info.in_notify = TRUE;

	/* clamp scrolling to visible area */
	gtk_adjustment_set_value (priv->hadjustment, MIN (priv->drag_info.hadj - dhadj_value,
					gtk_adjustment_get_upper (priv->hadjustment) -
					gtk_adjustment_get_page_size (priv->hadjustment)));
	gtk_adjustment_set_value (priv->vadjustment, MIN (priv->drag_info.vadj - dvadj_value,
					gtk_adjustment_get_upper (priv->vadjustment) -
					gtk_adjustment_get_page_size (priv->vadjustment)));

	priv->drag_info.in_notify = FALSE;

	gdouble x, y;

	gdk_event_get_axis (event, GDK_AXIS_X, &x);
	gdk_event_get_axis (event, GDK_AXIS_Y, &y);

	priv->drag_info.buffer[0].x = x;
	priv->drag_info.buffer[0].y = y;
}

static void
ev_view_remove_all_form_fields (EvView *view)
{
	GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (view));

	while (child != NULL) {
		GtkWidget *next = gtk_widget_get_next_sibling (child);

		if (g_object_get_data (G_OBJECT (child), "form-field"))
			gtk_widget_unparent (child);

		child = next;
	}
}

static void
ev_view_remove_all (EvView *view)
{
	GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (view));

	while (child != NULL) {
		GtkWidget *next = gtk_widget_get_next_sibling (child);

		gtk_widget_unparent (child);

		child = next;
	}
}

/*** Drag and Drop ***/
static GdkContentProvider *
drag_prepare_cb (GtkDragSource	*self,
		 gdouble	 x,
		 gdouble	 y,
		 EvView		*view)
{
	EvImage *image;
	GdkPixbuf *pixbuf;
	const char *tmp_uri;
	GFile *file;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->selection_info.in_select)
		return NULL;

	if (EV_IS_SELECTION (priv->document) && priv->selection_info.in_drag &&
	    location_in_selected_text (view, x + priv->scroll_x, y + priv->scroll_y)) {
		gchar *text = get_selected_text (view);

		return gdk_content_provider_new_for_bytes ("text/plain",
				g_bytes_new_take (text, strlen (text)));
	}

	if (!location_in_text (view, x + priv->scroll_x, y + priv->scroll_y) &&
				   (image = ev_view_get_image_at_location (view, x, y))) {
		ev_document_doc_mutex_lock ();
		pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (priv->document), image);
		ev_document_doc_mutex_unlock ();

		tmp_uri = ev_image_save_tmp (image, pixbuf);
		file = g_file_new_for_uri (tmp_uri);

		return gdk_content_provider_new_union ((GdkContentProvider *[2]) {
				gdk_content_provider_new_typed (G_TYPE_FILE, file),
				gdk_content_provider_new_typed (GDK_TYPE_PIXBUF, pixbuf),
				}, 2);
	}

	return NULL;
}

static void
selection_update_idle_cb (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	compute_selections (view,
			    priv->selection_info.style,
			    &priv->selection_info.start,
			    &priv->motion);
	priv->selection_update_id = 0;
}

static gboolean
selection_scroll_timeout_cb (EvView *view)
{
	gint x, y, shift_x = 0, shift_y = 0;
	GtkWidget *widget = GTK_WIDGET (view);
	EvViewPrivate *priv = GET_PRIVATE (view);
	int widget_width = gtk_widget_get_width (widget);
	int widget_height = gtk_widget_get_height (widget);

	if (!ev_document_misc_get_pointer_position_impl (widget, &x, &y))
		return G_SOURCE_CONTINUE;

	if ((y + SCROLL_SENSITIVITY_THRESHOLD) > widget_height) {
		shift_y = (y + SCROLL_SENSITIVITY_THRESHOLD - widget_height) / 2;
	} else if (y < SCROLL_SENSITIVITY_THRESHOLD) {
		shift_y = (y - SCROLL_SENSITIVITY_THRESHOLD) / 2;
	}

	if (shift_y)
		gtk_adjustment_set_value (priv->vadjustment,
					  CLAMP (gtk_adjustment_get_value (priv->vadjustment) + shift_y,
						 gtk_adjustment_get_lower (priv->vadjustment),
						 gtk_adjustment_get_upper (priv->vadjustment) -
						 gtk_adjustment_get_page_size (priv->vadjustment)));

	if ((x + SCROLL_SENSITIVITY_THRESHOLD) > widget_width) {
		shift_x = (x + SCROLL_SENSITIVITY_THRESHOLD - widget_width) / 2;
	} else if (x < SCROLL_SENSITIVITY_THRESHOLD) {
		shift_x = (x - SCROLL_SENSITIVITY_THRESHOLD) / 2;
	}

	if (shift_x)
		gtk_adjustment_set_value (priv->hadjustment,
					  CLAMP (gtk_adjustment_get_value (priv->hadjustment) + shift_x,
						 gtk_adjustment_get_lower (priv->hadjustment),
						 gtk_adjustment_get_upper (priv->hadjustment) -
						 gtk_adjustment_get_page_size (priv->hadjustment)));

	return TRUE;
}

static void
ev_view_motion_notify_event (GtkEventControllerMotion	*self,
			     gdouble			 x,
			     gdouble			 y,
			     gpointer			 user_data)
{
	EvView    *view = EV_VIEW (user_data);
	GtkWidget *widget = GTK_WIDGET (view);
	GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (self));
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return;

	if (gtk_gesture_is_recognized (priv->zoom_gesture))
		return;

	if (priv->scroll_info.autoscrolling) {
		if (y >= 0)
			priv->scroll_info.last_y = y;
		return;
	}

	switch (priv->pressed_button) {
	case GDK_BUTTON_PRIMARY:
		/* For the Evince 0.4.x release, we limit selection to un-rotated
		 * documents only.
		 */
		if (priv->rotation != 0)
			return;

		if (priv->adding_annot_info.adding_annot) {
			EvRectangle  rect;
			EvRectangle  current_area;
			EvPoint      start;
			EvPoint      end;
			GdkRectangle page_area;
			GtkBorder    border;
			guint        annot_page;

			if (!priv->adding_annot_info.annot)
				return;

			ev_annotation_get_area (priv->adding_annot_info.annot, &current_area);

			priv->adding_annot_info.stop.x = x + priv->scroll_x;
			priv->adding_annot_info.stop.y = y + priv->scroll_y;
			annot_page = ev_annotation_get_page_index (priv->adding_annot_info.annot);
			ev_view_get_page_extents (view, annot_page, &page_area, &border);
			_ev_view_transform_view_point_to_doc_point (view, &priv->adding_annot_info.start, &page_area, &border,
								    &start.x, &start.y);
			_ev_view_transform_view_point_to_doc_point (view, &priv->adding_annot_info.stop, &page_area, &border,
								    &end.x, &end.y);

			switch (priv->adding_annot_info.type) {
			case EV_ANNOTATION_TYPE_TEXT:
				rect.x1 = end.x;
				rect.y1 = end.y;
				rect.x2 = rect.x1 + current_area.x2 - current_area.x1;
				rect.y2 = rect.y1 + current_area.y2 - current_area.y1;
				break;
			case EV_ANNOTATION_TYPE_TEXT_MARKUP:
				rect.x1 = start.x;
				rect.y1 = start.y;
				rect.x2 = end.x;
				rect.y2 = end.y;
				break;
			default:
				g_assert_not_reached ();
			}

			/* Take the mutex before set_area, because the notify signal
			 * updates the mappings in the backend */
			ev_document_doc_mutex_lock ();
			if (ev_annotation_set_area (priv->adding_annot_info.annot, &rect)) {
				ev_document_annotations_save_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
									 priv->adding_annot_info.annot,
									 EV_ANNOTATIONS_SAVE_AREA);
			}
			ev_document_doc_mutex_unlock ();


			/* FIXME: reload only annotation area */
			ev_view_reload_page (view, annot_page, NULL);
		} else if (priv->moving_annot_info.annot_clicked) {
			EvRectangle  rect;
			EvRectangle  current_area;
			GdkPoint     view_point;
			EvPoint      doc_point;
			GdkRectangle page_area;
			GtkBorder    border;
			guint        annot_page;
			double       page_width;
			double       page_height;

			if (!priv->moving_annot_info.annot)
				return;

			view_point.x = x + priv->scroll_x;
			view_point.y = y + priv->scroll_y;

			if (!priv->moving_annot_info.moving_annot) {
				/* Only move the annotation if the threshold is exceeded */
				if (!gtk_drag_check_threshold (widget,
							       priv->moving_annot_info.start.x,
							       priv->moving_annot_info.start.y,
							       view_point.x,
							       view_point.y))
					return;
				priv->moving_annot_info.moving_annot = TRUE;
			}

			ev_annotation_get_area (priv->moving_annot_info.annot, &current_area);
			annot_page = ev_annotation_get_page_index (priv->moving_annot_info.annot);
			ev_view_get_page_extents (view, annot_page, &page_area, &border);
			_ev_view_transform_view_point_to_doc_point (view, &view_point, &page_area, &border,
								    &doc_point.x, &doc_point.y);

			ev_document_get_page_size (priv->document, annot_page, &page_width, &page_height);

			rect.x1 = MAX (0, doc_point.x - priv->moving_annot_info.cursor_offset.x);
			rect.y1 = MAX (0, doc_point.y - priv->moving_annot_info.cursor_offset.y);
			rect.x2 = rect.x1 + current_area.x2 - current_area.x1;
			rect.y2 = rect.y1 + current_area.y2 - current_area.y1;

			/* Prevent the annotation from being moved off the page */
			if (rect.x2 > page_width) {
				rect.x2 = page_width;
				rect.x1 = page_width - current_area.x2 + current_area.x1;
			}
			if (rect.y2 > page_height) {
				rect.y2 = page_height;
				rect.y1 = page_height - current_area.y2 + current_area.y1;
			}

			/* Take the mutex before set_area, because the notify signal
			 * updates the mappings in the backend */
			ev_document_doc_mutex_lock ();
			if (ev_annotation_set_area (priv->moving_annot_info.annot, &rect)) {
				ev_document_annotations_save_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
									 priv->moving_annot_info.annot,
									 EV_ANNOTATIONS_SAVE_AREA);
			}
			ev_document_doc_mutex_unlock ();

			/* FIXME: reload only annotation area */
			ev_view_reload_page (view, annot_page, NULL);
		} else if (ev_document_has_synctex (priv->document) && (state & GDK_CONTROL_MASK)) {
			/* Ignore spurious motion event triggered by slightly moving mouse
			 * while clicking for launching synctex. Issue #951 */
			return;
		} else {
			/* Schedule timeout to scroll during selection and additionally
			 * scroll once to allow arbitrary speed. */
			if (!priv->selection_scroll_id)
				priv->selection_scroll_id = g_timeout_add (SCROLL_TIME,
									   (GSourceFunc)selection_scroll_timeout_cb,
									   view);
			else
				selection_scroll_timeout_cb (view);

			priv->motion.x = x + priv->scroll_x;
			priv->motion.y = y + priv->scroll_y;

			/* Queue an idle to handle the motion.  We do this because
			 * handling any selection events in the motion could be slower
			 * than new motion events reach us.  We always put it in the
			 * idle to make sure we catch up and don't visibly lag the
			 * mouse. */
			if (priv->selection_info.in_select && !priv->selection_update_id)
				priv->selection_update_id =
					g_idle_add_once ((GSourceOnceFunc)selection_update_idle_cb,
							 view);
		}

		return;
	case GDK_BUTTON_MIDDLE:
		break;
	default:
		ev_view_handle_cursor_over_xy (view, x, y, TRUE);
	}
}

/**
 * ev_view_get_selected_text:
 * @view: #EvView instance
 *
 * Returns a pointer to a constant string containing the selected
 * text in the view.
 *
 * The value returned may be NULL if there is no selected text.
 *
 * Returns: The string representing selected text.
 *
 * Since: 3.30
 */
char *
ev_view_get_selected_text (EvView *view)
{
	return get_selected_text (view);
}

/**
 * ev_view_add_text_markup_annotation_for_selected_text:
 * @view: #EvView instance
 *
 * Adds a Text Markup annotation (defaulting to a 'highlight' one) to
 * the currently selected text on the document.
 *
 * When the selected text spans more than one page, it will add a
 * corresponding annotation for each page that contains selected text.
 *
 * Returns: %TRUE if annotations were added successfully, %FALSE otherwise.
 *
 * Since: 3.30
 */
gboolean
ev_view_add_text_markup_annotation_for_selected_text (EvView  *view)
{
	GList *l;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->adding_annot_info.annot || priv->adding_annot_info.adding_annot ||
	    !ev_view_has_selection (view))
		return FALSE;

	for (l = priv->selection_info.selections; l != NULL; l = l->next) {
		EvViewSelection *selection = (EvViewSelection *)l->data;

		priv->adding_annot_info.adding_annot = TRUE;
		priv->adding_annot_info.type = EV_ANNOTATION_TYPE_TEXT_MARKUP;

		ev_view_create_annotation_from_selection (view, selection);

		if (priv->adding_annot_info.adding_annot)
			g_signal_emit (view, signals[SIGNAL_ANNOT_ADDED], 0, priv->adding_annot_info.annot);
	}

	clear_selection (view);

	priv->adding_annot_info.adding_annot = FALSE;
	priv->adding_annot_info.annot = NULL;

	return TRUE;
}

void
ev_view_set_enable_spellchecking (EvView *view,
                                  gboolean enabled)
{
        EvMappingList *annots;
        GList         *l;
        gint           n_pages = 0;
        gint           current_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

        g_return_if_fail (EV_IS_VIEW (view));

        priv->enable_spellchecking = enabled;

        if (priv->document)
                n_pages = ev_document_get_n_pages (priv->document);

        for (current_page = 0; current_page < n_pages; current_page++) {
                annots = ev_page_cache_get_annot_mapping (priv->page_cache, current_page);

                for (l = ev_mapping_list_get_list (annots); l && l->data; l = g_list_next (l)) {
                        EvAnnotation      *annot;
                        GtkWidget         *window;

                        annot = ((EvMapping *)(l->data))->data;

                        if (!EV_IS_ANNOTATION_MARKUP (annot))
                                continue;

                        window = get_window_for_annot (view, annot);

                        if (window) {
                                ev_annotation_window_set_enable_spellchecking (EV_ANNOTATION_WINDOW (window), priv->enable_spellchecking);
                        }
                }
        }
}

gboolean
ev_view_get_enable_spellchecking (EvView *view)
{
        g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

        return FALSE;
}

static void
ev_view_button_release_event(GtkGestureClick		*self,
			     gint 			 n_press,
			     gdouble			 x,
			     gdouble			 y,
			     gpointer			 user_data)
{
	EvView *view = EV_VIEW (user_data);
	GtkEventController *controller = GTK_EVENT_CONTROLLER (self);
	GdkEvent *event = gtk_event_controller_get_current_event (controller);
	GdkModifierType state = gtk_event_controller_get_current_event_state (controller);
	guint32 time = gtk_event_controller_get_current_event_time (controller);
	EvLink *link = NULL;
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->image_dnd_info.in_drag = FALSE;
	priv->selection_info.in_select = FALSE;

	if (gtk_gesture_is_recognized (priv->zoom_gesture))
		return;

	if (gtk_gesture_is_recognized (priv->pan_gesture))
		return;

	if (priv->scroll_info.autoscrolling) {
		ev_view_autoscroll_stop (view);
		priv->pressed_button = -1;

		return;
	}

	if (priv->pressed_button == GDK_BUTTON_PRIMARY && state & GDK_CONTROL_MASK) {
		priv->pressed_button = -1;
		return;
	}

	if (priv->document && !priv->drag_info.in_drag &&
	    (priv->pressed_button == GDK_BUTTON_PRIMARY ||
	     priv->pressed_button == GDK_BUTTON_MIDDLE)) {
		link = ev_view_get_link_at_location (view, x, y);
	}

	priv->drag_info.in_drag = FALSE;

	if (priv->adding_annot_info.adding_annot && !priv->selection_scroll_id) {
		gboolean annot_added = TRUE;

		/* We ignore right-click buttons while in annotation add mode */
		if (priv->pressed_button != GDK_BUTTON_PRIMARY)
			return;
		g_assert (priv->adding_annot_info.annot);

		if (EV_IS_ANNOTATION_MARKUP (priv->adding_annot_info.annot)) {
			EvRectangle area;
			EvRectangle popup_rect;

			ev_annotation_get_area (priv->adding_annot_info.annot, &area);

			if (area.x1 == 0 && area.y1 == 0 && area.x2 == 0 && area.y2 == 0) {
				/* Do not create empty annots */
				annot_added = FALSE;

				ev_document_doc_mutex_lock ();
				ev_document_annotations_remove_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
									   priv->adding_annot_info.annot);
				ev_document_doc_mutex_unlock ();

				ev_page_cache_mark_dirty (priv->page_cache,
							  ev_annotation_get_page_index (priv->adding_annot_info.annot),
							  EV_PAGE_DATA_INCLUDE_ANNOTS);
			} else {
				popup_rect.x1 = area.x2;
				popup_rect.x2 = popup_rect.x1 + ANNOT_POPUP_WINDOW_DEFAULT_WIDTH;
				popup_rect.y1 = area.y2;
				popup_rect.y2 = popup_rect.y1 + ANNOT_POPUP_WINDOW_DEFAULT_HEIGHT;

				if (ev_annotation_markup_set_rectangle (EV_ANNOTATION_MARKUP (priv->adding_annot_info.annot),
									&popup_rect)) {
					ev_document_doc_mutex_lock ();
					ev_document_annotations_save_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
										 priv->adding_annot_info.annot,
										 EV_ANNOTATIONS_SAVE_POPUP_RECT);
					ev_document_doc_mutex_unlock ();
				}
			}
		}

		if (priv->adding_annot_info.type == EV_ANNOTATION_TYPE_TEXT)
			ev_view_annotation_create_show_popup_window (view, priv->adding_annot_info.annot);

		priv->adding_annot_info.stop.x = x + priv->scroll_x;
		priv->adding_annot_info.stop.y = y + priv->scroll_y;
		if (annot_added)
			g_signal_emit (view, signals[SIGNAL_ANNOT_ADDED], 0, priv->adding_annot_info.annot);
		else
			g_signal_emit (view, signals[SIGNAL_ANNOT_CANCEL_ADD], 0, NULL);

		priv->adding_annot_info.adding_annot = FALSE;
		priv->adding_annot_info.annot = NULL;
		ev_view_handle_cursor_over_xy (view, x, y, FALSE);
		priv->pressed_button = -1;

		return;
	}

	if (priv->moving_annot_info.annot_clicked) {
		if (priv->moving_annot_info.moving_annot)
			ev_view_handle_cursor_over_xy (view, x, y, FALSE);
		else
			ev_view_handle_annotation (view, priv->moving_annot_info.annot, x, y, time);

		priv->moving_annot_info.annot_clicked = FALSE;
		priv->moving_annot_info.moving_annot = FALSE;
		priv->moving_annot_info.annot = NULL;
		priv->pressed_button = -1;

		return;
	}

	if (priv->pressed_button == GDK_BUTTON_PRIMARY) {
		EvAnnotation *annot = ev_view_get_annotation_at_location (view, x, y);

		if (annot)
			ev_view_handle_annotation (view, annot, x, y, time);
	}

	if (priv->pressed_button == GDK_BUTTON_MIDDLE) {
		ev_view_handle_cursor_over_xy (view, x, y, FALSE);
	}

	priv->pressed_button = -1;

	g_clear_handle_id (&priv->selection_scroll_id, g_source_remove);
	g_clear_handle_id (&priv->selection_update_id, g_source_remove);

	if (priv->selection_info.selections) {
		g_clear_object (&priv->link_selected);

		position_caret_cursor_for_event (view, x, y, FALSE);

		if (priv->selection_info.in_drag)
			clear_selection (view);
		priv->selection_info.in_drag = FALSE;
	} else if (link) {
		if (gdk_button_event_get_button (event) == GDK_BUTTON_MIDDLE) {
			EvLinkAction    *action;
			EvLinkActionType type;

			action = ev_link_get_action (link);
			if (!action)
				return;

			type = ev_link_action_get_action_type (action);
			if (type == EV_LINK_ACTION_TYPE_GOTO_DEST) {
				g_signal_emit (view,
					       signals[SIGNAL_EXTERNAL_LINK],
					       0, action);
			}
		} else {
			ev_view_handle_link (view, link);
		}
	}
}

static gint
go_to_next_page (EvView *view,
		 gint    page)
{
	int      n_pages;
	gboolean dual_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return -1;

	n_pages = ev_document_get_n_pages (priv->document);

	dual_page = is_dual_page (view, NULL);
	page += dual_page ? 2 : 1;

	if (page < n_pages)
		return page;

	if (dual_page && page == n_pages)
		return page - 1;

	return -1;
}

static gint
go_to_previous_page (EvView *view,
		     gint    page)
{
	gboolean dual_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return -1;

	dual_page = is_dual_page (view, NULL);
	page -= dual_page ? 2 : 1;

	if (page >= 0)
		return page;

	if (dual_page && page == -1)
		return 0;

	return -1;
}

static gboolean
cursor_go_to_page_start (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	priv->cursor_offset = 0;

	return TRUE;
}

static gboolean
cursor_go_to_page_end (EvView *view)
{
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);
	if (!log_attrs)
		return FALSE;

	priv->cursor_offset = n_attrs;

	return TRUE;
}

static gboolean
cursor_go_to_next_page (EvView *view)
{
	gint new_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	new_page = go_to_next_page (view, priv->cursor_page);
	if (new_page != -1) {
		priv->cursor_page = new_page;
		return cursor_go_to_page_start (view);
	}

	return FALSE;
}

static gboolean
cursor_go_to_previous_page (EvView *view)
{
	gint new_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	new_page = go_to_previous_page (view, priv->cursor_page);
	if (new_page != -1) {
		priv->cursor_page = new_page;
		return cursor_go_to_page_end (view);
	}
	return FALSE;
}

static gboolean
cursor_go_to_document_start (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	priv->cursor_page = 0;
	return cursor_go_to_page_start (view);
}

static gboolean
cursor_go_to_document_end (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->document)
		return FALSE;

	priv->cursor_page = ev_document_get_n_pages (priv->document) - 1;
	return cursor_go_to_page_end (view);
}

static gboolean
cursor_backward_char (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);
	if (!log_attrs)
		return FALSE;

	if (priv->cursor_offset == 0)
		return cursor_go_to_previous_page (view);

	do {
		priv->cursor_offset--;
	} while (priv->cursor_offset >= 0 && !log_attrs[priv->cursor_offset].is_cursor_position);

	return TRUE;
}

static gboolean
cursor_forward_char (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);
	if (!log_attrs)
		return FALSE;

	if (priv->cursor_offset >= n_attrs)
		return cursor_go_to_next_page (view);

	do {
		priv->cursor_offset++;
	} while (priv->cursor_offset <= n_attrs && !log_attrs[priv->cursor_offset].is_cursor_position);

	return TRUE;
}

static gboolean
cursor_backward_word_start (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;
	gint          i, j;

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);
	if (!log_attrs)
		return FALSE;

	/* Skip current word starts */
	for (i = priv->cursor_offset; i >= 0 && log_attrs[i].is_word_start; i--);
	if (i <= 0) {
		if (cursor_go_to_previous_page (view))
			return cursor_backward_word_start (view);
		return FALSE;
	}

	/* Move to the beginning of the word */
	for (j = i; j >= 0 && !log_attrs[j].is_word_start; j--);
	priv->cursor_offset = MAX (0, j);

	return TRUE;
}

static gboolean
cursor_forward_word_end (EvView *view)
{
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;
	gint          i, j;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);
	if (!log_attrs)
		return FALSE;

	/* Skip current word ends */
	for (i = priv->cursor_offset; i < n_attrs && log_attrs[i].is_word_end; i++);
	if (i >= n_attrs) {
		if (cursor_go_to_next_page (view))
			return cursor_forward_word_end (view);
		return FALSE;
	}

	/* Move to the end of the word. */
	for (j = i; j < n_attrs && !log_attrs[j].is_word_end; j++);
	priv->cursor_offset = MIN (j, n_attrs);

	return TRUE;
}

static gboolean
cursor_go_to_line_start (EvView *view)
{
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;
	gint          i;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);
	if (!log_attrs)
		return FALSE;

	for (i = priv->cursor_offset; i >= 0 && !log_attrs[i].is_mandatory_break; i--);
	priv->cursor_offset = MAX (0, i);

	return TRUE;
}

static gboolean
cursor_backward_line (EvView *view)
{
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!cursor_go_to_line_start (view))
		return FALSE;

	if (priv->cursor_offset == 0)
		return cursor_go_to_previous_page (view);

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);

	do {
		priv->cursor_offset--;
	} while (priv->cursor_offset >= 0 && !log_attrs[priv->cursor_offset].is_mandatory_break);
	priv->cursor_offset = MAX (0, priv->cursor_offset);

	return TRUE;
}

static gboolean
cursor_go_to_line_end (EvView *view)
{
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;
	gint          i;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->page_cache)
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);
	if (!log_attrs)
		return FALSE;

	for (i = priv->cursor_offset + 1; i <= n_attrs && !log_attrs[i].is_mandatory_break; i++);
	priv->cursor_offset = MIN (i, n_attrs);

	if (priv->cursor_offset == n_attrs)
		return TRUE;

	do {
		priv->cursor_offset--;
	} while (priv->cursor_offset >= 0 && !log_attrs[priv->cursor_offset].is_cursor_position);

	return TRUE;
}

static gboolean
cursor_forward_line (EvView *view)
{
	PangoLogAttr *log_attrs = NULL;
	gulong        n_attrs;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!cursor_go_to_line_end (view))
		return FALSE;

	ev_page_cache_get_text_log_attrs (priv->page_cache, priv->cursor_page, &log_attrs, &n_attrs);

	if (priv->cursor_offset == n_attrs)
		return cursor_go_to_next_page (view);

	do {
		priv->cursor_offset++;
	} while (priv->cursor_offset <= n_attrs && !log_attrs[priv->cursor_offset].is_cursor_position);

	return TRUE;
}

static void
extend_selection (EvView *view,
		  GdkPoint *start_point,
		  GdkPoint *end_point)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->selection_info.selections) {
		priv->selection_info.start.x = start_point->x;
		priv->selection_info.start.y = start_point->y;
	}

	compute_selections (view,
			    EV_SELECTION_STYLE_GLYPH,
			    &(priv->selection_info.start),
			    end_point);
}

static gboolean
cursor_clear_selection (EvView  *view,
			gboolean forward)
{
	GList                *l;
	EvViewSelection      *selection;
	cairo_rectangle_int_t rect;
	cairo_region_t        *region, *tmp_region = NULL;
	gint                  doc_x, doc_y;
	GdkRectangle          area;
	EvViewPrivate *priv = GET_PRIVATE (view);

	/* When clearing the selection, move the cursor to
	 * the limits of the selection region.
	 */
	if (!priv->selection_info.selections)
		return FALSE;

	l = forward ? g_list_last (priv->selection_info.selections) : priv->selection_info.selections;
	selection = (EvViewSelection *)l->data;

	region = selection->covered_region;

	/* The selection boundary is not in the current page */
	if (!region || cairo_region_is_empty (region)) {
		EvRenderContext *rc;
		EvPage          *page;

		ev_document_doc_mutex_lock ();

		page = ev_document_get_page (priv->document, selection->page);
		rc = ev_render_context_new (page, priv->rotation, priv->scale);
		g_object_unref (page);

		tmp_region = ev_selection_get_selection_region (EV_SELECTION (priv->document),
								rc,
								EV_SELECTION_STYLE_GLYPH,
								&(selection->rect));
		g_object_unref (rc);

		ev_document_doc_mutex_unlock();

		if (!tmp_region || cairo_region_is_empty (tmp_region)) {
			cairo_region_destroy (tmp_region);
			return FALSE;
		}

		region = tmp_region;
	}

	cairo_region_get_rectangle (region,
				    forward ? cairo_region_num_rectangles (region) - 1 : 0,
				    &rect);

	if (tmp_region) {
		cairo_region_destroy (tmp_region);
		region = NULL;
	}

	if (!get_doc_point_from_offset (view, selection->page,
					forward ? rect.x + rect.width : rect.x,
					rect.y + (rect.height / 2), &doc_x, &doc_y))
		return FALSE;

	position_caret_cursor_at_doc_point (view, selection->page, doc_x, doc_y);

	if (get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &area))
		priv->cursor_line_offset = area.x;

	return TRUE;
}

static gboolean
ev_view_move_cursor (EvView         *view,
		     GtkMovementStep step,
		     gint            count,
		     gboolean        extend_selections)
{
	GdkRectangle    rect;
	GdkRectangle    prev_rect;
	gint            prev_offset;
	gint            prev_page;
	GdkRectangle    select_start_rect;
	gint            select_start_offset = 0;
	gint            select_start_page = 0;
	gboolean        changed_page;
	gboolean        clear_selections = FALSE;
	const gboolean  forward = count >= 0;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->caret_enabled || priv->rotation != 0)
		return FALSE;

	priv->key_binding_handled = TRUE;
	priv->cursor_blink_time = 0;

	prev_offset = priv->cursor_offset;
	prev_page = priv->cursor_page;

	if (extend_selections) {
		select_start_offset = priv->cursor_offset;
		select_start_page = priv->cursor_page;
	}

	clear_selections = !extend_selections && ev_view_has_selection (view);

	switch (step) {
	case GTK_MOVEMENT_VISUAL_POSITIONS:
		if (!clear_selections || !cursor_clear_selection (view, count > 0)) {
			while (count > 0) {
				cursor_forward_char (view);
				count--;
			}
			while (count < 0) {
				cursor_backward_char (view);
				count++;
			}
		}
		break;
	case GTK_MOVEMENT_WORDS:
		if (!clear_selections || cursor_clear_selection (view, count > 0)) {
			while (count > 0) {
				cursor_forward_word_end (view);
				count--;
			}
			while (count < 0) {
				cursor_backward_word_start (view);
				count++;
			}
		}
		break;
	case GTK_MOVEMENT_DISPLAY_LINES:
		if (!clear_selections || cursor_clear_selection (view, count > 0)) {
			while(count > 0) {
				cursor_forward_line (view);
				count--;
			}
			while (count < 0) {
				cursor_backward_line (view);
				count++;
			}
		}
		break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
		if (!clear_selections  || cursor_clear_selection (view, count > 0)) {
			if (count > 0)
				cursor_go_to_line_end (view);
			else if (count < 0)
				cursor_go_to_line_start (view);
		}
		break;
	case GTK_MOVEMENT_BUFFER_ENDS:
		/* If we are selecting and there is a previous selection,
		   set the new selection's start point to the start point
		   of the previous selection */
		if (extend_selections && ev_view_has_selection (view)) {
			if (cursor_clear_selection (view, FALSE)) {
				select_start_offset = priv->cursor_offset;
				select_start_page = priv->cursor_page;
			}
		}

		if (count > 0)
			cursor_go_to_document_end (view);
		else if (count < 0)
			cursor_go_to_document_start (view);
		break;
	default:
		g_assert_not_reached ();
	}

	ev_view_pend_cursor_blink (view);

	/* Notify the user that it was not possible to move the caret cursor */
	if (!clear_selections &&
	    prev_offset == priv->cursor_offset && prev_page == priv->cursor_page) {
		gtk_widget_error_bell (GTK_WIDGET (view));
		return TRUE;
	}

	/* Scroll to make the caret visible */
	if (!get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &rect))
		return TRUE;

	if (!priv->continuous) {
		changed_page = FALSE;
		if (prev_page < priv->cursor_page) {
			ev_view_next_page (view);
			cursor_go_to_page_start (view);
			changed_page = TRUE;
		} else if (prev_page > priv->cursor_page) {
			ev_view_previous_page (view);
			cursor_go_to_page_end (view);
			_ev_view_ensure_rectangle_is_visible (view, &rect);
			changed_page = TRUE;
		}

		if (changed_page) {
                       rect.x += priv->scroll_x;
                       rect.y += priv->scroll_y;
                       _ev_view_ensure_rectangle_is_visible (view, &rect);
			g_signal_emit (view, signals[SIGNAL_CURSOR_MOVED], 0, priv->cursor_page, priv->cursor_offset);
			clear_selection (view);
			return TRUE;
		}
	}

	if (step == GTK_MOVEMENT_DISPLAY_LINES) {
		const gint prev_cursor_offset = priv->cursor_offset;

		position_caret_cursor_at_location (view,
						   MAX (rect.x, priv->cursor_line_offset),
						   rect.y + (rect.height / 2));
		/* Make sure we didn't move the cursor in the wrong direction
		 * in case the visual order isn't the same as the logical one,
		 * in order to avoid cursor movement loops */
		if ((forward && prev_cursor_offset > priv->cursor_offset) ||
		    (!forward && prev_cursor_offset < priv->cursor_offset)) {
			priv->cursor_offset = prev_cursor_offset;
		}
		if (!clear_selections &&
		    prev_offset == priv->cursor_offset && prev_page == priv->cursor_page) {
			gtk_widget_error_bell (GTK_WIDGET (view));
			return TRUE;
		}

		if (!get_caret_cursor_area (view, priv->cursor_page, priv->cursor_offset, &rect))
			return TRUE;
	} else {
		priv->cursor_line_offset = rect.x;
	}

	get_caret_cursor_area (view, prev_page, prev_offset, &prev_rect);

	rect.x += priv->scroll_x;
	rect.y += priv->scroll_y;

	ev_document_model_set_page (priv->model, priv->cursor_page);
	_ev_view_ensure_rectangle_is_visible (view, &rect);

	g_signal_emit (view, signals[SIGNAL_CURSOR_MOVED], 0, priv->cursor_page, priv->cursor_offset);

	gtk_widget_queue_draw (GTK_WIDGET (view));

	/* Select text */
	if (extend_selections && EV_IS_SELECTION (priv->document)) {
		GdkPoint start_point, end_point;

		if (!get_caret_cursor_area (view, select_start_page, select_start_offset, &select_start_rect))
			return TRUE;

		start_point.x = select_start_rect.x + priv->scroll_x;
		start_point.y = select_start_rect.y + (select_start_rect.height / 2) + priv->scroll_y;

		end_point.x = rect.x;
		end_point.y = rect.y + rect.height / 2;

		extend_selection (view, &start_point, &end_point);
	} else if (clear_selections)
		clear_selection (view);

	return TRUE;
}

static gboolean
ev_view_activate_form_field (EvView      *view,
			     EvFormField *field)
{
	gboolean handled = FALSE;

	if (field->is_read_only)
		return handled;

	if (field->activation_link) {
		ev_view_handle_link (view, field->activation_link);
		handled = TRUE;
	}

	if (EV_IS_FORM_FIELD_BUTTON (field)) {
		ev_view_form_field_button_toggle (view, field);
		handled = TRUE;
	}

	return handled;
}

#if 0
static gboolean
current_event_is_space_key_press (void)
{
	GdkEvent *current_event;
	guint     keyval;
	gboolean  is_space_key_press;

	current_event = gtk_get_current_event ();
	if (!current_event)
		return FALSE;

	is_space_key_press = current_event->type == GDK_KEY_PRESS &&
		gdk_event_get_keyval (current_event, &keyval) &&
		(keyval == GDK_KEY_space || keyval == GDK_KEY_KP_Space);
	gdk_event_free (current_event);

	return is_space_key_press;
}
#endif

static gboolean
ev_view_activate_link (EvView *view,
		       EvLink *link)
{
#if 0
	/* Most of the GtkWidgets emit activate on both Space and Return key press,
	 * but we don't want to activate links on Space for consistency with the Web.
	 */
	if (current_event_is_space_key_press ())
		return FALSE;
#endif
	ev_view_handle_link (view, link);

	return TRUE;
}

static void
ev_view_activate (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->focused_element)
		return;

	if (EV_IS_DOCUMENT_FORMS (priv->document) &&
	    EV_IS_FORM_FIELD (priv->focused_element->data)) {
		priv->key_binding_handled = ev_view_activate_form_field (view, EV_FORM_FIELD (priv->focused_element->data));
		return;
	}

	if (EV_IS_DOCUMENT_LINKS (priv->document) &&
	    EV_IS_LINK (priv->focused_element->data)) {
		priv->key_binding_handled = ev_view_activate_link (view, EV_LINK (priv->focused_element->data));
		return;
	}
}

static gboolean
ev_view_autoscroll_cb (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gdouble speed, value;

	/* If the user stops autoscrolling, autoscrolling will be
	 * set to false but the timeout will continue; stop the timeout: */
	if (!priv->scroll_info.autoscrolling) {
		priv->scroll_info.timeout_id = 0;
		return G_SOURCE_REMOVE;
	}

	/* Replace 100 with your speed of choice: The lower the faster.
	 * Replace 3 with another speed of choice: The higher, the faster it accelerated
	 * 	based on the distance of the starting point from the mouse
	 * (All also effected by the timeout interval of this callback) */

	if (priv->scroll_info.start_y > priv->scroll_info.last_y)
		speed = -pow ((((gdouble)priv->scroll_info.start_y - priv->scroll_info.last_y) / 100), 3);
	else
		speed = pow ((((gdouble)priv->scroll_info.last_y - priv->scroll_info.start_y) / 100), 3);

	value = gtk_adjustment_get_value (priv->vadjustment);
	value = CLAMP (value + speed, 0,
		       gtk_adjustment_get_upper (priv->vadjustment) -
		       gtk_adjustment_get_page_size (priv->vadjustment));
	gtk_adjustment_set_value (priv->vadjustment, value);

	return G_SOURCE_CONTINUE;

}

static void
ev_view_autoscroll_resume (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->scroll_info.autoscrolling)
		return;

	if (priv->scroll_info.timeout_id > 0)
		return;

	priv->scroll_info.timeout_id =
		g_timeout_add (20, (GSourceFunc)ev_view_autoscroll_cb,
			       view);
}

static void
ev_view_autoscroll_pause (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->scroll_info.autoscrolling)
		return;

	g_clear_handle_id (&priv->scroll_info.timeout_id, g_source_remove);
}

static void
ev_view_focus_in (GtkEventControllerFocus	*self,
		  gpointer			 user_data)
{
	EvView *view = EV_VIEW (user_data);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->pixbuf_cache)
		ev_pixbuf_cache_style_changed (priv->pixbuf_cache);

	ev_view_autoscroll_resume (view);

	ev_view_check_cursor_blink (view);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
ev_view_focus_out (GtkEventControllerFocus	*self,
		   gpointer			 user_data)
{
	EvView *view = EV_VIEW (user_data);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->pixbuf_cache)
		ev_pixbuf_cache_style_changed (priv->pixbuf_cache);

	ev_view_autoscroll_pause (view);

	ev_view_check_cursor_blink (view);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
ev_view_leave_notify_event (GtkEventControllerMotion	*self,
			    gpointer			 user_data)
{
	EvView *view = EV_VIEW (user_data);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->cursor != EV_VIEW_CURSOR_NORMAL)
		ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
}

static void
ev_view_enter_notify_event (GtkEventControllerMotion	*self,
			    gdouble			 x,
			    gdouble			 y,
			    gpointer			 user_data)
{
	ev_view_handle_cursor_over_xy (EV_VIEW (user_data), x, y, FALSE);
}

/*** Drawing ***/

static void
draw_rubberband (EvView             *view,
		 GtkSnapshot        *snapshot,
		 const GdkRectangle *rect,
		 gboolean            active)
{
	GtkStyleContext *context;
	EvViewPrivate *priv = GET_PRIVATE (view);

	context = gtk_widget_get_style_context (GTK_WIDGET (view));
	gtk_style_context_save (context);
	gtk_style_context_add_class (context, EV_STYLE_CLASS_FIND_RESULTS);
	if (active)
		gtk_style_context_set_state (context, GTK_STATE_FLAG_ACTIVE);
	else
		gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);

	gtk_snapshot_render_background (snapshot, context,
			  rect->x - priv->scroll_x,
			  rect->y - priv->scroll_y,
			  rect->width, rect->height);
	gtk_style_context_restore (context);
}


static void
highlight_find_results (EvView		*view,
                        GtkSnapshot	*snapshot,
                        int		 page)
{
	EvRectangle *ev_rect;
	gint i, n_results = 0;
	EvViewPrivate *priv = GET_PRIVATE (view);

	n_results = ev_view_find_get_n_results (view, page);
	ev_rect = ev_rectangle_new ();

	for (i = 0; i < n_results; i++) {
		EvFindRectangle *find_rect;
		GdkRectangle view_rectangle;
		gboolean active;

		find_rect = ev_view_find_get_result (view, page, i);
		ev_rect->x1 = find_rect->x1;
		ev_rect->x2 = find_rect->x2;
		ev_rect->y1 = find_rect->y1;
		ev_rect->y2 = find_rect->y2;

		active = page == priv->find_page && i == priv->find_result;
		_ev_view_transform_doc_rect_to_view_rect (view, page, ev_rect, &view_rectangle);
		draw_rubberband (view, snapshot, &view_rectangle, active);

		if (active && find_rect->next_line) {
			/* Draw now next result (which is second part of multi-line match) */
			i++;
			find_rect = ev_view_find_get_result (view, page, i);
			ev_rect->x1 = find_rect->x1;
			ev_rect->x2 = find_rect->x2;
			ev_rect->y1 = find_rect->y1;
			ev_rect->y2 = find_rect->y2;
			_ev_view_transform_doc_rect_to_view_rect (view, page, ev_rect, &view_rectangle);
			draw_rubberband (view, snapshot, &view_rectangle, TRUE);
		}
        }

	ev_rectangle_free (ev_rect);
}

static void
highlight_forward_search_results (EvView	*view,
                                  GtkSnapshot	*snapshot,
                                  int		 page)
{
	GdkRectangle rect;
	EvViewPrivate *priv = GET_PRIVATE (view);
	EvMapping   *mapping = priv->synctex_result;
	GdkRGBA color = { 1.0, 0.0, 0.0, 0.3 };

	if (GPOINTER_TO_INT (mapping->data) != page)
		return;

	_ev_view_transform_doc_rect_to_view_rect (view, page, &mapping->area, &rect);

	gtk_snapshot_append_color (snapshot, &color,
			&GRAPHENE_RECT_INIT (
			 rect.x - priv->scroll_x,
			 rect.y - priv->scroll_y,
			 rect.width, rect.height));
}

static void
draw_surface (GtkSnapshot     *snapshot,
	      GdkTexture      *texture,
	      const graphene_point_t *point,
	      const graphene_rect_t *area,
	      gboolean inverted)
{
	gtk_snapshot_save (snapshot);
	gtk_snapshot_translate (snapshot, point);

	if (inverted) {
		gtk_snapshot_push_blend (snapshot, GSK_BLEND_MODE_DIFFERENCE);
		gtk_snapshot_append_color (snapshot, &(GdkRGBA) {1., 1., 1., 1.}, area);
		gtk_snapshot_pop (snapshot);
	}

	gtk_snapshot_append_texture (snapshot, texture, area);

	if (inverted)
		gtk_snapshot_pop (snapshot);

	gtk_snapshot_restore (snapshot);
}

void
_ev_view_get_selection_colors (EvView  *view,
			       GdkRGBA *bg_color,
			       GdkRGBA *fg_color)
{
	GtkWidget       *widget = GTK_WIDGET (view);
	GtkStyleContext *context = gtk_widget_get_style_context (widget);

	if (bg_color &&
	    !gtk_style_context_lookup_color (context, "accent_bg_color", bg_color) &&
	    !gtk_style_context_lookup_color (context, "theme_selected_bg_color", bg_color)) {
		bg_color->red = 0;
		bg_color->green = 0.623;
		bg_color->blue = 1.;
		bg_color->alpha = 1.;
	}

	if (gtk_widget_has_focus (widget))
		bg_color->alpha = 0.3;
	else
		bg_color->alpha = 0.6;

	if (fg_color &&
	    !gtk_style_context_lookup_color (context, "accent_fg_color", fg_color) &&
	    !gtk_style_context_lookup_color (context, "theme_selected_fg_color", fg_color)) {
		fg_color->red = 1.;
		fg_color->green = 1.;
		fg_color->blue = 1.;
		fg_color->alpha = 1.;
	}
}

static void
draw_selection_region (GtkSnapshot    *snapshot,
		       GtkWidget      *widget,
		       cairo_region_t *region,
		       GdkRGBA        *color,
		       gint            x,
		       gint            y,
		       gdouble         scale_x,
		       gdouble         scale_y)
{
	cairo_rectangle_int_t box;
	gint n_boxes, i;
	guint state;
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_save (context);
	gtk_style_context_add_class (context, EV_STYLE_CLASS_FIND_RESULTS);
	state = gtk_style_context_get_state (context) |
		(gtk_widget_has_focus (widget) ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_ACTIVE);
	gtk_style_context_set_state (context, state);

	gtk_snapshot_save (snapshot);
	gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));

	n_boxes = cairo_region_num_rectangles (region);

	for (i = 0; i < n_boxes; i++) {
		cairo_region_get_rectangle (region, i, &box);

		gtk_snapshot_render_background (snapshot, context,
			box.x, box.y, box.width, box.height);
	}

	gtk_snapshot_restore (snapshot);
	gtk_style_context_restore (context);
}

static void
draw_one_page (EvView       *view,
	       gint          page,
	       GtkSnapshot  *snapshot,
	       GdkRectangle *page_area,
	       GtkBorder    *border,
	       GdkRectangle *expose_area,
	       gboolean     *page_ready)
{
	GtkStyleContext *context;
	GdkRectangle     overlap;
	GdkRectangle     real_page_area;
	gint             current_page;
	GtkWidget	*widget = GTK_WIDGET (view);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (! gdk_rectangle_intersect (page_area, expose_area, &overlap))
		return;

	/* Render the document itself */
	real_page_area = *page_area;

	real_page_area.x += border->left;
	real_page_area.y += border->top;
	real_page_area.width -= (border->left + border->right);
	real_page_area.height -= (border->top + border->bottom);
	*page_ready = TRUE;

	context = gtk_widget_get_style_context (GTK_WIDGET (view));
	current_page = ev_document_model_get_page (priv->model);

	gtk_style_context_save (context);
	gtk_style_context_add_class (context, EV_STYLE_CLASS_DOCUMENT_PAGE);
	if (ev_document_model_get_inverted_colors (priv->model))
		gtk_style_context_add_class (context, EV_STYLE_CLASS_INVERTED);

	if (priv->continuous && page == current_page)
		gtk_style_context_set_state (context, GTK_STATE_FLAG_ACTIVE);

	gtk_snapshot_render_background (snapshot, context, page_area->x, page_area->y, page_area->width, page_area->height);
	gtk_snapshot_render_frame (snapshot, context, page_area->x, page_area->y, page_area->width, page_area->height);
	gtk_style_context_restore (context);

	if (gdk_rectangle_intersect (&real_page_area, expose_area, &overlap)) {
		gint             width, height;
		GdkTexture      *page_texture = NULL, *selection_texture = NULL;
		graphene_point_t point;
		graphene_rect_t area;
		cairo_region_t *region = NULL;
		gboolean inverted = ev_document_model_get_inverted_colors (priv->model);

		page_texture = ev_pixbuf_cache_get_texture (priv->pixbuf_cache, page);

		if (!page_texture) {
			if (page == current_page)
				ev_view_set_loading (view, TRUE);

			*page_ready = FALSE;

			return;
		}

		if (page == current_page)
			ev_view_set_loading (view, FALSE);

		ev_view_get_page_size (view, page, &width, &height);

		area = GRAPHENE_RECT_INIT (real_page_area.x - overlap.x,
					   real_page_area.y - overlap.y,
					   width, height);
		point = GRAPHENE_POINT_INIT (overlap.x, overlap.y);

		draw_surface (snapshot, page_texture, &point, &area, inverted);

		/* Get the selection pixbuf iff we have something to draw */
		if (!find_selection_for_page (view, page))
			return;

		selection_texture = ev_pixbuf_cache_get_selection_texture (priv->pixbuf_cache,
									   page,
									   priv->scale);
		if (selection_texture) {
			draw_surface (snapshot, selection_texture, &point, &area, false);
			return;
		}

		region = ev_pixbuf_cache_get_selection_region (priv->pixbuf_cache,
							       page,
							       priv->scale);
		if (region) {
			double scale_x, scale_y;
			GdkRGBA color;

			scale_x = (gdouble)width / gdk_texture_get_width (page_texture);
			scale_y = (gdouble)height / gdk_texture_get_height (page_texture);

			_ev_view_get_selection_colors (view, &color, NULL);
			draw_selection_region (snapshot, widget, region, &color, real_page_area.x, real_page_area.y,
					       scale_x, scale_y);
		}
	}
}

/*** GObject functions ***/

static void
ev_view_finalize (GObject *object)
{
	EvView *view = EV_VIEW (object);
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_list_free_full (g_steal_pointer (&priv->selection_info.selections), (GDestroyNotify)selection_free);
	g_clear_object (&priv->link_selected);

	g_clear_pointer (&priv->synctex_result, g_free);

	g_clear_object (&priv->image_dnd_info.image);
	g_clear_pointer (&priv->annot_window_map, g_hash_table_destroy);

	G_OBJECT_CLASS (ev_view_parent_class)->finalize (object);
}

static void
ev_view_dispose (GObject *object)
{
	EvView *view = EV_VIEW (object);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->model) {
		g_signal_handlers_disconnect_by_data (priv->model, view);
		g_clear_object (&priv->model);
	}

	g_clear_object (&priv->pixbuf_cache);
	g_clear_object (&priv->document);
	g_clear_object (&priv->page_cache);

	ev_view_find_cancel (view);

	ev_view_window_children_free (view);

	g_clear_handle_id (&priv->update_cursor_idle_id, g_source_remove);
	g_clear_handle_id (&priv->selection_scroll_id, g_source_remove);
	g_clear_handle_id (&priv->selection_update_id, g_source_remove);
	g_clear_handle_id (&priv->scroll_info.timeout_id, g_source_remove);
	g_clear_handle_id (&priv->drag_info.drag_timeout_id, g_source_remove);
	g_clear_handle_id (&priv->drag_info.release_timeout_id, g_source_remove);
	g_clear_handle_id (&priv->cursor_blink_timeout_id, g_source_remove);
	g_clear_handle_id (&priv->child_focus_idle_id, g_source_remove);

	if (priv->link_preview.job) {
		ev_job_cancel (priv->link_preview.job);
		g_clear_object (&priv->link_preview.job);
	}

        gtk_scrollable_set_hadjustment (GTK_SCROLLABLE (view), NULL);
        gtk_scrollable_set_vadjustment (GTK_SCROLLABLE (view), NULL);

	G_OBJECT_CLASS (ev_view_parent_class)->dispose (object);
}

static void
ev_view_get_property (GObject     *object,
		      guint        prop_id,
		      GValue      *value,
		      GParamSpec  *pspec)
{
	EvView *view = EV_VIEW (object);
	EvViewPrivate *priv = GET_PRIVATE (view);

	switch (prop_id) {
	case PROP_IS_LOADING:
		g_value_set_boolean (value, priv->loading);
		break;
	case PROP_CAN_ZOOM_IN:
		g_value_set_boolean (value, priv->can_zoom_in);
		break;
	case PROP_CAN_ZOOM_OUT:
		g_value_set_boolean (value, priv->can_zoom_out);
		break;
	case PROP_HADJUSTMENT:
		g_value_set_object (value, priv->hadjustment);
		break;
	case PROP_VADJUSTMENT:
		g_value_set_object (value, priv->vadjustment);
		break;
	case PROP_HSCROLL_POLICY:
		g_value_set_enum (value, priv->hscroll_policy);
		break;
	case PROP_VSCROLL_POLICY:
		g_value_set_enum (value, priv->vscroll_policy);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_view_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	EvView *view = EV_VIEW (object);
	EvViewPrivate *priv = GET_PRIVATE (view);

	switch (prop_id) {
	case PROP_IS_LOADING:
		ev_view_set_loading (view, g_value_get_boolean (value));
		break;
	case PROP_HADJUSTMENT:
		ev_view_set_scroll_adjustment (view, GTK_ORIENTATION_HORIZONTAL,
					       (GtkAdjustment *) g_value_get_object (value));
		break;
	case PROP_VADJUSTMENT:
		ev_view_set_scroll_adjustment (view, GTK_ORIENTATION_VERTICAL,
					       (GtkAdjustment *) g_value_get_object (value));
		break;
	case PROP_HSCROLL_POLICY:
		priv->hscroll_policy = g_value_get_enum (value);
		gtk_widget_queue_resize (GTK_WIDGET (view));
		break;
	case PROP_VSCROLL_POLICY:
		priv->vscroll_policy = g_value_get_enum (value);
		gtk_widget_queue_resize (GTK_WIDGET (view));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
view_update_scale_limits (EvView *view)
{
	gdouble    min_width, min_height;
	gdouble    width, height;
	gdouble    max_scale;
	gdouble    dpi;
	gint       rotation;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->document)
		return;

	rotation = ev_document_model_get_rotation (priv->model);

	dpi = ev_document_misc_get_widget_dpi (GTK_WIDGET (view)) / 72.0;

	ev_document_get_min_page_size (priv->document, &min_width, &min_height);
	width = (rotation == 0 || rotation == 180) ? min_width : min_height;
	height = (rotation == 0 || rotation == 180) ? min_height : min_width;
	max_scale = sqrt (priv->pixbuf_cache_size / (width * dpi * 4 * height * dpi));

	ev_document_model_set_min_scale (priv->model, MIN_SCALE * dpi);
	ev_document_model_set_max_scale (priv->model, max_scale * dpi);
}

static void
pan_gesture_pan_cb (GtkGesturePan   *gesture,
		    GtkPanDirection  direction,
		    gdouble          offset,
		    EvView          *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->continuous ||
	    gtk_widget_get_width (GTK_WIDGET (view)) < priv->requisition.width) {
		gtk_gesture_set_state (GTK_GESTURE (gesture),
				       GTK_EVENT_SEQUENCE_DENIED);
		return;
	}

#define PAN_ACTION_DISTANCE 200

	priv->pan_action = EV_PAN_ACTION_NONE;
	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

	if (offset > PAN_ACTION_DISTANCE) {
		if (direction == GTK_PAN_DIRECTION_LEFT ||
		    gtk_widget_get_direction (GTK_WIDGET (view)) == GTK_TEXT_DIR_RTL)
			priv->pan_action = EV_PAN_ACTION_NEXT;
		else
			priv->pan_action = EV_PAN_ACTION_PREV;
	}
#undef PAN_ACTION_DISTANCE
}

static void
pan_gesture_end_cb (GtkGesture       *gesture,
		    GdkEventSequence *sequence,
		    EvView           *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!gtk_gesture_handles_sequence (gesture, sequence))
		return;

	if (priv->pan_action == EV_PAN_ACTION_PREV)
		ev_view_previous_page (view);
	else if (priv->pan_action == EV_PAN_ACTION_NEXT)
		ev_view_next_page (view);

	priv->pan_action = EV_PAN_ACTION_NONE;
}

static void
add_move_binding_keypad (GtkWidgetClass	*widget_class,
			 guint           keyval,
			 GdkModifierType modifiers,
			 GtkMovementStep step,
			 gint            count)
{
	guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;

	gtk_widget_class_add_binding_signal (widget_class, keyval, modifiers,
					"move-cursor", "(iib)",
					step, count, FALSE);

	gtk_widget_class_add_binding_signal (widget_class, keypad_keyval, modifiers,
					"move-cursor", "(iib)",
					step, count, FALSE);

	/* Selection-extending version */
	gtk_widget_class_add_binding_signal (widget_class, keyval, modifiers | GDK_SHIFT_MASK,
					"move-cursor", "(iib)",
					step, count, TRUE);
	gtk_widget_class_add_binding_signal (widget_class, keypad_keyval, modifiers | GDK_SHIFT_MASK,
					"move-cursor", "(iib)",
					step, count, TRUE);
}

static gint
ev_view_mapping_compare (const EvMapping *a,
			 const EvMapping *b,
			 gpointer         user_data)
{
	GtkTextDirection text_direction = GPOINTER_TO_INT (user_data);
	gint y1 = a->area.y1 + (a->area.y2 - a->area.y1) / 2;
	gint y2 = b->area.y1 + (b->area.y2 - b->area.y1) / 2;

	if (y1 == y2) {
		gint x1 = a->area.x1 + (a->area.x2 - a->area.x1) / 2;
		gint x2 = b->area.x1 + (b->area.x2 - b->area.x1) / 2;

		if (text_direction == GTK_TEXT_DIR_RTL)
			return (x1 < x2) ? 1 : ((x1 == x2) ? 0 : -1);

		return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
	}

	return (y1 < y2) ? -1 : 1;
}

static GList *
ev_view_get_sorted_mapping_list (EvView          *view,
				 GtkDirectionType direction,
				 gint             page)
{
	GList         *mapping_list = NULL, *l;
	EvMappingList *forms_mapping;
	EvViewPrivate *priv = GET_PRIVATE (view);

	forms_mapping = ev_page_cache_get_form_field_mapping (priv->page_cache, page);

	for (l = ev_mapping_list_get_list (forms_mapping); l; l = g_list_next (l)) {
		EvMapping   *mapping = (EvMapping *)l->data;
		EvFormField *field = (EvFormField *)mapping->data;

		if (field->is_read_only || EV_IS_FORM_FIELD_SIGNATURE (field))
			continue;

		mapping_list = g_list_prepend (mapping_list, mapping);
	}

	if (!mapping_list)
		return NULL;

	mapping_list = g_list_sort_with_data (g_list_reverse (mapping_list),
					      (GCompareDataFunc)ev_view_mapping_compare,
					      GINT_TO_POINTER (gtk_widget_get_direction (GTK_WIDGET (view))));

	if (direction == GTK_DIR_TAB_BACKWARD)
		mapping_list = g_list_reverse (mapping_list);
	return mapping_list;
}

static gboolean
child_focus_forward_idle_cb (gpointer user_data)
{
	EvView *view = EV_VIEW (user_data);
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->child_focus_idle_id = 0;
	gtk_widget_child_focus (GTK_WIDGET (view), GTK_DIR_TAB_FORWARD);

	return G_SOURCE_REMOVE;
}

static gboolean
child_focus_backward_idle_cb (gpointer user_data)
{
	EvView *view = EV_VIEW (user_data);
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->child_focus_idle_id = 0;
	gtk_widget_child_focus (GTK_WIDGET (view), GTK_DIR_TAB_BACKWARD);

	return G_SOURCE_REMOVE;
}

static void
schedule_child_focus_in_idle (EvView           *view,
			      GtkDirectionType  direction)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_clear_handle_id (&priv->child_focus_idle_id, g_source_remove);
	priv->child_focus_idle_id =
		g_idle_add (direction == GTK_DIR_TAB_FORWARD ? child_focus_forward_idle_cb : child_focus_backward_idle_cb,
			    view);
}

static gboolean
ev_view_focus_next (EvView           *view,
		    GtkDirectionType  direction)
{
	EvMapping *focus_element;
	GList     *elements;
	gboolean   had_focused_element;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->focused_element) {
		GList *l;

		elements = ev_view_get_sorted_mapping_list (view, direction, priv->focused_element_page);
		l = g_list_find (elements, priv->focused_element);
		l = g_list_next (l);
		focus_element = l ? l->data : NULL;
		had_focused_element = TRUE;
	} else {
		elements = ev_view_get_sorted_mapping_list (view, direction, priv->current_page);
		focus_element = elements ? elements->data : NULL;
		had_focused_element = FALSE;
	}

	g_list_free (elements);

	if (focus_element) {
		ev_view_remove_all_form_fields (view);
		_ev_view_focus_form_field (view, EV_FORM_FIELD (focus_element->data));

		return TRUE;
	}

	ev_view_remove_all_form_fields (view);
	_ev_view_set_focused_element (view, NULL, -1);

	/* Only try to move the focus to next/previous pages when the current page had
	 * a focused element. This prevents the view from jumping to the first/last page
	 * when there are not focusable elements.
	 */
	if (!had_focused_element)
		return FALSE;

	/* FIXME: this doesn't work if the next/previous page doesn't have form fields */
	if (direction == GTK_DIR_TAB_FORWARD) {
		if (ev_view_next_page (view)) {
			schedule_child_focus_in_idle (view, direction);
			return TRUE;
		}
	} else if (direction == GTK_DIR_TAB_BACKWARD) {
		if (ev_view_previous_page (view)) {
			schedule_child_focus_in_idle (view, direction);
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
ev_view_focus (GtkWidget        *widget,
	       GtkDirectionType  direction)
{
	EvView *view = EV_VIEW (widget);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->document) {
		if (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD)
			return ev_view_focus_next (view, direction);
	}

	return GTK_WIDGET_CLASS (ev_view_parent_class)->focus (widget, direction);
}

static void
notify_scale_factor_cb (EvView     *view,
			GParamSpec *pspec)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->document)
		view_update_range_and_current_page (view);
}

static void
zoom_gesture_begin_cb (GtkGesture       *gesture,
		       GdkEventSequence *sequence,
		       EvView           *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->prev_zoom_gesture_scale = 1;
}

static void
zoom_gesture_scale_changed_cb (GtkGestureZoom *gesture,
			       gdouble         scale,
			       EvView         *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gdouble factor;

	priv->drag_info.in_drag = FALSE;
	priv->image_dnd_info.in_drag = FALSE;

	factor = scale - priv->prev_zoom_gesture_scale + 1;
	priv->prev_zoom_gesture_scale = scale;
	ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);

	gtk_gesture_get_bounding_box_center (GTK_GESTURE (gesture), &priv->zoom_center_x, &priv->zoom_center_y);

	if ((factor < 1.0 && ev_view_can_zoom_out (view)) ||
	    (factor >= 1.0 && ev_view_can_zoom_in (view)))
		ev_view_zoom (view, factor);
}

static void
ev_view_class_init (EvViewClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	object_class->get_property = ev_view_get_property;
	object_class->set_property = ev_view_set_property;
        object_class->dispose = ev_view_dispose;
	object_class->finalize = ev_view_finalize;

	widget_class->snapshot = ev_view_snapshot;
	widget_class->measure = ev_view_measure;
	widget_class->size_allocate = ev_view_size_allocate;
	widget_class->query_tooltip = ev_view_query_tooltip;
	widget_class->focus = ev_view_focus;

	gtk_widget_class_set_css_name (widget_class, "evview");

	class->scroll = ev_view_scroll;
	class->move_cursor = ev_view_move_cursor;
	class->activate = ev_view_activate;

	gtk_widget_class_set_template_from_resource (widget_class,
			"/org/gnome/evince/ui/view.ui");

	gtk_widget_class_bind_template_child_private (widget_class, EvView, zoom_gesture);
	gtk_widget_class_bind_template_child_private (widget_class, EvView, pan_gesture);

	gtk_widget_class_bind_template_callback (widget_class, ev_view_button_press_event);
	gtk_widget_class_bind_template_callback (widget_class, ev_view_button_release_event);
	gtk_widget_class_bind_template_callback (widget_class, ev_view_motion_notify_event);
	gtk_widget_class_bind_template_callback (widget_class, ev_view_enter_notify_event);
	gtk_widget_class_bind_template_callback (widget_class, ev_view_leave_notify_event);
	gtk_widget_class_bind_template_callback (widget_class, zoom_gesture_begin_cb);
	gtk_widget_class_bind_template_callback (widget_class, zoom_gesture_scale_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, notify_scale_factor_cb);
	gtk_widget_class_bind_template_callback (widget_class, ev_view_focus_in);
	gtk_widget_class_bind_template_callback (widget_class, ev_view_focus_out);
	gtk_widget_class_bind_template_callback (widget_class, middle_clicked_drag_begin_cb);
	gtk_widget_class_bind_template_callback (widget_class, middle_clicked_drag_end_cb);
	gtk_widget_class_bind_template_callback (widget_class, middle_clicked_drag_update_cb);
	gtk_widget_class_bind_template_callback (widget_class, ev_view_scroll_event);
	gtk_widget_class_bind_template_callback (widget_class, drag_prepare_cb);
	gtk_widget_class_bind_template_callback (widget_class, pan_gesture_pan_cb);
	gtk_widget_class_bind_template_callback (widget_class, pan_gesture_end_cb);

	/**
	 * EvView:is-loading:
	 *
	 * Allows to implement a custom notification system.
	 *
	 * Since: 3.8
	 */
	g_object_class_install_property (object_class,
					 PROP_IS_LOADING,
					 g_param_spec_boolean ("is-loading",
							       "Is Loading",
							       "Whether the view is loading",
							       FALSE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));
	/**
	 * EvView:can-zoom-in:
	 *
	 * Since: 3.8
	 */
	g_object_class_install_property (object_class,
					 PROP_CAN_ZOOM_IN,
					 g_param_spec_boolean ("can-zoom-in",
							       "Can Zoom In",
							       "Whether the view can be zoomed in further",
							       TRUE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));
	/**
	 * EvView:can-zoom-out:
	 *
	 * Since: 3.8
	 */
	g_object_class_install_property (object_class,
					 PROP_CAN_ZOOM_OUT,
					 g_param_spec_boolean ("can-zoom-out",
							       "Can Zoom Out",
							       "Whether the view can be zoomed out further",
							       TRUE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));

	/* Scrollable interface */
	g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	signals[SIGNAL_SCROLL] = g_signal_new ("scroll",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, scroll),
		         NULL, NULL,
		         ev_view_marshal_BOOLEAN__ENUM_ENUM,
		         G_TYPE_BOOLEAN, 2,
		         GTK_TYPE_SCROLL_TYPE,
		         GTK_TYPE_ORIENTATION);
	signals[SIGNAL_HANDLE_LINK] = g_signal_new ("handle-link",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, handle_link),
		         NULL, NULL,
		         NULL,
		         G_TYPE_NONE, 2,
		         G_TYPE_OBJECT, G_TYPE_OBJECT);
	signals[SIGNAL_EXTERNAL_LINK] = g_signal_new ("external-link",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, external_link),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__OBJECT,
		         G_TYPE_NONE, 1,
		         G_TYPE_OBJECT);
	signals[SIGNAL_POPUP_MENU] = g_signal_new ("popup",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, popup_menu),
		         NULL, NULL,
		         ev_view_marshal_VOID__POINTER_DOUBLE_DOUBLE,
		         G_TYPE_NONE, 3,
			 G_TYPE_POINTER,
			 G_TYPE_DOUBLE,
			 G_TYPE_DOUBLE);
	signals[SIGNAL_SELECTION_CHANGED] = g_signal_new ("selection-changed",
                         G_TYPE_FROM_CLASS (object_class),
                         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                         G_STRUCT_OFFSET (EvViewClass, selection_changed),
                         NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
                         G_TYPE_NONE, 0,
                         G_TYPE_NONE);
	signals[SIGNAL_SYNC_SOURCE] = g_signal_new ("sync-source",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, sync_source),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__BOXED,
		         G_TYPE_NONE, 1,
			 EV_TYPE_SOURCE_LINK | G_SIGNAL_TYPE_STATIC_SCOPE);
	signals[SIGNAL_ANNOT_ADDED] = g_signal_new ("annot-added",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, annot_added),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__OBJECT,
		         G_TYPE_NONE, 1,
			 EV_TYPE_ANNOTATION);
	signals[SIGNAL_ANNOT_CANCEL_ADD] = g_signal_new ("annot-cancel-add",
                         G_TYPE_FROM_CLASS (object_class),
                         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                         G_STRUCT_OFFSET (EvViewClass, annot_cancel_add),
                         NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
                         G_TYPE_NONE, 0,
                         G_TYPE_NONE);
	signals[SIGNAL_ANNOT_CHANGED] = g_signal_new ("annot-changed",
		         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, annot_changed),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__OBJECT,
		         G_TYPE_NONE, 1,
		         EV_TYPE_ANNOTATION);
	signals[SIGNAL_ANNOT_REMOVED] = g_signal_new ("annot-removed",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, annot_removed),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__OBJECT,
		         G_TYPE_NONE, 1,
 		         EV_TYPE_ANNOTATION);
	signals[SIGNAL_LAYERS_CHANGED] = g_signal_new ("layers-changed",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, layers_changed),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__VOID,
		         G_TYPE_NONE, 0,
			 G_TYPE_NONE);
	signals[SIGNAL_MOVE_CURSOR] = g_signal_new ("move-cursor",
		         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, move_cursor),
		         NULL, NULL,
		         ev_view_marshal_BOOLEAN__ENUM_INT_BOOLEAN,
		         G_TYPE_BOOLEAN, 3,
		         GTK_TYPE_MOVEMENT_STEP,
			 G_TYPE_INT,
			 G_TYPE_BOOLEAN);
	signals[SIGNAL_CURSOR_MOVED] = g_signal_new ("cursor-moved",
			 G_TYPE_FROM_CLASS (object_class),
			 G_SIGNAL_RUN_LAST,
		         0,
		         NULL, NULL,
		         ev_view_marshal_VOID__INT_INT,
		         G_TYPE_NONE, 2,
		         G_TYPE_INT,
			 G_TYPE_INT);
	signals[SIGNAL_ACTIVATE] = g_signal_new ("activate",
			 G_OBJECT_CLASS_TYPE (object_class),
			 G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			 G_STRUCT_OFFSET (EvViewClass, activate),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE, 0,
			 G_TYPE_NONE);


	gtk_widget_class_set_activate_signal (widget_class, signals[SIGNAL_ACTIVATE]);

	add_move_binding_keypad (widget_class, GDK_KEY_Left,  0, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
	add_move_binding_keypad (widget_class, GDK_KEY_Right, 0, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
	add_move_binding_keypad (widget_class, GDK_KEY_Left,  GDK_CONTROL_MASK, GTK_MOVEMENT_WORDS, -1);
	add_move_binding_keypad (widget_class, GDK_KEY_Right, GDK_CONTROL_MASK, GTK_MOVEMENT_WORDS, 1);
	add_move_binding_keypad (widget_class, GDK_KEY_Up,    0, GTK_MOVEMENT_DISPLAY_LINES, -1);
	add_move_binding_keypad (widget_class, GDK_KEY_Down,  0, GTK_MOVEMENT_DISPLAY_LINES, 1);
	add_move_binding_keypad (widget_class, GDK_KEY_Home,  0, GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);
	add_move_binding_keypad (widget_class, GDK_KEY_End,   0, GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
	add_move_binding_keypad (widget_class, GDK_KEY_Home,  GDK_CONTROL_MASK, GTK_MOVEMENT_BUFFER_ENDS, -1);
	add_move_binding_keypad (widget_class, GDK_KEY_End,   GDK_CONTROL_MASK, GTK_MOVEMENT_BUFFER_ENDS, 1);

        add_scroll_binding_keypad (widget_class, GDK_KEY_Left,  0, GTK_SCROLL_STEP_BACKWARD, GTK_ORIENTATION_HORIZONTAL);
        add_scroll_binding_keypad (widget_class, GDK_KEY_Right, 0, GTK_SCROLL_STEP_FORWARD, GTK_ORIENTATION_HORIZONTAL);
        add_scroll_binding_keypad (widget_class, GDK_KEY_Left,  GDK_ALT_MASK, GTK_SCROLL_STEP_DOWN, GTK_ORIENTATION_HORIZONTAL);
        add_scroll_binding_keypad (widget_class, GDK_KEY_Right, GDK_ALT_MASK, GTK_SCROLL_STEP_UP, GTK_ORIENTATION_HORIZONTAL);
        add_scroll_binding_keypad (widget_class, GDK_KEY_Up,    0, GTK_SCROLL_STEP_BACKWARD, GTK_ORIENTATION_VERTICAL);
        add_scroll_binding_keypad (widget_class, GDK_KEY_Down,  0, GTK_SCROLL_STEP_FORWARD, GTK_ORIENTATION_VERTICAL);
        add_scroll_binding_keypad (widget_class, GDK_KEY_Up,    GDK_ALT_MASK, GTK_SCROLL_STEP_DOWN, GTK_ORIENTATION_VERTICAL);
        add_scroll_binding_keypad (widget_class, GDK_KEY_Down,  GDK_ALT_MASK, GTK_SCROLL_STEP_UP, GTK_ORIENTATION_VERTICAL);
	add_scroll_binding_keypad (widget_class, GDK_KEY_Page_Up, 0, GTK_SCROLL_PAGE_BACKWARD, GTK_ORIENTATION_VERTICAL);
	add_scroll_binding_keypad (widget_class, GDK_KEY_Page_Down, 0, GTK_SCROLL_PAGE_FORWARD, GTK_ORIENTATION_VERTICAL);
	add_scroll_binding_keypad (widget_class, GDK_KEY_Home, GDK_CONTROL_MASK, GTK_SCROLL_START, GTK_ORIENTATION_VERTICAL);
	add_scroll_binding_keypad (widget_class, GDK_KEY_End, GDK_CONTROL_MASK, GTK_SCROLL_END, GTK_ORIENTATION_VERTICAL);

	/* We can't use the bindings defined in GtkWindow for Space and Return,
	 * because we also have those bindings for scrolling.
	 */
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0,
				      "activate", NULL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Space, 0,
				      "activate", NULL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0,
				      "activate", NULL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, 0,
				      "activate", NULL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0,
				      "activate", NULL);

	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0, "scroll",
				      "(ii)", GTK_SCROLL_PAGE_FORWARD, GTK_ORIENTATION_VERTICAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, GDK_SHIFT_MASK, "scroll",
				      "(ii)", GTK_SCROLL_PAGE_BACKWARD, GTK_ORIENTATION_VERTICAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_H, 0, "scroll",
				      "(ii)", GTK_SCROLL_STEP_BACKWARD, GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_J, 0, "scroll",
				      "(ii)", GTK_SCROLL_STEP_FORWARD, GTK_ORIENTATION_VERTICAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_K, 0, "scroll",
				      "(ii)", GTK_SCROLL_STEP_BACKWARD, GTK_ORIENTATION_VERTICAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_L, 0, "scroll",
				      "(ii)", GTK_SCROLL_STEP_FORWARD, GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0, "scroll",
				      "(ii)", GTK_SCROLL_PAGE_FORWARD, GTK_ORIENTATION_VERTICAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, GDK_SHIFT_MASK, "scroll",
				      "(ii)", GTK_SCROLL_PAGE_BACKWARD, GTK_ORIENTATION_VERTICAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_BackSpace, 0, "scroll",
				      "(ii)", GTK_SCROLL_PAGE_BACKWARD, GTK_ORIENTATION_VERTICAL);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_BackSpace, GDK_SHIFT_MASK, "scroll",
				      "(ii)",  GTK_SCROLL_PAGE_FORWARD, GTK_ORIENTATION_VERTICAL);
}

static void
ev_view_init (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->start_page = -1;
	priv->end_page = -1;
	priv->spacing = 14;
	priv->scale = 1.0;
	priv->current_page = -1;
	priv->pressed_button = -1;
	priv->cursor = EV_VIEW_CURSOR_NORMAL;
	priv->drag_info.in_drag = FALSE;
	priv->scroll_info.autoscrolling = FALSE;
	priv->selection_info.selections = NULL;
	priv->selection_info.in_drag = FALSE;
	priv->continuous = TRUE;
	priv->dual_even_left = TRUE;
	priv->sizing_mode = EV_SIZING_FIT_WIDTH;
	priv->page_layout = EV_PAGE_LAYOUT_SINGLE;
	priv->pending_scroll = SCROLL_TO_PAGE_POSITION;
	priv->pending_point.x = 0;
	priv->pending_point.y = 0;
	priv->find_page = -1;
	priv->jump_to_find_result = TRUE;
	priv->highlight_find_results = FALSE;
	priv->pixbuf_cache_size = DEFAULT_PIXBUF_CACHE_SIZE;
	priv->caret_enabled = FALSE;
	priv->cursor_page = 0;
	priv->allow_links_change_zoom = TRUE;
	priv->window_children = NULL;
	priv->zoom_center_x = -1;
	priv->zoom_center_y = -1;

	gtk_widget_init_template (GTK_WIDGET (view));
}

/*** Callbacks ***/

static void
ev_view_change_page (EvView *view,
		     gint    new_page)
{
	gint x, y;
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->current_page = new_page;
	priv->pending_scroll = SCROLL_TO_PAGE_POSITION;

	ev_view_set_loading (view, FALSE);

	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y, FALSE);

	gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
job_finished_cb (EvPixbufCache  *pixbuf_cache,
		 cairo_region_t *region,
		 EvView         *view)
{
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
ev_view_page_changed_cb (EvDocumentModel *model,
			 gint             old_page,
			 gint             new_page,
			 EvView          *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (!priv->document)
		return;

	if (priv->current_page != new_page) {
		ev_view_change_page (view, new_page);
	} else {
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}

#if 0
static gboolean
cursor_scroll_update (gpointer data)
{
	EvView *view = data;
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint x, y;

	priv->update_cursor_idle_id = 0;
	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y, FALSE);

	return FALSE;
}

static void
schedule_scroll_cursor_update (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (priv->update_cursor_idle_id)
		return;

	priv->update_cursor_idle_id =
		g_idle_add (cursor_scroll_update, view);
}
#endif

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
			     EvView        *view)
{
	GtkWidget *widget = GTK_WIDGET (view);
	EvViewPrivate *priv = GET_PRIVATE (view);
	int dx = 0, dy = 0;
	gint value;

	if (!gtk_widget_get_realized (widget))
		return;

	/* If the adjustment value is set during a drag event, update the drag
	 * start position so it can continue from the new location. */
	if (priv->drag_info.in_drag && !priv->drag_info.in_notify) {
		priv->drag_info.hadj += gtk_adjustment_get_value (priv->hadjustment) - priv->scroll_x;
		priv->drag_info.vadj += gtk_adjustment_get_value (priv->vadjustment) - priv->scroll_y;
	}

	if (priv->hadjustment) {
		value = (gint) gtk_adjustment_get_value (priv->hadjustment);
		dx = priv->scroll_x - value;
		priv->scroll_x = value;
	} else {
		priv->scroll_x = 0;
	}

	if (priv->vadjustment) {
		value = (gint) gtk_adjustment_get_value (priv->vadjustment);
		dy = priv->scroll_y - value;
		priv->scroll_y = value;
	} else {
		priv->scroll_y = 0;
	}

	for (GtkWidget *child = gtk_widget_get_first_child (widget);
		child != NULL;
		child = gtk_widget_get_next_sibling (child)) {
		EvViewChild *data = g_object_get_data (G_OBJECT (child), "ev-child");

		if (!data)
			continue;

		data->x += dx;
		data->y += dy;
		if (gtk_widget_get_visible (child) && gtk_widget_get_visible (widget))
			gtk_widget_queue_resize (widget);
	}

	if (priv->pending_resize) {
		gtk_widget_queue_draw (widget);
	}

#if 0
	cursor_updated = FALSE;
	event = gtk_get_current_event ();
	if (event) {
		if (event->type == GDK_SCROLL &&
		    gdk_event_get_window (event) == gtk_widget_get_window (widget)) {
			gdk_event_get_coords (event, &x, &y);
			ev_view_handle_cursor_over_xy (view, (gint) x, (gint) y);
			cursor_updated = TRUE;
		}
		gdk_event_free (event);
	}

	if (!cursor_updated)
		schedule_scroll_cursor_update (view);
#endif

	if (priv->document)
		view_update_range_and_current_page (view);
}

EvView *
ev_view_new (void)
{
	return g_object_new (EV_TYPE_VIEW, NULL);
}

static void
setup_caches (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	priv->height_to_page_cache = ev_view_get_height_to_page_cache (view);
	priv->pixbuf_cache = ev_pixbuf_cache_new (GTK_WIDGET (view), priv->model, priv->pixbuf_cache_size);
	priv->page_cache = ev_page_cache_new (priv->document);

	ev_page_cache_set_flags (priv->page_cache,
				 ev_page_cache_get_flags (priv->page_cache) |
				 EV_PAGE_DATA_INCLUDE_TEXT_LAYOUT |
				 EV_PAGE_DATA_INCLUDE_TEXT |
				 EV_PAGE_DATA_INCLUDE_TEXT_ATTRS |
		                 EV_PAGE_DATA_INCLUDE_TEXT_LOG_ATTRS);

	g_signal_connect (priv->pixbuf_cache, "job-finished", G_CALLBACK (job_finished_cb), view);
}

static void
clear_caches (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	g_clear_object (&priv->pixbuf_cache);
	g_clear_object (&priv->page_cache);
}

/**
 * ev_view_set_page_cache_size:
 * @view: #EvView instance
 * @cache_size: size in bytes
 *
 * Sets the maximum size in bytes that will be used to cache
 * rendered pages. Use 0 to disable caching rendered pages.
 *
 * Note that this limit doesn't affect the current visible page range,
 * which will always be rendered. In order to limit the total memory used
 * you have to use ev_document_model_set_max_scale() too.
 *
 */
void
ev_view_set_page_cache_size (EvView *view,
			     gsize   cache_size)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	if (priv->pixbuf_cache_size == cache_size)
		return;

	priv->pixbuf_cache_size = cache_size;
	if (priv->pixbuf_cache)
		ev_pixbuf_cache_set_max_size (priv->pixbuf_cache, cache_size);

	view_update_scale_limits (view);
}

/**
 * ev_view_is_loading:
 * @view:
 *
 * Returns: %TRUE iff the view is currently loading a document
 *
 * Since: 3.8
 */
gboolean
ev_view_is_loading (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	return priv->loading;
}

void
ev_view_autoscroll_start (EvView *view)
{
	gint x, y;

	g_return_if_fail (EV_IS_VIEW (view));
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->scroll_info.autoscrolling)
		return;

	priv->scroll_info.autoscrolling = TRUE;
	ev_view_autoscroll_resume (view);

	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y, FALSE);
}

void
ev_view_autoscroll_stop (EvView *view)
{
	gint x, y;

	g_return_if_fail (EV_IS_VIEW (view));
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->scroll_info.autoscrolling)
		return;

	priv->scroll_info.autoscrolling = FALSE;
	ev_view_autoscroll_pause (view);

	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y, FALSE);
}

static void
ev_view_document_changed_cb (EvDocumentModel *model,
			     GParamSpec      *pspec,
			     EvView          *view)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (document != priv->document) {
		gint current_page;

		ev_view_remove_all (view);
		clear_caches (view);

		if (priv->document) {
			g_object_unref (priv->document);
                }

		priv->document = document ? g_object_ref (document) : NULL;
		priv->find_page = -1;
		priv->find_result = 0;

		if (priv->document) {
			if (ev_document_get_n_pages (priv->document) <= 0 ||
			    !ev_document_check_dimensions (priv->document))
				return;

			ev_view_set_loading (view, FALSE);
			setup_caches (view);

			if (priv->caret_enabled)
				preload_pages_for_caret_navigation (view);
		}

		current_page = ev_document_model_get_page (model);
		if (priv->current_page != current_page) {
			ev_view_change_page (view, current_page);
		} else {
			priv->pending_scroll = SCROLL_TO_KEEP_POSITION;
			gtk_widget_queue_resize (GTK_WIDGET (view));
		}

		view_update_scale_limits (view);
	}
}

static void
ev_view_rotation_changed_cb (EvDocumentModel *model,
			     GParamSpec      *pspec,
			     EvView          *view)
{
	gint rotation = ev_document_model_get_rotation (model);
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->rotation = rotation;

	if (priv->pixbuf_cache) {
		ev_pixbuf_cache_clear (priv->pixbuf_cache);
		if (!ev_document_is_page_size_uniform (priv->document))
			priv->pending_scroll = SCROLL_TO_PAGE_POSITION;
		gtk_widget_queue_resize (GTK_WIDGET (view));
	}

	ev_view_remove_all (view);
	view_update_scale_limits (view);

	if (rotation != 0)
		clear_selection (view);
}

static void
ev_view_inverted_colors_changed_cb (EvDocumentModel *model,
				    GParamSpec      *pspec,
				    EvView          *view)
{
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
ev_view_sizing_mode_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvView          *view)
{
	EvSizingMode mode = ev_document_model_get_sizing_mode (model);
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->sizing_mode = mode;
	if (mode != EV_SIZING_FREE)
		gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
update_can_zoom (EvView *view)
{
	gdouble min_scale;
	gdouble max_scale;
	gboolean can_zoom_in;
	gboolean can_zoom_out;
	EvViewPrivate *priv = GET_PRIVATE (view);

	min_scale = ev_document_model_get_min_scale (priv->model);
	max_scale = ev_document_model_get_max_scale (priv->model);

	can_zoom_in = priv->scale <= max_scale;
	can_zoom_out = priv->scale > min_scale;

	if (can_zoom_in != priv->can_zoom_in) {
		priv->can_zoom_in = can_zoom_in;
		g_object_notify (G_OBJECT (view), "can-zoom-in");
	}

	if (can_zoom_out != priv->can_zoom_out) {
		priv->can_zoom_out = can_zoom_out;
		g_object_notify (G_OBJECT (view), "can-zoom-out");
	}
}

static void
ev_view_page_layout_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvView          *view)
{
	EvPageLayout layout = ev_document_model_get_page_layout (model);
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->page_layout = layout;

	priv->pending_scroll = SCROLL_TO_PAGE_POSITION;
	gtk_widget_queue_resize (GTK_WIDGET (view));

	/* FIXME: if we're keeping the pixbuf cache around, we should extend the
	 * preload_cache_size to be 2 if dual_page is set.
	 */
}

#define EPSILON 0.0000001
static void
ev_view_scale_changed_cb (EvDocumentModel *model,
			  GParamSpec      *pspec,
			  EvView          *view)
{
	gdouble scale = ev_document_model_get_scale (model);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (ABS (priv->scale - scale) < EPSILON)
		return;

	priv->scale = scale;

	priv->pending_resize = TRUE;
	if (priv->sizing_mode == EV_SIZING_FREE)
		gtk_widget_queue_resize (GTK_WIDGET (view));

	update_can_zoom (view);
}

static void
ev_view_min_scale_changed_cb (EvDocumentModel *model,
			      GParamSpec      *pspec,
			      EvView          *view)
{
	update_can_zoom (view);
}

static void
ev_view_max_scale_changed_cb (EvDocumentModel *model,
			      GParamSpec      *pspec,
			      EvView          *view)
{
	update_can_zoom (view);
}

static void
ev_view_continuous_changed_cb (EvDocumentModel *model,
			       GParamSpec      *pspec,
			       EvView          *view)
{
	gboolean continuous = ev_document_model_get_continuous (model);
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->document) {
		GdkPoint     view_point;
		GdkRectangle page_area;
		GtkBorder    border;

		view_point.x = priv->scroll_x;
		view_point.y = priv->scroll_y;
		ev_view_get_page_extents (view, priv->start_page, &page_area, &border);
		_ev_view_transform_view_point_to_doc_point (view, &view_point,
							    &page_area, &border,
							    &priv->pending_point.x,
							    &priv->pending_point.y);
	}
	priv->continuous = continuous;
	priv->pending_scroll = SCROLL_TO_PAGE_POSITION;
	gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
ev_view_dual_odd_left_changed_cb (EvDocumentModel *model,
				  GParamSpec      *pspec,
				  EvView          *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	priv->dual_even_left = !ev_document_model_get_dual_page_odd_pages_left (model);
	priv->pending_scroll = SCROLL_TO_PAGE_POSITION;
	if (ev_document_model_get_page_layout (model) == EV_PAGE_LAYOUT_DUAL)
		/* odd_left may be set when not in dual mode,
		   queue_resize is not needed in that case */
		gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
ev_view_direction_changed_cb (EvDocumentModel *model,
                              GParamSpec      *pspec,
                              EvView          *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	gboolean rtl = ev_document_model_get_rtl (model);
	gtk_widget_set_direction (GTK_WIDGET (view), rtl ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
	priv->pending_scroll = SCROLL_TO_PAGE_POSITION;
	gtk_widget_queue_resize (GTK_WIDGET (view));
}

void
ev_view_set_model (EvView          *view,
		   EvDocumentModel *model)
{
	g_return_if_fail (EV_IS_VIEW (view));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (model == priv->model)
		return;

	if (priv->model) {
		g_signal_handlers_disconnect_by_data (priv->model, view);
		g_object_unref (priv->model);
	}
	priv->model = g_object_ref (model);

	/* Initialize view from model */
	priv->rotation = ev_document_model_get_rotation (priv->model);
	priv->sizing_mode = ev_document_model_get_sizing_mode (priv->model);
	priv->scale = ev_document_model_get_scale (priv->model);
	priv->continuous = ev_document_model_get_continuous (priv->model);
	priv->page_layout = ev_document_model_get_page_layout (priv->model);
	gtk_widget_set_direction (GTK_WIDGET(view), ev_document_model_get_rtl (priv->model));
	ev_view_document_changed_cb (priv->model, NULL, view);

	g_signal_connect (priv->model, "notify::document",
			  G_CALLBACK (ev_view_document_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::rotation",
			  G_CALLBACK (ev_view_rotation_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::inverted-colors",
			  G_CALLBACK (ev_view_inverted_colors_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::sizing-mode",
			  G_CALLBACK (ev_view_sizing_mode_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::page-layout",
			  G_CALLBACK (ev_view_page_layout_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::scale",
			  G_CALLBACK (ev_view_scale_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::min-scale",
			  G_CALLBACK (ev_view_min_scale_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::max-scale",
			  G_CALLBACK (ev_view_max_scale_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::continuous",
			  G_CALLBACK (ev_view_continuous_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::dual-odd-left",
			  G_CALLBACK (ev_view_dual_odd_left_changed_cb),
			  view);
	g_signal_connect (priv->model, "notify::rtl",
			  G_CALLBACK (ev_view_direction_changed_cb),
			  view);
	g_signal_connect (priv->model, "page-changed",
			  G_CALLBACK (ev_view_page_changed_cb),
			  view);
#if 0
	if (priv->accessible)
		ev_view_accessible_set_model (EV_VIEW_ACCESSIBLE (priv->accessible),
					      priv->model);
#endif
}

static void
ev_view_reload_page (EvView         *view,
		     gint            page,
		     cairo_region_t *region)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	ev_pixbuf_cache_reload_page (priv->pixbuf_cache,
				     region,
				     page,
				     priv->rotation,
				     priv->scale);
}

void
ev_view_reload (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	ev_pixbuf_cache_clear (priv->pixbuf_cache);
	view_update_range_and_current_page (view);
}

/*** Zoom and sizing mode ***/

static gboolean
ev_view_can_zoom (EvView *view, gdouble factor)
{
	if (factor == 1.0)
		return TRUE;

	else if (factor < 1.0) {
		return ev_view_can_zoom_out (view);
	} else {
		return ev_view_can_zoom_in (view);
	}
}

gboolean
ev_view_can_zoom_in (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	return priv->can_zoom_in;
}

gboolean
ev_view_can_zoom_out (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	return priv->can_zoom_out;
}

static void
ev_view_zoom (EvView *view, gdouble factor)
{
	gdouble scale;
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_return_if_fail (priv->sizing_mode == EV_SIZING_FREE);

	priv->pending_scroll = SCROLL_TO_CENTER;
	scale = ev_document_model_get_scale (priv->model) * factor;
	ev_document_model_set_scale (priv->model, scale);
}

void
ev_view_zoom_in (EvView *view)
{
	ev_view_zoom (view, ZOOM_IN_FACTOR);
}

void
ev_view_zoom_out (EvView *view)
{
	ev_view_zoom (view, ZOOM_OUT_FACTOR);
}

static double
zoom_for_size_fit_width (gdouble doc_width,
			 gdouble doc_height,
			 int     target_width,
			 int     target_height)
{
	return (double)target_width / doc_width;
}

static double
zoom_for_size_fit_height (gdouble doc_width,
			  gdouble doc_height,
			  int     target_width,
			  int     target_height)
{
	return (double)target_height / doc_height;
}

static double
zoom_for_size_fit_page (gdouble doc_width,
			gdouble doc_height,
			int     target_width,
			int     target_height)
{
	double w_scale;
	double h_scale;

	w_scale = (double)target_width / doc_width;
	h_scale = (double)target_height / doc_height;

	return MIN (w_scale, h_scale);
}

static double
zoom_for_size_automatic (GtkWidget *widget,
			 gdouble    doc_width,
			 gdouble    doc_height,
			 int        target_width,
			 int        target_height)
{
	double fit_width_scale;
	double scale;

	fit_width_scale = zoom_for_size_fit_width (doc_width, doc_height, target_width, target_height);

	if (doc_height < doc_width) {
		double fit_height_scale;

		fit_height_scale = zoom_for_size_fit_height (doc_width, doc_height, target_width, target_height);
		scale = MIN (fit_width_scale, fit_height_scale);
	} else {
		double actual_scale;

		actual_scale = ev_document_misc_get_widget_dpi (widget) / 72.0;
		scale = MIN (fit_width_scale, actual_scale);
	}

	return scale;
}

static void
ev_view_zoom_for_size_continuous_and_dual_page (EvView *view,
						int     width,
						int     height)
{
	gdouble doc_width, doc_height;
	GtkBorder border;
	gdouble scale;
	EvViewPrivate *priv = GET_PRIVATE (view);

	ev_document_get_max_page_size (priv->document, &doc_width, &doc_height);
	if (priv->rotation == 90 || priv->rotation == 270) {
		gdouble tmp;

		tmp = doc_width;
		doc_width = doc_height;
		doc_height = tmp;
	}

	compute_border (view, &border);

	doc_width *= 2;
	width -= (2 * (border.left + border.right) + 3 * priv->spacing);
	height -= (border.top + border.bottom + 2 * priv->spacing);

	switch (priv->sizing_mode) {
	case EV_SIZING_FIT_WIDTH:
		scale = zoom_for_size_fit_width (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_FIT_PAGE:
		scale = zoom_for_size_fit_page (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_AUTOMATIC:
		scale = zoom_for_size_automatic (GTK_WIDGET (view),
						 doc_width, doc_height, width, height);
		break;
	default:
		g_assert_not_reached ();
	}

	ev_document_model_set_scale (priv->model, scale);
}

static void
ev_view_zoom_for_size_continuous (EvView *view,
				  int     width,
				  int     height)
{
	gdouble doc_width, doc_height;
	GtkBorder border;
	gdouble scale;
	EvViewPrivate *priv = GET_PRIVATE (view);

	ev_document_get_max_page_size (priv->document, &doc_width, &doc_height);
	if (priv->rotation == 90 || priv->rotation == 270) {
		gdouble tmp;

		tmp = doc_width;
		doc_width = doc_height;
		doc_height = tmp;
	}

	compute_border (view, &border);

	width -= (border.left + border.right + 2 * priv->spacing);
	height -= (border.top + border.bottom + 2 * priv->spacing);

	switch (priv->sizing_mode) {
	case EV_SIZING_FIT_WIDTH:
		scale = zoom_for_size_fit_width (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_FIT_PAGE:
		scale = zoom_for_size_fit_page (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_AUTOMATIC:
		scale = zoom_for_size_automatic (GTK_WIDGET (view),
						 doc_width, doc_height, width, height);
		break;
	default:
		g_assert_not_reached ();
	}

	ev_document_model_set_scale (priv->model, scale);
}

static void
ev_view_zoom_for_size_dual_page (EvView *view,
				 int     width,
				 int     height)
{
	GtkBorder border;
	gdouble doc_width, doc_height;
	gdouble scale;
	gint other_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	other_page = priv->current_page ^ 1;

	/* Find the largest of the two. */
	get_doc_page_size (view, priv->current_page, &doc_width, &doc_height);
	if (other_page < ev_document_get_n_pages (priv->document)) {
		gdouble width_2, height_2;

		get_doc_page_size (view, other_page, &width_2, &height_2);
		if (width_2 > doc_width)
			doc_width = width_2;
		if (height_2 > doc_height)
			doc_height = height_2;
	}
	compute_border (view, &border);

	doc_width = doc_width * 2;
	width -= ((border.left + border.right)* 2 + 3 * priv->spacing);
	height -= (border.top + border.bottom + 2 * priv->spacing);

	switch (priv->sizing_mode) {
	case EV_SIZING_FIT_WIDTH:
		scale = zoom_for_size_fit_width (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_FIT_PAGE:
		scale = zoom_for_size_fit_page (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_AUTOMATIC:
		scale = zoom_for_size_automatic (GTK_WIDGET (view),
						 doc_width, doc_height, width, height);
		break;
	default:
		g_assert_not_reached ();
	}

	ev_document_model_set_scale (priv->model, scale);
}

static void
ev_view_zoom_for_size_single_page (EvView *view,
				   int     width,
				   int     height)
{
	gdouble doc_width, doc_height;
	GtkBorder border;
	gdouble scale;
	EvViewPrivate *priv = GET_PRIVATE (view);

	get_doc_page_size (view, priv->current_page, &doc_width, &doc_height);

	/* Get an approximate border */
	compute_border (view, &border);

	width -= (border.left + border.right + 2 * priv->spacing);
	height -= (border.top + border.bottom + 2 * priv->spacing);

	switch (priv->sizing_mode) {
	case EV_SIZING_FIT_WIDTH:
		scale = zoom_for_size_fit_width (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_FIT_PAGE:
		scale = zoom_for_size_fit_page (doc_width, doc_height, width, height);
		break;
	case EV_SIZING_AUTOMATIC:
		scale = zoom_for_size_automatic (GTK_WIDGET (view),
						 doc_width, doc_height, width, height);
		break;
	default:
		g_assert_not_reached ();
	}

	ev_document_model_set_scale (priv->model, scale);
}

static void
ev_view_zoom_for_size (EvView *view,
		       int     width,
		       int     height)
{
	gboolean dual_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_return_if_fail (EV_IS_VIEW (view));
	g_return_if_fail (priv->sizing_mode == EV_SIZING_FIT_WIDTH ||
			  priv->sizing_mode == EV_SIZING_FIT_PAGE ||
			  priv->sizing_mode == EV_SIZING_AUTOMATIC);
	g_return_if_fail (width >= 0);
	g_return_if_fail (height >= 0);


	if (priv->document == NULL)
		return;

	dual_page = is_dual_page (view, NULL);
	if (priv->continuous && dual_page)
		ev_view_zoom_for_size_continuous_and_dual_page (view, width, height);
	else if (priv->continuous)
		ev_view_zoom_for_size_continuous (view, width, height);
	else if (dual_page)
		ev_view_zoom_for_size_dual_page (view, width, height);
	else
		ev_view_zoom_for_size_single_page (view, width, height);
}

static gboolean
ev_view_page_fits (EvView         *view,
		   GtkOrientation  orientation)
{
	GtkRequisition requisition;
	double         size;
	EvViewPrivate *priv = GET_PRIVATE (view);
	int widget_width = gtk_widget_get_width (GTK_WIDGET (view));
	int widget_height = gtk_widget_get_height (GTK_WIDGET (view));

	if (priv->sizing_mode == EV_SIZING_FIT_PAGE)
		return TRUE;

	if (orientation == GTK_ORIENTATION_HORIZONTAL &&
	    (priv->sizing_mode == EV_SIZING_FIT_WIDTH ||
	     priv->sizing_mode == EV_SIZING_AUTOMATIC))
		return TRUE;

	ev_view_size_request (GTK_WIDGET (view), &requisition);

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		if (requisition.width == 1) {
			size = 1.0;
		} else {
			if (widget_width > 0.0)
				size = (double) requisition.width / widget_width;
			else
				size = 1.0;
		}
	} else {
		if (requisition.height == 1) {
			size = 1.0;
		} else {
			if (widget_height > 0.0)
				size = (double) requisition.height / widget_height;
			else
				size = 1.0;
		}
	}

	return size <= 1.0;
}

/*** Find ***/
static gint
ev_view_find_get_n_results (EvView *view, gint page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	return priv->find_pages ? g_list_length (priv->find_pages[page]) : 0;
}

static EvFindRectangle *
ev_view_find_get_result (EvView *view, gint page, gint result)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	return priv->find_pages ? (EvFindRectangle *) g_list_nth_data (priv->find_pages[page], result) : NULL;
}

static gboolean
ev_view_find_is_next_line (EvView *view, gint page, gint result)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->find_pages)
		return FALSE;

	GList *elem = g_list_nth (priv->find_pages[page], result);
	return elem && ((EvFindRectangle *) elem->data)->next_line;
}

static void
jump_to_find_result (EvView *view)
{
	EvRectangle *rect;
	gint n_results;
	EvViewPrivate *priv = GET_PRIVATE (view);
	gint page = priv->find_page;

	n_results = ev_view_find_get_n_results (view, page);
	rect = ev_rectangle_new ();

	if (n_results > 0 && priv->find_result < n_results) {
		EvFindRectangle *find_rect, *rect_next;
		GdkRectangle view_rect;

		rect_next = NULL;
		find_rect = ev_view_find_get_result (view, page, priv->find_result);
		if (find_rect->next_line) {
			/* For an across-lines match, make sure both rectangles are visible */
			rect_next = ev_view_find_get_result (view, page, priv->find_result + 1);
			rect->x1 = MIN (find_rect->x1, rect_next->x1);
			rect->y1 = MIN (find_rect->y1, rect_next->y1);
			rect->x2 = MAX (find_rect->x2, rect_next->x2);
			rect->y2 = MAX (find_rect->y2, rect_next->y2);
		} else {
			rect->x1 = find_rect->x1;
			rect->y1 = find_rect->y1;
			rect->x2 = find_rect->x2;
			rect->y2 = find_rect->y2;
		}
		_ev_view_transform_doc_rect_to_view_rect (view, page, rect, &view_rect);
		_ev_view_ensure_rectangle_is_visible (view, &view_rect);
		if (priv->caret_enabled && priv->rotation == 0)
			position_caret_cursor_at_doc_point (view, page, find_rect->x1, find_rect->y1);

		priv->jump_to_find_result = FALSE;
	}

	ev_rectangle_free (rect);
}

/**
 * jump_to_find_page:
 * @view: #EvView instance
 * @direction: Direction to look
 * @shift: Shift from current page
 *
 * Jumps to the first page that has occurrences of searched word.
 * Uses a direction where to look and a shift from current page.
 *
 */
static void
jump_to_find_page (EvView *view, EvViewFindDirection direction, gint shift)
{
	int n_pages, i;
	EvViewPrivate *priv = GET_PRIVATE (view);

	n_pages = ev_document_get_n_pages (priv->document);

	for (i = 0; i < n_pages; i++) {
		int page;

		if (direction == EV_VIEW_FIND_NEXT)
			page = priv->find_page + i;
		else
			page = priv->find_page - i;
		page += shift;

		if (page >= n_pages)
			page = page - n_pages;
		else if (page < 0)
			page = page + n_pages;

		if (priv->find_pages && priv->find_pages[page]) {
			priv->find_page = page;
			break;
		}
	}

	if (!priv->continuous)
		ev_document_model_set_page (priv->model, priv->find_page);
}

static void
find_job_updated_cb (EvJobFind *job, gint page, EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->find_pages = ev_job_find_get_results (job);
	if (priv->find_page == -1)
		priv->find_page = priv->current_page;

	if (priv->jump_to_find_result == TRUE) {
		jump_to_find_page (view, EV_VIEW_FIND_NEXT, 0);
		jump_to_find_result (view);
	}

	if (priv->find_page == page)
		gtk_widget_queue_draw (GTK_WIDGET (view));
}

/**
 * ev_view_find_started:
 * @view:
 * @job:
 *
 * Since: 3.6
 */
void
ev_view_find_started (EvView *view, EvJobFind *job)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->find_job == job)
		return;

	ev_view_find_cancel (view);
	priv->find_job = g_object_ref (job);
	priv->find_page = priv->current_page;
	priv->find_result = 0;

	g_signal_connect (job, "updated", G_CALLBACK (find_job_updated_cb), view);
}

/**
 * ev_view_find_restart:
 * @view: an #EvView
 * @page: a page index
 *
 * Restart the current search operation from the given @page.
 *
 * Since: 3.12
 */
void
ev_view_find_restart (EvView *view,
		      gint    page)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!priv->find_job)
		return;

	priv->find_page = page;
	priv->find_result = 0;
	jump_to_find_page (view, EV_VIEW_FIND_NEXT, 0);
	jump_to_find_result (view);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_find_next (EvView *view)
{
	gint n_results;
	EvViewPrivate *priv = GET_PRIVATE (view);

	n_results = ev_view_find_get_n_results (view, priv->find_page);
	priv->find_result += ev_view_find_is_next_line (view, priv->find_page, priv->find_result)
	                     ? 2 : 1;

	if (priv->find_result >= n_results) {
		priv->find_result = 0;
		jump_to_find_page (view, EV_VIEW_FIND_NEXT, 1);
	} else if (priv->find_page != priv->current_page) {
		jump_to_find_page (view, EV_VIEW_FIND_NEXT, 0);
	}

	jump_to_find_result (view);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_find_previous (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->find_result -= ev_view_find_is_next_line (view, priv->find_page, priv->find_result - 2)
	                     ? 2 : 1;

	if (priv->find_result < 0) {
		jump_to_find_page (view, EV_VIEW_FIND_PREV, -1);
		priv->find_result = MAX (0, ev_view_find_get_n_results (view, priv->find_page) - 1);
		if (priv->find_result && ev_view_find_is_next_line (view, priv->find_page, priv->find_result))
			priv->find_result--; /* set to last "non-nextline" result */
	} else if (priv->find_page != priv->current_page) {
		jump_to_find_page (view, EV_VIEW_FIND_PREV, 0);
	}

	jump_to_find_result (view);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

/**
 * ev_view_find_set_result:
 * @view: a #EvView
 * @page:
 * @result:
 *
 * FIXME
 *
 * Since: 3.10
 */
void
ev_view_find_set_result (EvView *view, gint page, gint result)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->find_page = page;
	priv->find_result = result;
	jump_to_find_page (view, EV_VIEW_FIND_NEXT, 0);
	jump_to_find_result (view);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_find_search_changed (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	/* search string has changed, focus on new search result */
	priv->jump_to_find_result = TRUE;
	ev_view_find_cancel (view);
}

void
ev_view_find_set_highlight_search (EvView *view, gboolean value)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->highlight_find_results = value;
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_find_cancel (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	priv->find_pages = NULL;
	priv->find_page = -1;
	priv->find_result = 0;

	if (!priv->find_job)
		return;

	g_signal_handlers_disconnect_by_func (priv->find_job, find_job_updated_cb, view);
	g_clear_object (&priv->find_job);
}

/*** Synctex ***/
void
ev_view_highlight_forward_search (EvView       *view,
				  EvSourceLink *link)
{
	EvMapping   *mapping;
	gint         page;
	GdkRectangle view_rect;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!ev_document_has_synctex (priv->document))
		return;

	mapping = ev_document_synctex_forward_search (priv->document, link);
	if (!mapping)
		return;

	if (priv->synctex_result)
		g_free (priv->synctex_result);
	priv->synctex_result = mapping;

	page = GPOINTER_TO_INT (mapping->data);
	ev_document_model_set_page (priv->model, page);

	_ev_view_transform_doc_rect_to_view_rect (view, page, &mapping->area, &view_rect);
	_ev_view_ensure_rectangle_is_visible (view, &view_rect);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

/*** Selections ***/
static gboolean
gdk_rectangle_point_in (GdkRectangle *rectangle,
			GdkPoint     *point)
{
	return rectangle->x <= point->x &&
		rectangle->y <= point->y &&
		point->x < rectangle->x + rectangle->width &&
		point->y < rectangle->y + rectangle->height;
}

static inline gboolean
gdk_point_equal (GdkPoint *a,
		 GdkPoint *b)
{
	return a->x == b->x && a->y == b->y;
}

static gboolean
get_selection_page_range (EvView          *view,
			  EvSelectionStyle style,
			  GdkPoint        *start,
			  GdkPoint        *stop,
			  gint            *first_page,
			  gint            *last_page)
{
	gint start_page, end_page;
	gint first, last;
	gint i, n_pages;
	GtkBorder border;
	EvViewPrivate *priv = GET_PRIVATE (view);

	n_pages = ev_document_get_n_pages (priv->document);

	if (gdk_point_equal (start, stop)) {
		start_page = priv->start_page;
		end_page = priv->end_page;
	} else if (priv->continuous) {
		start_page = 0;
		end_page = n_pages - 1;
	} else if (is_dual_page (view, NULL)) {
		start_page = priv->start_page;
		end_page = priv->end_page;
	} else {
		start_page = priv->current_page;
		end_page = priv->current_page;
	}

	first = -1;
	last = -1;
	compute_border (view, &border);
	for (i = start_page; i <= end_page; i++) {
		GdkRectangle page_area;

		ev_view_get_page_extents_for_border (view, i, &border, &page_area);
		page_area.x -= border.left;
		page_area.y -= border.top;
		page_area.width += border.left + border.right;
		page_area.height += border.top + border.bottom;
		if (gdk_rectangle_point_in (&page_area, start) ||
		    gdk_rectangle_point_in (&page_area, stop)) {
			if (first == -1)
				first = i;
			last = i;
		}
	}

	if (first != -1 && last != -1) {
		*first_page = first;
		*last_page = last;

		return TRUE;
	}

	return FALSE;
}

static GList *
compute_new_selection (EvView          *view,
		       EvSelectionStyle style,
		       GdkPoint        *start,
		       GdkPoint        *stop)
{
	int i, first, last;
	GtkBorder border;
	GList *list = NULL;

	/* First figure out the range of pages the selection affects. */
	if (!get_selection_page_range (view, style, start, stop, &first, &last))
		return list;

	/* Now create a list of EvViewSelection's for the affected
	 * pages. This could be an empty list, a list of just one
	 * page or a number of pages.*/
	compute_border (view, &border);
	for (i = first; i <= last; i++) {
		EvViewSelection *selection;
		GdkRectangle     page_area;
		GdkPoint        *point;
		gdouble          width, height;

		get_doc_page_size (view, i, &width, &height);

		selection = g_slice_new0 (EvViewSelection);
		selection->page = i;
		selection->style = style;
		selection->rect.x1 = selection->rect.y1 = 0;
		selection->rect.x2 = width;
		selection->rect.y2 = height;

		ev_view_get_page_extents_for_border (view, i, &border, &page_area);
		if (gdk_rectangle_point_in (&page_area, start))
			point = start;
		else
			point = stop;

		if (i == first) {
			_ev_view_transform_view_point_to_doc_point (view, point,
								    &page_area, &border,
								    &selection->rect.x1,
								    &selection->rect.y1);
		}

		/* If the selection is contained within just one page,
		 * make sure we don't write 'start' into both points
		 * in selection->rect. */
		if (first == last)
			point = stop;

		if (i == last) {
			_ev_view_transform_view_point_to_doc_point (view, point,
								    &page_area, &border,
								    &selection->rect.x2,
								    &selection->rect.y2);
		}

		list = g_list_prepend (list, selection);
	}

	return g_list_reverse (list);
}

/* This function takes the newly calculated list, and figures out which regions
 * have changed.  It then queues a redraw appropriately.
 */
static void
merge_selection_region (EvView *view,
			GList  *new_list)
{
	GList *old_list;
	GList *new_list_ptr, *old_list_ptr;
	EvViewPrivate *priv = GET_PRIVATE (view);

	/* Update the selection */
	old_list = ev_pixbuf_cache_get_selection_list (priv->pixbuf_cache);
	g_list_free_full (priv->selection_info.selections, (GDestroyNotify)selection_free);
	priv->selection_info.selections = new_list;
	ev_pixbuf_cache_set_selection_list (priv->pixbuf_cache, new_list);
	g_signal_emit (view, signals[SIGNAL_SELECTION_CHANGED], 0, NULL);

	new_list_ptr = new_list;
	old_list_ptr = old_list;

	while (new_list_ptr || old_list_ptr) {
		EvViewSelection *old_sel, *new_sel;
		int cur_page;
		gboolean need_redraw = FALSE;

		new_sel = (new_list_ptr) ? (new_list_ptr->data) : NULL;
		old_sel = (old_list_ptr) ? (old_list_ptr->data) : NULL;

		/* Assume that the lists are in order, and we run through them
		 * comparing them, one page at a time.  We come out with the
		 * first page we see. */
		if (new_sel && old_sel) {
			if (new_sel->page < old_sel->page) {
				new_list_ptr = new_list_ptr->next;
				old_sel = NULL;
			} else if (new_sel->page > old_sel->page) {
				old_list_ptr = old_list_ptr->next;
				new_sel = NULL;
			} else {
				new_list_ptr = new_list_ptr->next;
				old_list_ptr = old_list_ptr->next;
			}
		} else if (new_sel) {
			new_list_ptr = new_list_ptr->next;
		} else if (old_sel) {
			old_list_ptr = old_list_ptr->next;
		}

		g_assert (new_sel || old_sel);

		/* is the page we're looking at on the screen?*/
		cur_page = new_sel ? new_sel->page : old_sel->page;
		if (cur_page < priv->start_page || cur_page > priv->end_page)
			continue;

		/* seed the cache with a new page.  We are going to need the new
		 * region too. */
		if (new_sel) {
			cairo_region_t *tmp_region;

			tmp_region = ev_pixbuf_cache_get_selection_region (priv->pixbuf_cache,
									   cur_page,
									   priv->scale);
			if (tmp_region)
				new_sel->covered_region = cairo_region_reference (tmp_region);
		}

		/* Now we figure out what needs redrawing */
		if (old_sel && new_sel && (old_sel->covered_region || new_sel->covered_region))
			need_redraw = TRUE;
		else if (old_sel && !new_sel &&
			 old_sel->covered_region &&
			 !cairo_region_is_empty (old_sel->covered_region))
			need_redraw = TRUE;
		else if (!old_sel && new_sel &&
			 new_sel->covered_region &&
			 !cairo_region_is_empty (new_sel->covered_region))
			need_redraw = TRUE;

		if (need_redraw)
			gtk_widget_queue_draw (GTK_WIDGET (view));
	}

	ev_view_check_cursor_blink (view);

	/* Free the old list, now that we're done with it. */
	g_list_free_full (old_list, (GDestroyNotify)selection_free);
}

static void
compute_selections (EvView          *view,
		    EvSelectionStyle style,
		    GdkPoint        *start,
		    GdkPoint        *stop)
{
	merge_selection_region (view, compute_new_selection (view, style, start, stop));
}

/* Free's the selection.  It's up to the caller to queue redraws if needed.
 */
static void
selection_free (EvViewSelection *selection)
{
	if (selection->covered_region)
		cairo_region_destroy (selection->covered_region);
	g_slice_free (EvViewSelection, selection);
}

static void
clear_selection (EvView *view)
{
	merge_selection_region (view, NULL);
}

void
ev_view_select_all (EvView *view)
{
	GList *selections = NULL;
	int n_pages, i;
	EvViewPrivate *priv = GET_PRIVATE (view);

	/* Disable selection on rotated pages for the 0.4.0 series */
	if (priv->rotation != 0)
		return;

	n_pages = ev_document_get_n_pages (priv->document);
	for (i = 0; i < n_pages; i++) {
		gdouble width, height;
		EvViewSelection *selection;

		get_doc_page_size (view, i, &width, &height);

		selection = g_slice_new0 (EvViewSelection);
		selection->page = i;
		selection->style = EV_SELECTION_STYLE_GLYPH;
		selection->rect.x1 = selection->rect.y1 = 0;
		selection->rect.x2 = width;
		selection->rect.y2 = height;

		selections = g_list_prepend (selections, selection);
	}

	merge_selection_region (view, g_list_reverse (selections));
}

gboolean
ev_view_has_selection (EvView *view)
{
	EvViewPrivate *priv = GET_PRIVATE (view);
	g_return_val_if_fail(EV_IS_VIEW(view), FALSE);

	return priv->selection_info.selections != NULL;
}

void
_ev_view_clear_selection (EvView *view)
{
	clear_selection (view);
}

void
_ev_view_set_selection (EvView   *view,
			GdkPoint *start_point,
			GdkPoint *end_point)
{
	compute_selections (view, EV_SELECTION_STYLE_GLYPH, start_point, end_point);
}

static char *
get_selected_text (EvView *view)
{
	GString *text;
	GList *l;
	gchar *normalized_text;
	EvViewPrivate *priv = GET_PRIVATE (view);

	text = g_string_new (NULL);

	ev_document_doc_mutex_lock ();

	for (l = priv->selection_info.selections; l != NULL; l = l->next) {
		EvViewSelection *selection = (EvViewSelection *)l->data;
		EvPage *page;
		gchar *tmp;

		page = ev_document_get_page (priv->document, selection->page);
		tmp = ev_selection_get_selected_text (EV_SELECTION (priv->document),
						      page, selection->style,
						      &(selection->rect));
		g_object_unref (page);
		g_string_append (text, tmp);
		g_free (tmp);
	}

	ev_document_doc_mutex_unlock ();

	/* For copying text from the document to the clipboard, we want a normalization
	 * that preserves 'canonical equivalence' i.e. that text after normalization
	 * is not visually different than the original text. Issue #1085 */
	normalized_text = g_utf8_normalize (text->str, text->len, G_NORMALIZE_NFC);
	g_string_free (text, TRUE);
	return normalized_text;
}

static void
ev_view_clipboard_copy (EvView      *view,
			const gchar *text)
{
	GdkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
	gdk_clipboard_set_text (clipboard, text);
}

void
ev_view_copy (EvView *view)
{
	char *text;
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (!EV_IS_SELECTION (priv->document))
		return;

	text = get_selected_text (view);
	ev_view_clipboard_copy (view, text);
	g_free (text);
}

void
ev_view_copy_link_address (EvView       *view,
			   EvLinkAction *action)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_clear_object (&priv->link_selected);

	ev_view_clipboard_copy (view, ev_link_action_get_uri (action));

	priv->link_selected = g_object_ref (action);
}

/*** Cursor operations ***/
static void
ev_view_set_cursor (EvView *view, EvViewCursor new_cursor)
{
	EvViewPrivate *priv = GET_PRIVATE (view);

	if (priv->cursor == new_cursor) {
		return;
	}

	priv->cursor = new_cursor;

	gtk_widget_set_cursor_from_name (GTK_WIDGET (view),
			ev_view_cursor_name (new_cursor));
}

void
ev_view_hide_cursor (EvView *view)
{
       ev_view_set_cursor (view, EV_VIEW_CURSOR_HIDDEN);
}

void
ev_view_show_cursor (EvView *view)
{
       ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
}

gboolean
ev_view_next_page (EvView *view)
{
	gint next_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

	next_page = go_to_next_page (view, priv->current_page);
	if (next_page == -1)
		return FALSE;

	ev_document_model_set_page (priv->model, next_page);

	return TRUE;
}

gboolean
ev_view_previous_page (EvView *view)
{
	gint prev_page;
	EvViewPrivate *priv = GET_PRIVATE (view);

	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

	prev_page = go_to_previous_page (view, priv->current_page);
	if (prev_page == -1)
		return FALSE;

	ev_document_model_set_page (priv->model, prev_page);

	return TRUE;
}

void
ev_view_set_allow_links_change_zoom (EvView *view, gboolean allowed)
{
	g_return_if_fail (EV_IS_VIEW (view));
	EvViewPrivate *priv = GET_PRIVATE (view);

	priv->allow_links_change_zoom = allowed;
}

gboolean
ev_view_get_allow_links_change_zoom (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);
	EvViewPrivate *priv = GET_PRIVATE (view);

	return priv->allow_links_change_zoom;
}
