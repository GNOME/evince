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
#include "ev-document-misc.h"
#include "ev-pixbuf-cache.h"
#include "ev-page-cache.h"
#include "ev-view-marshal.h"
#include "ev-document-annotations.h"
#include "ev-annotation-window.h"
#include "ev-loading-window.h"
#include "ev-view.h"
#include "ev-view-accessible.h"
#include "ev-view-private.h"
#include "ev-view-type-builtins.h"

#define EV_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_VIEW, EvViewClass))
#define EV_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_VIEW))
#define EV_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_VIEW, EvViewClass))

enum {
	SIGNAL_BINDING_ACTIVATED,
	SIGNAL_HANDLE_LINK,
	SIGNAL_EXTERNAL_LINK,
	SIGNAL_POPUP_MENU,
	SIGNAL_SELECTION_CHANGED,
	SIGNAL_SYNC_SOURCE,
	SIGNAL_ANNOT_ADDED,
	SIGNAL_LAYERS_CHANGED,
	N_SIGNALS
};

enum {
	TARGET_DND_URI,
	TARGET_DND_TEXT,
	TARGET_DND_IMAGE
};

enum {
	PROP_0,
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY
};

static guint signals[N_SIGNALS];

typedef enum {
	EV_VIEW_FIND_NEXT,
	EV_VIEW_FIND_PREV
} EvViewFindDirection;

typedef struct {
	GtkWidget  *widget;

	/* View coords */
	gint        x;
	gint        y;

	/* Document */
	guint       page;
	EvRectangle doc_rect;
} EvViewChild;

#define ZOOM_IN_FACTOR  1.2
#define ZOOM_OUT_FACTOR (1.0/ZOOM_IN_FACTOR)

#define SCROLL_TIME 150

/*** Scrolling ***/
static void       view_update_range_and_current_page         (EvView             *view);
static void       add_scroll_binding_keypad                  (GtkBindingSet      *binding_set,
							      guint               keyval,
							      GdkModifierType modifiers,
							      GtkScrollType       scroll,
							      gboolean            horizontal);
static void       ensure_rectangle_is_visible                (EvView             *view,
							      GdkRectangle       *rect);

/*** Geometry computations ***/
static void       compute_border                             (EvView             *view,
							      int                 width,
							      int                 height,
							      GtkBorder          *border);
static void       get_page_y_offset                          (EvView             *view,
							      int                 page,
							      int                *y_offset);
static void       view_rect_to_doc_rect                      (EvView             *view,
							      GdkRectangle       *view_rect,
							      GdkRectangle       *page_area,
							      EvRectangle        *doc_rect);
static void       doc_rect_to_view_rect                      (EvView             *view,
							      int                 page,
							      EvRectangle        *doc_rect,
							      GdkRectangle       *view_rect);
static void       find_page_at_location                      (EvView             *view,
							      gdouble             x,
							      gdouble             y,
							      gint               *page,
							      gint               *x_offset,
							      gint               *y_offset);
static void       doc_point_to_view_point 		     (EvView             *view,
				                              int                 page,
							      EvPoint            *doc_point,
					     	              GdkPoint           *view_point);
/*** Hyperrefs ***/
static EvLink *   ev_view_get_link_at_location 		     (EvView             *view,
				  	         	      gdouble             x,
		            				      gdouble             y);
static char*      tip_from_link                              (EvView             *view,
							      EvLink             *link);
/*** Forms ***/
static EvFormField *ev_view_get_form_field_at_location       (EvView             *view,
							       gdouble            x,
							       gdouble            y);

/*** Annotations ***/
static EvAnnotation *ev_view_get_annotation_at_location      (EvView             *view,
							      gdouble             x,
							      gdouble             y);
static void          show_annotation_windows                 (EvView             *view,
							      gint                page);
static void          hide_annotation_windows                 (EvView             *view,
							      gint                page);
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
							      GtkAllocation      *allocation);
static gboolean   ev_view_scroll_event                       (GtkWidget          *widget,
							      GdkEventScroll     *event);
static gboolean   ev_view_draw                               (GtkWidget          *widget,
                                                              cairo_t            *cr);
static gboolean   ev_view_popup_menu                         (GtkWidget 	 *widget);
static gboolean   ev_view_button_press_event                 (GtkWidget          *widget,
							      GdkEventButton     *event);
static gboolean   ev_view_motion_notify_event                (GtkWidget          *widget,
							      GdkEventMotion     *event);
static gboolean   ev_view_button_release_event               (GtkWidget          *widget,
							      GdkEventButton     *event);
static gboolean   ev_view_enter_notify_event                 (GtkWidget          *widget,
							      GdkEventCrossing   *event);
static gboolean   ev_view_leave_notify_event                 (GtkWidget          *widget,
							      GdkEventCrossing   *event);
static void       ev_view_style_updated                      (GtkWidget          *widget);
static void       ev_view_remove_all                         (EvView             *view);

static AtkObject *ev_view_get_accessible                     (GtkWidget *widget);

/*** Drawing ***/
static void       highlight_find_results                     (EvView             *view,
                                                              cairo_t            *cr,
							      int                 page);
static void       highlight_forward_search_results           (EvView             *view,
                                                              cairo_t            *cr,
							      int                 page);
static void       focus_annotation                           (EvView             *view,
                                                              cairo_t            *cr,
							      int                 page,
							      GdkRectangle       *clip);
static void       draw_one_page                              (EvView             *view,
							      gint                page,
							      cairo_t            *cr,
							      GdkRectangle       *page_area,
							      GtkBorder          *border,
							      GdkRectangle       *expose_area,
							      gboolean		 *page_ready);
static void       show_loading_window                        (EvView             *view);
static void       hide_loading_window                        (EvView             *view);
static void       ev_view_reload_page                        (EvView             *view,
							      gint                page,
							      cairo_region_t     *region);
static void       ev_view_loading_window_move                (EvView             *view);

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
static void       on_adjustment_value_changed                (GtkAdjustment      *adjustment,
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
static double	zoom_for_size_best_fit 			     (gdouble doc_width,
							      gdouble doc_height,
							      int     target_width,
							      int     target_height);
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
/*** Cursors ***/
static void       ev_view_set_cursor                         (EvView             *view,
							      EvViewCursor        new_cursor);
static void       ev_view_handle_cursor_over_xy              (EvView *view,
							      gint x,
							      gint y);

/*** Find ***/
static gint         ev_view_find_get_n_results               (EvView             *view,
							      gint                page);
static EvRectangle *ev_view_find_get_result                  (EvView             *view,
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
static void       clear_selection                            (EvView             *view);
static void       clear_link_selected                        (EvView             *view);
static void       selection_free                             (EvViewSelection    *selection);
static char*      get_selected_text                          (EvView             *ev_view);
static void       ev_view_primary_get_cb                     (GtkClipboard       *clipboard,
							      GtkSelectionData   *selection_data,
							      guint               info,
							      gpointer            data);
static void       ev_view_primary_clear_cb                   (GtkClipboard       *clipboard,
							      gpointer            data);
static void       ev_view_update_primary_selection           (EvView             *ev_view);

G_DEFINE_TYPE_WITH_CODE (EvView, ev_view, GTK_TYPE_CONTAINER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

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
	EvDocument *document = view->document;

	swap = (view->rotation == 90 || view->rotation == 270);

	uniform = ev_document_is_page_size_uniform (document);
	n_pages = ev_document_get_n_pages (document);

	g_free (cache->height_to_page);
	g_free (cache->dual_height_to_page);

	cache->rotation = view->rotation;
	cache->dual_even_left = view->dual_even_left;
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
	if (cache->height_to_page) {
		g_free (cache->height_to_page);
		cache->height_to_page = NULL;
	}

	if (cache->dual_height_to_page) {
		g_free (cache->dual_height_to_page);
		cache->dual_height_to_page = NULL;
	}
	g_free (cache);
}

static EvHeightToPageCache *
ev_view_get_height_to_page_cache (EvView *view)
{
	EvHeightToPageCache *cache;

	if (!view->document)
		return NULL;

	cache = g_object_get_data (G_OBJECT (view->document), EV_HEIGHT_TO_PAGE_CACHE_KEY);
	if (!cache) {
		cache = g_new0 (EvHeightToPageCache, 1);
		ev_view_build_height_to_page_cache (view, cache);
		g_object_set_data_full (G_OBJECT (view->document),
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
	gdouble h, dh;

	if (!view->height_to_page_cache)
		return;

	cache = view->height_to_page_cache;
	if (cache->rotation != view->rotation ||
	    cache->dual_even_left != view->dual_even_left) {
		ev_view_build_height_to_page_cache (view, cache);
	}
	h = cache->height_to_page[page];
	dh = cache->dual_height_to_page[page];

	if (height)
		*height = (gint)(h * view->scale + 0.5);

	if (dual_height)
		*dual_height = (gint)(dh * view->scale + 0.5);
}

static gint
ev_view_get_scrollbar_size (EvView        *view,
			    GtkOrientation orientation)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkWidget *sb;
	GtkWidget *swindow = gtk_widget_get_parent (GTK_WIDGET (view));
	GtkAllocation allocation;
	GtkRequisition req;
	gint spacing;

	if (!GTK_IS_SCROLLED_WINDOW (swindow))
		return 0;

	gtk_widget_get_allocation (widget, &allocation);

	if (orientation == GTK_ORIENTATION_VERTICAL) {
		if (allocation.height >= view->requisition.height)
			sb = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (swindow));
		else
			return 0;
	} else {
		if (allocation.width >= view->requisition.width)
			sb = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (swindow));
		else
			return 0;
	}

	gtk_widget_style_get (swindow, "scrollbar_spacing", &spacing, NULL);
	gtk_widget_get_preferred_size (sb, &req, NULL);

	return (orientation == GTK_ORIENTATION_VERTICAL ? req.width : req.height) + spacing;
}

static void
scroll_to_point (EvView        *view,
		 gdouble        x,
		 gdouble        y,
		 GtkOrientation orientation)
{
	gdouble page_size;
	gdouble upper, lower;

	if (orientation == GTK_ORIENTATION_VERTICAL) {
		page_size = gtk_adjustment_get_page_size (view->vadjustment);
		upper = gtk_adjustment_get_upper (view->vadjustment);
		lower = gtk_adjustment_get_lower (view->vadjustment);

		if (view->continuous) {
    			gtk_adjustment_clamp_page (view->vadjustment,
						   y - view->spacing / 2,
						   y + page_size);
		} else {
			gtk_adjustment_set_value (view->vadjustment,
						  CLAMP (y, lower, upper - page_size));
		}
	} else {
		page_size = gtk_adjustment_get_page_size (view->hadjustment);
		upper = gtk_adjustment_get_upper (view->hadjustment);
		lower = gtk_adjustment_get_lower (view->hadjustment);

		if (view->dual_page) {
			gtk_adjustment_clamp_page (view->hadjustment, x,
						   x + page_size);
		} else {
			gtk_adjustment_set_value (view->hadjustment,
						  CLAMP (x, lower, upper - page_size));
		}
	}
}

static void
ev_view_scroll_to_page_position (EvView *view, GtkOrientation orientation)
{
	gdouble x, y;

	if (!view->document)
		return;

	if ((orientation == GTK_ORIENTATION_VERTICAL && view->pending_point.y == 0) ||
	    (orientation == GTK_ORIENTATION_HORIZONTAL && view->pending_point.x == 0)) {
		GdkRectangle page_area;
		GtkBorder    border;

		ev_view_get_page_extents (view, view->current_page, &page_area, &border);
		x = page_area.x;
		y = page_area.y;
	} else {
		GdkPoint view_point;

		doc_point_to_view_point (view, view->current_page,
					 &view->pending_point, &view_point);
		x = view_point.x;
		y = view_point.y;
	}

	scroll_to_point (view, x, y, orientation);
}

static void
ev_view_set_adjustment_values (EvView         *view,
			       GtkOrientation  orientation)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkAdjustment *adjustment;
	GtkAllocation allocation;
	int req_size;
	int alloc_size;
	gdouble page_size;
	gdouble value;
	gdouble upper;
	double factor;
	gint new_value;

	gtk_widget_get_allocation (widget, &allocation);

	if (orientation == GTK_ORIENTATION_HORIZONTAL)  {
		req_size = view->requisition.width;
		alloc_size = allocation.width;
		adjustment = view->hadjustment;
	} else {
		req_size = view->requisition.height;
		alloc_size = allocation.height;
		adjustment = view->vadjustment;
	}

	if (!adjustment)
		return;

	factor = 1.0;
	value = gtk_adjustment_get_value (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	if (upper != .0) {
		switch (view->pending_scroll) {
    	        case SCROLL_TO_KEEP_POSITION:
    	        case SCROLL_TO_FIND_LOCATION:
			factor = value / upper;
			break;
    	        case SCROLL_TO_PAGE_POSITION:
			break;
    	        case SCROLL_TO_CENTER:
			factor = (value + page_size * 0.5) / upper;
			break;
		}
	}

	upper = MAX (alloc_size, req_size);
	page_size = alloc_size;

	gtk_adjustment_set_page_size (adjustment, page_size);
	gtk_adjustment_set_step_increment (adjustment, alloc_size * 0.1);
	gtk_adjustment_set_page_increment (adjustment, alloc_size * 0.9);
	gtk_adjustment_set_lower (adjustment, 0);
	gtk_adjustment_set_upper (adjustment, upper);

	/*
	 * We add 0.5 to the values before to average out our rounding errors.
	 */
	switch (view->pending_scroll) {
    	        case SCROLL_TO_KEEP_POSITION:
    	        case SCROLL_TO_FIND_LOCATION:
			new_value = CLAMP (upper * factor + 0.5, 0, upper - page_size);
			gtk_adjustment_set_value (adjustment, (int)new_value);
			break;
    	        case SCROLL_TO_PAGE_POSITION:
			ev_view_scroll_to_page_position (view, orientation);
			break;
    	        case SCROLL_TO_CENTER:
			new_value = CLAMP (upper * factor - page_size * 0.5 + 0.5,
					   0, upper - page_size);
			gtk_adjustment_set_value (adjustment, (int)new_value);
			break;
	}

	gtk_adjustment_changed (adjustment);
}

static void
view_update_range_and_current_page (EvView *view)
{
	gint start = view->start_page;
	gint end = view->end_page;

	if (ev_document_get_n_pages (view->document) <= 0 ||
	    !ev_document_check_dimensions (view->document))
		return;

	if (view->continuous) {
		GdkRectangle current_area, unused, page_area;
		GtkBorder border;
		gboolean found = FALSE;
		gint area_max = -1, area;
		gint best_current_page = -1;
		int i, j = 0;

		if (!(view->vadjustment && view->hadjustment))
			return;

		current_area.x = gtk_adjustment_get_value (view->hadjustment);
		current_area.width = gtk_adjustment_get_page_size (view->hadjustment);
		current_area.y = gtk_adjustment_get_value (view->vadjustment);
		current_area.height = gtk_adjustment_get_page_size (view->vadjustment);

		for (i = 0; i < ev_document_get_n_pages (view->document); i++) {

			ev_view_get_page_extents (view, i, &page_area, &border);

			if (gdk_rectangle_intersect (&current_area, &page_area, &unused)) {
				area = unused.width * unused.height;

				if (!found) {
					area_max = area;
					view->start_page = i;
					found = TRUE;
					best_current_page = i;
				}
				if (area > area_max) {
					best_current_page = (area == area_max) ? MIN (i, best_current_page) : i;
					area_max = area;
				}

				view->end_page = i;
				j = 0;
			} else if (found && view->current_page <= view->end_page) {
				if (view->dual_page && j < 1) {
					/* In dual mode  we stop searching
					 * after two consecutive non-visible pages.
					 */
					j++;
					continue;
				}
				break;
			}
		}

		if (view->pending_scroll == SCROLL_TO_KEEP_POSITION) {
			best_current_page = MAX (best_current_page, view->start_page);

			if (view->current_page != best_current_page) {
				view->current_page = best_current_page;
				hide_loading_window (view);
				ev_document_model_set_page (view->model, best_current_page);
			}
		}
	} else if (view->dual_page) {
		if (view->current_page % 2 == view->dual_even_left) {
			view->start_page = view->current_page;
			if (view->current_page + 1 < ev_document_get_n_pages (view->document))
				view->end_page = view->start_page + 1;
			else
				view->end_page = view->start_page;
		} else {
			if (view->current_page < 1)
				view->start_page = view->current_page;
			else
				view->start_page = view->current_page - 1;
			view->end_page = view->current_page;
		}
	} else {
		view->start_page = view->current_page;
		view->end_page = view->current_page;
	}

	if (view->start_page == -1 || view->end_page == -1)
		return;

	if (start != view->start_page || end != view->end_page) {
		gint i;

		for (i = start; i < view->start_page && start != -1; i++) {
			hide_annotation_windows (view, i);
		}

		for (i = end; i > view->end_page && end != -1; i--) {
			hide_annotation_windows (view, i);
		}
	}

	ev_page_cache_set_page_range (view->page_cache,
				      view->start_page,
				      view->end_page);
	ev_pixbuf_cache_set_page_range (view->pixbuf_cache,
					view->start_page,
					view->end_page,
					view->selection_info.selections);

	if (ev_pixbuf_cache_get_surface (view->pixbuf_cache, view->current_page))
	    gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
ev_view_set_scroll_adjustment (EvView         *view,
			       GtkOrientation  orientation,
			       GtkAdjustment  *adjustment)
{
	GtkAdjustment **to_set;
	const gchar    *prop_name;

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		to_set = &view->hadjustment;
		prop_name = "hadjustment";
	} else {
		to_set = &view->vadjustment;
		prop_name = "vadjustment";
	}

	if (adjustment && adjustment == *to_set)
		return;

	if (*to_set) {
		g_signal_handlers_disconnect_by_func (*to_set,
						      (gpointer) on_adjustment_value_changed,
						      view);
		g_object_unref (*to_set);
	}

	if (!adjustment)
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	g_signal_connect (adjustment, "value_changed",
			  G_CALLBACK (on_adjustment_value_changed),
			  view);
	*to_set = g_object_ref_sink (adjustment);
	ev_view_set_adjustment_values (view, orientation);

	g_object_notify (G_OBJECT (view), prop_name);
}

static void
add_scroll_binding_keypad (GtkBindingSet  *binding_set,
    			   guint           keyval,
    			   GdkModifierType modifiers,
    			   GtkScrollType   scroll,
			   gboolean        horizontal)
{
  guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_binding_entry_add_signal (binding_set, keyval, modifiers,
                                "binding_activated", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
  gtk_binding_entry_add_signal (binding_set, keypad_keyval, modifiers,
                                "binding_activated", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
}

static gdouble
compute_scroll_increment (EvView        *view,
			  GtkScrollType  scroll)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkAdjustment *adjustment = view->vadjustment;
	cairo_region_t *text_region, *region;
	GtkAllocation allocation;
	gint page;
	GdkRectangle rect;
	EvRectangle doc_rect;
	GdkRectangle page_area;
	GtkBorder border;
	gdouble fraction = 1.0;

	if (scroll != GTK_SCROLL_PAGE_BACKWARD && scroll != GTK_SCROLL_PAGE_FORWARD)
		return gtk_adjustment_get_page_size (adjustment);

	page = scroll == GTK_SCROLL_PAGE_BACKWARD ? view->start_page : view->end_page;

	text_region = ev_page_cache_get_text_mapping (view->page_cache, page);
	if (!text_region || cairo_region_is_empty (text_region))
		return gtk_adjustment_get_page_size (adjustment);

	gtk_widget_get_allocation (widget, &allocation);
	ev_view_get_page_extents (view, page, &page_area, &border);
	rect.x = page_area.x + view->scroll_x;
	rect.y = view->scroll_y + (scroll == GTK_SCROLL_PAGE_BACKWARD ? 5 : allocation.height - 5);
	rect.width = page_area.width;
	rect.height = 1;
	view_rect_to_doc_rect (view, &rect, &page_area, &doc_rect);

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
		ev_page = ev_document_get_page (view->document, page);
		rc = ev_render_context_new (ev_page, view->rotation, view->scale);
		g_object_unref (ev_page);
		/* Get the selection region to know the height of the line */
		doc_rect.x1 = doc_rect.x2 = rect.x + 0.5;
		doc_rect.y1 = doc_rect.y2 = rect.y + 0.5;

		ev_document_doc_mutex_lock ();
		sel_region = ev_selection_get_selection_region (EV_SELECTION (view->document),
								rc, EV_SELECTION_STYLE_LINE,
								&doc_rect);
		ev_document_doc_mutex_unlock ();

		g_object_unref (rc);

		if (cairo_region_num_rectangles (sel_region) > 0) {
			cairo_region_get_rectangle (sel_region, 0, &rect);
			fraction = 1 - (rect.height / gtk_adjustment_get_page_size (adjustment));
		}
		cairo_region_destroy (sel_region);
	}
	cairo_region_destroy (region);

	return gtk_adjustment_get_page_size (adjustment) * fraction;

}

void
ev_view_scroll (EvView        *view,
	        GtkScrollType  scroll,
		gboolean       horizontal)
{
	GtkAdjustment *adjustment;
	double value, increment;
	gdouble upper, lower;
	gdouble page_size;
	gdouble step_increment;
	gboolean first_page = FALSE;
	gboolean last_page = FALSE;

	view->jump_to_find_result = FALSE;

	if (view->sizing_mode == EV_SIZING_BEST_FIT) {
		switch (scroll) {
			case GTK_SCROLL_PAGE_BACKWARD:
			case GTK_SCROLL_STEP_BACKWARD:
				ev_view_previous_page (view);
				break;
			case GTK_SCROLL_PAGE_FORWARD:
			case GTK_SCROLL_STEP_FORWARD:
				ev_view_next_page (view);
				break;
			default:
				break;
		}
		return;
	}

	/* Assign values for increment and vertical adjustment */
	adjustment = horizontal ? view->hadjustment : view->vadjustment;
	value = gtk_adjustment_get_value (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);
	step_increment = gtk_adjustment_get_step_increment (adjustment);

	/* Assign boolean for first and last page */
	if (view->current_page == 0)
		first_page = TRUE;
	if (view->current_page == ev_document_get_n_pages (view->document) - 1)
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
        	default:
			break;
	}

	value = CLAMP (value, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, value);
}

#define MARGIN 5

static void
ensure_rectangle_is_visible (EvView *view, GdkRectangle *rect)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkAdjustment *adjustment;
	GtkAllocation allocation;
	gdouble adj_value;
	int value;

	view->pending_scroll = SCROLL_TO_FIND_LOCATION;

	gtk_widget_get_allocation (widget, &allocation);

	adjustment = view->vadjustment;
	adj_value = gtk_adjustment_get_value (adjustment);

	if (rect->y < adj_value) {
		value = MAX (gtk_adjustment_get_lower (adjustment), rect->y - MARGIN);
		gtk_adjustment_set_value (view->vadjustment, value);
	} else if (rect->y + rect->height > adj_value + allocation.height) {
		value = MIN (gtk_adjustment_get_upper (adjustment), rect->y + rect->height -
			     allocation.height + MARGIN);
		gtk_adjustment_set_value (view->vadjustment, value);
	}

	adjustment = view->hadjustment;
	adj_value = gtk_adjustment_get_value (adjustment);

	if (rect->x < adj_value) {
		value = MAX (gtk_adjustment_get_lower (adjustment), rect->x - MARGIN);
		gtk_adjustment_set_value (view->hadjustment, value);
	} else if (rect->x + rect->height > adj_value + allocation.width) {
		value = MIN (gtk_adjustment_get_upper (adjustment), rect->x + rect->width -
			     allocation.width + MARGIN);
		gtk_adjustment_set_value (view->hadjustment, value);
	}

	gtk_widget_queue_resize (GTK_WIDGET (view));
}

/*** Geometry computations ***/

static void
compute_border (EvView *view, int width, int height, GtkBorder *border)
{
	ev_document_misc_get_page_border_size (width, height, border);
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
	_get_page_size_for_scale_and_rotation (view->document,
					       page,
					       view->scale,
					       view->rotation,
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

	ev_document_get_max_page_size (view->document, &w, &h);

	width = (gint)(w * view->scale + 0.5);
	height = (gint)(h * view->scale + 0.5);

	if (max_width)
		*max_width = (view->rotation == 0 || view->rotation == 180) ? width : height;
	if (max_height)
		*max_height = (view->rotation == 0 || view->rotation == 180) ? height : width;
}

static void
get_page_y_offset (EvView *view, int page, int *y_offset)
{
	int max_width, offset;
	GtkBorder border;

	g_return_if_fail (y_offset != NULL);

	ev_view_get_max_page_size (view, &max_width, NULL);
	compute_border (view, max_width, max_width, &border);

	if (view->dual_page) {
		ev_view_get_height_to_page (view, page, NULL, &offset);
		offset += ((page + view->dual_even_left) / 2 + 1) * view->spacing +
			((page + view->dual_even_left) / 2 ) * (border.top + border.bottom);
	} else {
		ev_view_get_height_to_page (view, page, &offset, NULL);
		offset += (page + 1) * view->spacing + page * (border.top + border.bottom);
	}

	*y_offset = offset;
	return;
}

gboolean
ev_view_get_page_extents (EvView       *view,
			  gint          page,
			  GdkRectangle *page_area,
			  GtkBorder    *border)
{
	GtkWidget *widget;
	int width, height;
	GtkAllocation allocation;

	widget = GTK_WIDGET (view);
	gtk_widget_get_allocation (widget, &allocation);

	/* Get the size of the page */
	ev_view_get_page_size (view, page, &width, &height);
	compute_border (view, width, height, border);
	page_area->width = width + border->left + border->right;
	page_area->height = height + border->top + border->bottom;

	if (view->continuous) {
		gint max_width;
		gint x, y;

		ev_view_get_max_page_size (view, &max_width, NULL);
		max_width = max_width + border->left + border->right;
		/* Get the location of the bounding box */
		if (view->dual_page) {
			x = view->spacing + ((page % 2 == view->dual_even_left) ? 0 : 1) * (max_width + view->spacing);
			x = x + MAX (0, allocation.width - (max_width * 2 + view->spacing * 3)) / 2;
			if (page % 2 == view->dual_even_left)
				x = x + (max_width - width - border->left - border->right);
		} else {
			x = view->spacing;
			x = x + MAX (0, allocation.width - (width + view->spacing * 2)) / 2;
		}

		get_page_y_offset (view, page, &y);

		page_area->x = x;
		page_area->y = y;
	} else {
		gint x, y;
		if (view->dual_page) {
			gint width_2, height_2;
			gint max_width = width;
			gint max_height = height;
			GtkBorder overall_border;
			gint other_page;

			other_page = (page % 2 == view->dual_even_left) ? page + 1: page - 1;

			/* First, we get the bounding box of the two pages */
			if (other_page < ev_document_get_n_pages (view->document)
			    && (0 <= other_page)) {
				ev_view_get_page_size (view, other_page,
						       &width_2, &height_2);
				if (width_2 > width)
					max_width = width_2;
				if (height_2 > height)
					max_height = height_2;
			}
			compute_border (view, max_width, max_height, &overall_border);

			/* Find the offsets */
			x = view->spacing;
			y = view->spacing;

			/* Adjust for being the left or right page */
			if (page % 2 == view->dual_even_left)
				x = x + max_width - width;
			else
				x = x + (max_width + overall_border.left + overall_border.right) + view->spacing;

			y = y + (max_height - height)/2;

			/* Adjust for extra allocation */
			x = x + MAX (0, allocation.width -
				     ((max_width + overall_border.left + overall_border.right) * 2 + view->spacing * 3))/2;
			y = y + MAX (0, allocation.height - (height + view->spacing * 2))/2;
		} else {
			x = view->spacing;
			y = view->spacing;

			/* Adjust for extra allocation */
			x = x + MAX (0, allocation.width - (width + border->left + border->right + view->spacing * 2))/2;
			y = y + MAX (0, allocation.height - (height + border->top + border->bottom +  view->spacing * 2))/2;
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

	ev_document_get_page_size (view->document, page, &w, &h);
	if (view->rotation == 0 || view->rotation == 180) {
		if (width) *width = w;
		if (height) *height = h;
	} else {
		if (width) *width = h;
		if (height) *height = w;
	}
}

static void
view_point_to_doc_point (EvView *view,
			 GdkPoint *view_point,
			 GdkRectangle *page_area,
			 double  *doc_point_x,
			 double  *doc_point_y)
{
	*doc_point_x = (double) (view_point->x - page_area->x) / view->scale;
	*doc_point_y = (double) (view_point->y - page_area->y) / view->scale;
}

static void
view_rect_to_doc_rect (EvView *view,
		       GdkRectangle *view_rect,
		       GdkRectangle *page_area,
		       EvRectangle  *doc_rect)
{
	doc_rect->x1 = (double) (view_rect->x - page_area->x) / view->scale;
	doc_rect->y1 = (double) (view_rect->y - page_area->y) / view->scale;
	doc_rect->x2 = doc_rect->x1 + (double) view_rect->width / view->scale;
	doc_rect->y2 = doc_rect->y1 + (double) view_rect->height / view->scale;
}

static void
doc_point_to_view_point (EvView       *view,
                         int           page,
		         EvPoint      *doc_point,
		         GdkPoint     *view_point)
{
	GdkRectangle page_area;
	GtkBorder border;
	double x, y, view_x, view_y;
	gdouble width, height;

	get_doc_page_size (view, page, &width, &height);

	if (view->rotation == 0) {
		x = doc_point->x;
		y = doc_point->y;
	} else if (view->rotation == 90) {
		x = width - doc_point->y;
		y = doc_point->x;
	} else if (view->rotation == 180) {
		x = width - doc_point->x;
		y = height - doc_point->y;
	} else if (view->rotation == 270) {
		x = doc_point->y;
		y = height - doc_point->x;
	} else {
		g_assert_not_reached ();
	}

	ev_view_get_page_extents (view, page, &page_area, &border);

	view_x = CLAMP (x * view->scale, 0, page_area.width);
	view_y = CLAMP (y * view->scale, 0, page_area.height);
	view_point->x = view_x + page_area.x;
	view_point->y = view_y + page_area.y;
}

static void
doc_rect_to_view_rect (EvView       *view,
                       int           page,
		       EvRectangle  *doc_rect,
		       GdkRectangle *view_rect)
{
	GdkRectangle page_area;
	GtkBorder border;
	double x, y, w, h;
	gdouble width, height;

	get_doc_page_size (view, page, &width, &height);

	if (view->rotation == 0) {
		x = doc_rect->x1;
		y = doc_rect->y1;
		w = doc_rect->x2 - doc_rect->x1;
		h = doc_rect->y2 - doc_rect->y1;
	} else if (view->rotation == 90) {
		x = width - doc_rect->y2;
		y = doc_rect->x1;
		w = doc_rect->y2 - doc_rect->y1;
		h = doc_rect->x2 - doc_rect->x1;
	} else if (view->rotation == 180) {
		x = width - doc_rect->x2;
		y = height - doc_rect->y2;
		w = doc_rect->x2 - doc_rect->x1;
		h = doc_rect->y2 - doc_rect->y1;
	} else if (view->rotation == 270) {
		x = doc_rect->y1;
		y = height - doc_rect->x2;
		w = doc_rect->y2 - doc_rect->y1;
		h = doc_rect->x2 - doc_rect->x1;
	} else {
		g_assert_not_reached ();
	}

	ev_view_get_page_extents (view, page, &page_area, &border);

	view_rect->x = x * view->scale + page_area.x;
	view_rect->y = y * view->scale + page_area.y;
	view_rect->width = w * view->scale;
	view_rect->height = h * view->scale;
}

static void
find_page_at_location (EvView  *view,
		       gdouble  x,
		       gdouble  y,
		       gint    *page,
		       gint    *x_offset,
		       gint    *y_offset)
{
	int i;

	if (view->document == NULL)
		return;

	g_assert (page);
	g_assert (x_offset);
	g_assert (y_offset);

	for (i = view->start_page; i >= 0 && i <= view->end_page; i++) {
		GdkRectangle page_area;
		GtkBorder border;

		if (! ev_view_get_page_extents (view, i, &page_area, &border))
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
	cairo_region_t *region;
	gint page = -1;
	gint x_offset = 0, y_offset = 0;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	if (page == -1)
		return FALSE;
	
	region = ev_page_cache_get_text_mapping (view->page_cache, page);

	if (region)
		return cairo_region_contains_point (region, x_offset / view->scale, y_offset / view->scale);
	else
		return FALSE;
}

static gboolean
location_in_selected_text (EvView  *view,
			   gdouble  x,
			   gdouble  y)
{
	gint page = -1;
	gint x_offset = 0, y_offset = 0;
	EvViewSelection *selection;
	GList *l = NULL;

	for (l = view->selection_info.selections; l != NULL; l = l->next) {
		selection = (EvViewSelection *)l->data;
		
		find_page_at_location (view, x, y, &page, &x_offset, &y_offset);
		
		if (page != selection->page)
			continue;
		
		if (selection->covered_region &&
		    cairo_region_contains_point (selection->covered_region, x_offset, y_offset))
			return TRUE;
	}

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
        gdouble width, height;
	double x, y;

	get_doc_page_size (view, page, &width, &height);

	x_offset = x_offset / view->scale;
	y_offset = y_offset / view->scale;

        if (view->rotation == 0) {
                x = x_offset;
                y = y_offset;
        } else if (view->rotation == 90) {
                x = y_offset;
                y = width - x_offset;
        } else if (view->rotation == 180) {
                x = width - x_offset;
                y = height - y_offset;
        } else if (view->rotation == 270) {
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
	gint x_offset = 0, y_offset = 0;

	x += view->scroll_x;
	y += view->scroll_y;
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
	EvMapping *mapping;

	mapping = ev_mapping_list_find (mapping_list, data);
	doc_rect_to_view_rect (view, page, &mapping->area, area);
	area->x -= view->scroll_x;
	area->y -= view->scroll_y;
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

	child->widget = child_widget;
	child->x = x;
	child->y = y;
	child->page = page;
	child->doc_rect = *doc_rect;

	gtk_widget_set_parent (child_widget, GTK_WIDGET (view));
	view->children = g_list_append (view->children, child);
}

static void
ev_view_put_to_doc_rect (EvView      *view,
			 GtkWidget   *child_widget,
			 guint        page,
			 EvRectangle *doc_rect)
{
	GdkRectangle area;

	doc_rect_to_view_rect (view, page, doc_rect, &area);
	area.x -= view->scroll_x;
	area.y -= view->scroll_y;
	ev_view_put (view, child_widget, area.x, area.y, page, doc_rect);
}

/*** Hyperref ***/
static EvLink *
ev_view_get_link_at_location (EvView  *view,
  	         	      gdouble  x,
		              gdouble  y)
{
	gint page = -1;
	gint x_new = 0, y_new = 0;
	EvMappingList *link_mapping;

	if (!EV_IS_DOCUMENT_LINKS (view->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, &page, &x_new, &y_new))
		return NULL;

	link_mapping = ev_page_cache_get_link_mapping (view->page_cache, page);

	if (link_mapping)
		return ev_mapping_list_get_data (link_mapping, x_new, y_new);
	else
		return NULL;
}

static void
goto_fitr_dest (EvView *view, EvLinkDest *dest)
{
	EvPoint doc_point;
	gdouble zoom, left, top;
	gboolean change_left, change_top;
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	left = ev_link_dest_get_left (dest, &change_left);
	top = ev_link_dest_get_top (dest, &change_top);

	zoom = zoom_for_size_best_fit (ev_link_dest_get_right (dest) - left,
				       ev_link_dest_get_bottom (dest) - top,
				       allocation.width,
				       allocation.height);

	ev_document_model_set_sizing_mode (view->model, EV_SIZING_FREE);
	ev_document_model_set_scale (view->model, zoom);

	doc_point.x = change_left ? left : 0;
	doc_point.y = change_top ? top : 0;
	view->pending_point = doc_point;

	ev_view_change_page (view, ev_link_dest_get_page (dest));
}

static void
goto_fitv_dest (EvView *view, EvLinkDest *dest)
{
	EvPoint doc_point;
	gdouble doc_width, doc_height;
	gint page;
	double zoom, left;
	gboolean change_left;
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	page = ev_link_dest_get_page (dest);
	ev_document_get_page_size (view->document, page, &doc_width, &doc_height);

	left = ev_link_dest_get_left (dest, &change_left);
	doc_point.x = change_left ? left : 0;
	doc_point.y = 0;

	zoom = zoom_for_size_fit_height (doc_width - doc_point.x , doc_height,
					 allocation.width,
				         allocation.height);

	ev_document_model_set_sizing_mode (view->model, EV_SIZING_FREE);
	ev_document_model_set_scale (view->model, zoom);

	view->pending_point = doc_point;

	ev_view_change_page (view, page);
}

static void
goto_fith_dest (EvView *view, EvLinkDest *dest)
{
	EvPoint doc_point;
	gdouble doc_width, doc_height;
	gint page;
	gdouble zoom, top;
	gboolean change_top;
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	page = ev_link_dest_get_page (dest);
	ev_document_get_page_size (view->document, page, &doc_width, &doc_height);

	top = ev_link_dest_get_top (dest, &change_top);

	doc_point.x = 0;
	doc_point.y = change_top ? top : 0;

	zoom = zoom_for_size_fit_width (doc_width, top,
					allocation.width,
				        allocation.height);

	ev_document_model_set_sizing_mode (view->model, EV_SIZING_FIT_WIDTH);
	ev_document_model_set_scale (view->model, zoom);

	view->pending_point = doc_point;

	ev_view_change_page (view, page);
}

static void
goto_fit_dest (EvView *view, EvLinkDest *dest)
{
	double zoom;
	gdouble doc_width, doc_height;
	int page;
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	page = ev_link_dest_get_page (dest);
	ev_document_get_page_size (view->document, page, &doc_width, &doc_height);

	zoom = zoom_for_size_best_fit (doc_width, doc_height,
				       allocation.width,
				       allocation.height);

	ev_document_model_set_sizing_mode (view->model, EV_SIZING_BEST_FIT);
	ev_document_model_set_scale (view->model, zoom);

	ev_view_change_page (view, page);
}

static void
goto_xyz_dest (EvView *view, EvLinkDest *dest)
{
	EvPoint doc_point;
	gint page;
	gdouble zoom, left, top;
	gboolean change_zoom, change_left, change_top; 

	zoom = ev_link_dest_get_zoom (dest, &change_zoom);
	page = ev_link_dest_get_page (dest);

	if (change_zoom && zoom > 1) {
		ev_document_model_set_sizing_mode (view->model, EV_SIZING_FREE);
		ev_document_model_set_scale (view->model, zoom);
	}

	left = ev_link_dest_get_left (dest, &change_left);
	top = ev_link_dest_get_top (dest, &change_top);

	doc_point.x = change_left ? left : 0;
	doc_point.y = change_top ? top : 0;
	view->pending_point = doc_point;

	ev_view_change_page (view, page);
}

static void
goto_dest (EvView *view, EvLinkDest *dest)
{
	EvLinkDestType type;
	int page, n_pages, current_page;

	page = ev_link_dest_get_page (dest);
	n_pages = ev_document_get_n_pages (view->document);

	if (page < 0 || page >= n_pages)
		return;

	current_page = view->current_page;
	
	type = ev_link_dest_get_dest_type (dest);

	switch (type) {
	        case EV_LINK_DEST_TYPE_PAGE:
			ev_document_model_set_page (view->model, page);
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
			ev_document_model_set_page_by_label (view->model, ev_link_dest_get_page_label (dest));
			break;
	        default:
			g_assert_not_reached ();
	}

	if (current_page != view->current_page)
		ev_document_model_set_page (view->model, view->current_page);
}

static void
ev_view_goto_dest (EvView *view, EvLinkDest *dest)
{
	EvLinkDestType type;

	type = ev_link_dest_get_dest_type (dest);

	if (type == EV_LINK_DEST_TYPE_NAMED) {
		EvLinkDest  *dest2;	
		const gchar *named_dest;

		named_dest = ev_link_dest_get_named_dest (dest);
		dest2 = ev_document_links_find_link_dest (EV_DOCUMENT_LINKS (view->document),
							  named_dest);
		if (dest2) {
			goto_dest (view, dest2);
			g_object_unref (dest2);
		}

		return;
	}

	goto_dest (view, dest);
}
	
void
ev_view_handle_link (EvView *view, EvLink *link)
{
	EvLinkAction    *action = NULL;
	EvLinkActionType type;

	action = ev_link_get_action (link);
	if (!action)
		return;

	type = ev_link_action_get_action_type (action);

	switch (type) {
	        case EV_LINK_ACTION_TYPE_GOTO_DEST: {
			EvLinkDest *dest;
			
			g_signal_emit (view, signals[SIGNAL_HANDLE_LINK], 0, link);
		
			dest = ev_link_action_get_dest (action);
			ev_view_goto_dest (view, dest);
		}
			break;
	        case EV_LINK_ACTION_TYPE_LAYERS_STATE: {
			GList            *show, *hide, *toggle;
			GList            *l;
			EvDocumentLayers *document_layers;

			document_layers = EV_DOCUMENT_LAYERS (view->document);

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
			ev_view_reload_page (view, view->current_page, NULL);
		}
			break;
	        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
	        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
	        case EV_LINK_ACTION_TYPE_LAUNCH:
	        case EV_LINK_ACTION_TYPE_NAMED:
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
			page_label = ev_document_links_get_dest_page_label (EV_DOCUMENT_LINKS (view->document),
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
	        default:
			if (title)
				msg = g_strdup (title);
			break;
	}
	
	return msg;
}

static void
ev_view_handle_cursor_over_xy (EvView *view, gint x, gint y)
{
	EvLink       *link;
	EvFormField  *field;
	EvAnnotation *annot = NULL;

	if (view->cursor == EV_VIEW_CURSOR_HIDDEN)
		return;

	if (view->adding_annot) {
		if (view->cursor != EV_VIEW_CURSOR_ADD)
			ev_view_set_cursor (view, EV_VIEW_CURSOR_ADD);
		return;
	}

	if (view->drag_info.in_drag) {
		if (view->cursor != EV_VIEW_CURSOR_DRAG)
			ev_view_set_cursor (view, EV_VIEW_CURSOR_DRAG);
		return;
	}

	if (view->scroll_info.autoscrolling) {
		if (view->cursor != EV_VIEW_CURSOR_AUTOSCROLL)
			ev_view_set_cursor (view, EV_VIEW_CURSOR_AUTOSCROLL);
		return;
	}

	link = ev_view_get_link_at_location (view, x, y);
        if (link) {
		ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);
	} else if ((field = ev_view_get_form_field_at_location (view, x, y))) {
		if (field->is_read_only) {
			if (view->cursor == EV_VIEW_CURSOR_LINK ||
			    view->cursor == EV_VIEW_CURSOR_IBEAM ||
			    view->cursor == EV_VIEW_CURSOR_DRAG)
				ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
		} else if (EV_IS_FORM_FIELD_TEXT (field)) {
			ev_view_set_cursor (view, EV_VIEW_CURSOR_IBEAM);
		} else {
			ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);
		}
	} else if ((annot = ev_view_get_annotation_at_location (view, x, y))) {
		ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);
	} else if (location_in_text (view, x + view->scroll_x, y + view->scroll_y)) {
		ev_view_set_cursor (view, EV_VIEW_CURSOR_IBEAM);
	} else {
		if (view->cursor == EV_VIEW_CURSOR_LINK ||
		    view->cursor == EV_VIEW_CURSOR_IBEAM ||
		    view->cursor == EV_VIEW_CURSOR_DRAG ||
		    view->cursor == EV_VIEW_CURSOR_AUTOSCROLL ||
		    view->cursor == EV_VIEW_CURSOR_ADD)
			ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
	}

	if (link || annot)
		g_object_set (view, "has-tooltip", TRUE, NULL);
}

/*** Images ***/
static EvImage *
ev_view_get_image_at_location (EvView  *view,
			       gdouble  x,
			       gdouble  y)
{
	gint page = -1;
	gint x_new = 0, y_new = 0;
	EvMappingList *image_mapping;

	if (!EV_IS_DOCUMENT_IMAGES (view->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, &page, &x_new, &y_new))
		return NULL;

	image_mapping = ev_page_cache_get_image_mapping (view->page_cache, page);

	if (image_mapping)
		return ev_mapping_list_get_data (image_mapping, x_new, y_new);
	else
		return NULL;
}

/*** Forms ***/
static EvFormField *
ev_view_get_form_field_at_location (EvView  *view,
				    gdouble  x,
				    gdouble  y)
{
	gint page = -1;
	gint x_new = 0, y_new = 0;
	EvMappingList *forms_mapping;
	
	if (!EV_IS_DOCUMENT_FORMS (view->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, &page, &x_new, &y_new))
		return NULL;

	forms_mapping = ev_page_cache_get_form_field_mapping (view->page_cache, page);

	if (forms_mapping)
		return ev_mapping_list_get_data (forms_mapping, x_new, y_new);
	else
		return NULL;
}

static cairo_region_t *
ev_view_form_field_get_region (EvView      *view,
			       EvFormField *field)
{
	GdkRectangle   view_area;
	EvMappingList *forms_mapping;

	forms_mapping = ev_page_cache_get_form_field_mapping (view->page_cache,
							      field->page->index);
	ev_view_get_area_from_mapping (view, field->page->index,
				       forms_mapping,
				       field, &view_area);

	return cairo_region_create_rectangle (&view_area);
}

static gboolean
ev_view_forms_remove_widgets (EvView *view)
{
	ev_view_remove_all (view);

	return FALSE;
}

static void
ev_view_form_field_destroy (GtkWidget *widget,
			    EvView    *view)
{
	g_idle_add ((GSourceFunc)ev_view_forms_remove_widgets, view);
}

static GtkWidget *
ev_view_form_field_button_create_widget (EvView      *view,
					 EvFormField *field)
{
	EvFormFieldButton *field_button = EV_FORM_FIELD_BUTTON (field);
	cairo_region_t    *field_region = NULL;
	
	switch (field_button->type) {
	        case EV_FORM_FIELD_BUTTON_PUSH:
			return NULL;
	        case EV_FORM_FIELD_BUTTON_CHECK:
  	        case EV_FORM_FIELD_BUTTON_RADIO: {
			gboolean       state;
			EvMappingList *forms_mapping;
			GList         *l;

			state = ev_document_forms_form_field_button_get_state (EV_DOCUMENT_FORMS (view->document),
									       field);

			/* FIXME: it actually depends on NoToggleToOff flags */
			if (field_button->type == EV_FORM_FIELD_BUTTON_RADIO &&
			    state && field_button->state)
				return NULL;
			
			field_region = ev_view_form_field_get_region (view, field);

			/* For radio buttons and checkbox buttons that are in a set
			 * we need to update also the region for the current selected item
			 */
			forms_mapping = ev_page_cache_get_form_field_mapping (view->page_cache,
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
				cairo_region_union (field_region, button_region);
				cairo_region_destroy (button_region);
			}
			
			ev_document_forms_form_field_button_set_state (EV_DOCUMENT_FORMS (view->document),
								       field, !state);
			field_button->state = !state;
		}
			break;
	}

	ev_view_reload_page (view, field->page->index, field_region);
	cairo_region_destroy (field_region);
	
	return NULL;
}

static void
ev_view_form_field_text_save (EvView    *view,
			      GtkWidget *widget)
{
	EvFormField *field;

	if (!view->document)
		return;
	
	field = g_object_get_data (G_OBJECT (widget), "form-field");
	
	if (field->changed) {
		EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (field);
		cairo_region_t  *field_region;

		field_region = ev_view_form_field_get_region (view, field);
		
		ev_document_forms_form_field_text_set_text (EV_DOCUMENT_FORMS (view->document),
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
		text = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
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

static gboolean
ev_view_form_field_text_focus_out (GtkWidget     *widget,
				   GdkEventFocus *event,
				   EvView        *view)
{
	ev_view_form_field_text_save (view, widget);

	return FALSE;
}

static GtkWidget *
ev_view_form_field_text_create_widget (EvView      *view,
				       EvFormField *field)
{
	EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (field);
	GtkWidget       *text = NULL;
	gchar           *txt;

	txt = ev_document_forms_form_field_text_get_text (EV_DOCUMENT_FORMS (view->document),
							  field);

	switch (field_text->type) {
	        case EV_FORM_FIELD_TEXT_FILE_SELECT:
			/* TODO */
	        case EV_FORM_FIELD_TEXT_NORMAL:
			text = gtk_entry_new ();
			gtk_entry_set_has_frame (GTK_ENTRY (text), FALSE);
			gtk_entry_set_max_length (GTK_ENTRY (text), field_text->max_len);
			gtk_entry_set_visibility (GTK_ENTRY (text), !field_text->is_password);
			
			if (txt) {
				gtk_entry_set_text (GTK_ENTRY (text), txt);
				g_free (txt);
			}

			g_signal_connect (text, "focus-out-event",
					  G_CALLBACK (ev_view_form_field_text_focus_out),
					  view);
			g_signal_connect (text, "changed",
					  G_CALLBACK (ev_view_form_field_text_changed),
					  field);
			g_signal_connect_after (text, "activate",
						G_CALLBACK (ev_view_form_field_destroy),
						view);
			break;
	        case EV_FORM_FIELD_TEXT_MULTILINE: {
			GtkTextBuffer *buffer;
		
			text = gtk_text_view_new ();
			buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text));
			
			if (txt) {
				gtk_text_buffer_set_text (buffer, txt, -1);
				g_free (txt);
			}

			g_signal_connect( buffer, "focus-out-event",
					  G_CALLBACK (ev_view_form_field_text_focus_out),
					  view);
			g_signal_connect (buffer, "changed",
					  G_CALLBACK (ev_view_form_field_text_changed),
					  field);
		}
			break;
	}			

	g_object_weak_ref (G_OBJECT (text),
			   (GWeakNotify)ev_view_form_field_text_save,
			   view);

	return text;
}

static void
ev_view_form_field_choice_save (EvView    *view,
				GtkWidget *widget)
{
	EvFormField *field;

	if (!view->document)
		return;
	
	field = g_object_get_data (G_OBJECT (widget), "form-field");

	if (field->changed) {
		GList             *l;
		EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (field);
		cairo_region_t    *field_region;

		field_region = ev_view_form_field_get_region (view, field);

		if (field_choice->is_editable) {
			ev_document_forms_form_field_choice_set_text (EV_DOCUMENT_FORMS (view->document),
								      field, field_choice->text);
		} else {
			ev_document_forms_form_field_choice_unselect_all (EV_DOCUMENT_FORMS (view->document), field);
			for (l = field_choice->selected_items; l; l = g_list_next (l)) {
				ev_document_forms_form_field_choice_select_item (EV_DOCUMENT_FORMS (view->document),
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
		if (!field_choice->selected_items ||
		    GPOINTER_TO_INT (field_choice->selected_items->data) != item) {
			g_list_free (field_choice->selected_items);
			field_choice->selected_items = NULL;
			field_choice->selected_items = g_list_prepend (field_choice->selected_items,
								       GINT_TO_POINTER (item));
			field->changed = TRUE;
		}

		if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (widget))) {
			const gchar *text;

			text = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))));
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
		g_list_free (field_choice->selected_items);
		field_choice->selected_items = NULL;

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

static GtkWidget *
ev_view_form_field_choice_create_widget (EvView      *view,
					 EvFormField *field)
{
	EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (field);
	GtkWidget         *choice;
	GtkTreeModel      *model;
	gint               n_items, i;
	gint               selected_item = 0;

	n_items = ev_document_forms_form_field_choice_get_n_items (EV_DOCUMENT_FORMS (view->document),
								   field);
	model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT));
	for (i = 0; i < n_items; i++) {
		GtkTreeIter iter;
		gchar      *item;

		item = ev_document_forms_form_field_choice_get_item (EV_DOCUMENT_FORMS (view->document),
								     field, i);
		if (ev_document_forms_form_field_choice_is_item_selected (
			    EV_DOCUMENT_FORMS (view->document), field, i)) {
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

		choice = gtk_scrolled_window_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (choice), tree_view);
		gtk_widget_show (tree_view);

		g_signal_connect (selection, "changed",
				  G_CALLBACK (ev_view_form_field_choice_changed),
				  field);
		g_signal_connect_after (selection, "changed",
					G_CALLBACK (ev_view_form_field_destroy),
					view);
	} else if (field_choice->is_editable) { /* ComboBoxEntry */
		gchar *text;

                /* FIXME once gtk bug 633050 is fixed */
                choice = g_object_new (GTK_TYPE_COMBO_BOX, "has-entry", TRUE, "model", model, NULL);

		text = ev_document_forms_form_field_choice_get_text (EV_DOCUMENT_FORMS (view->document), field);
		if (text) {
			gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (choice))), text);
			g_free (text);
		}

		g_signal_connect (choice, "changed",
				  G_CALLBACK (ev_view_form_field_choice_changed),
				  field);
		g_signal_connect_after (gtk_bin_get_child (GTK_BIN (choice)),
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
		
		g_signal_connect (choice, "changed",
				  G_CALLBACK (ev_view_form_field_choice_changed),
				  field);
		g_signal_connect_after (choice, "changed",
					G_CALLBACK (ev_view_form_field_destroy),
					view);
	}

	g_object_unref (model);

	g_object_weak_ref (G_OBJECT (choice),
			   (GWeakNotify)ev_view_form_field_choice_save,
			   view);

	return choice;
}

static void
ev_view_handle_form_field (EvView      *view,
			   EvFormField *field,
			   gdouble      x,
			   gdouble      y)
{
	GtkWidget     *field_widget = NULL;
	EvMappingList *form_field_mapping;
	EvMapping     *mapping;

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
	if (!field_widget)
		return;

	g_object_set_data_full (G_OBJECT (field_widget), "form-field",
				g_object_ref (field),
				(GDestroyNotify)g_object_unref);

	form_field_mapping = ev_page_cache_get_form_field_mapping (view->page_cache,
								   field->page->index);
	mapping = ev_mapping_list_find (form_field_mapping, field);
	ev_view_put_to_doc_rect (view, field_widget, field->page->index, &mapping->area);
	gtk_widget_show (field_widget);
	gtk_widget_grab_focus (field_widget);
}

/* Annotations */
static EvViewWindowChild *
ev_view_get_window_child (EvView    *view,
			  GtkWidget *window)
{
	GList *children = view->window_children;

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
ev_view_window_child_move (EvView            *view,
			   EvViewWindowChild *child,
			   gint               x,
			   gint               y)
{
	GtkAllocation allocation;
	gint          width, height;

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);
	gtk_window_get_size (GTK_WINDOW (child->window), &width, &height);

	child->x = x;
	child->y = y;
	gtk_window_move (GTK_WINDOW (child->window),
			 CLAMP (x, child->parent_x,
				child->parent_x + allocation.width - width),
			 CLAMP (y, child->parent_y,
				child->parent_y + allocation.height - height));
}

static void
ev_view_window_child_move_with_parent (EvView    *view,
				       GtkWidget *window)
{
	EvViewWindowChild *child;
	gint               root_x, root_y;

	child = ev_view_get_window_child (view, window);
	gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (view)),
			       &root_x, &root_y);
	if (root_x != child->parent_x || root_y != child->parent_y) {
		gint dest_x, dest_y;

		dest_x = child->x + (root_x - child->parent_x);
		dest_y = child->y + (root_y - child->parent_y);
		child->parent_x = root_x;
		child->parent_y = root_y;
		ev_view_window_child_move (view, child, dest_x, dest_y);
	}

	if (child->visible && !gtk_widget_get_visible (window))
		gtk_widget_show (window);
}

static void
ev_view_window_child_put (EvView    *view,
			  GtkWidget *window,
			  guint      page,
			  gint       x,
			  gint       y,
			  gdouble    orig_x,
			  gdouble    orig_y)
{
	EvViewWindowChild *child;
	gint               root_x, root_y;

	gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (view)),
			       &root_x, &root_y);

	child = g_new0 (EvViewWindowChild, 1);
	child->window = window;
	child->page = page;
	child->orig_x = orig_x;
	child->orig_y = orig_y;
	child->parent_x = root_x;
	child->parent_y = root_y;
	child->visible = ev_annotation_window_is_open (EV_ANNOTATION_WINDOW (window));
	ev_view_window_child_move (view, child, x + root_x, y + root_y);

	if (child->visible)
		gtk_widget_show (window);
	else
		gtk_widget_hide (window);

	view->window_children = g_list_append (view->window_children, child);
}

static EvViewWindowChild *
ev_view_find_window_child_for_annot (EvView       *view,
				     guint         page,
				     EvAnnotation *annot)
{
	GList *children = view->window_children;

	while (children) {
		EvViewWindowChild *child;
		EvAnnotation      *wannot;

		child = (EvViewWindowChild *)children->data;
		children = children->next;

		if (child->page != page)
			continue;

		wannot = ev_annotation_window_get_annotation (EV_ANNOTATION_WINDOW (child->window));
		if (ev_annotation_equal (wannot, annot))
			return child;
	}

	return NULL;
}

static void
ev_view_window_children_free (EvView *view)
{
	GList *l;

	if (!view->window_children)
		return;

	for (l = view->window_children; l && l->data; l = g_list_next (l)) {
		EvViewWindowChild *child;

		child = (EvViewWindowChild *)l->data;
		gtk_widget_destroy (GTK_WIDGET (child->window));
		g_free (child);
	}
	g_list_free (view->window_children);
	view->window_children = NULL;
	view->window_child_focus = NULL;
}

static void
annotation_window_grab_focus (GtkWidget *widget,
			      EvView    *view)
{
	if (view->window_child_focus)
		ev_annotation_window_ungrab_focus (EV_ANNOTATION_WINDOW (view->window_child_focus->window));
	view->window_child_focus = ev_view_get_window_child (view, widget);
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
annotation_window_moved (EvAnnotationWindow *window,
			 gint                x,
			 gint                y,
			 EvView             *view)
{
	EvViewWindowChild *child;
	GdkRectangle       page_area;
	GtkBorder          border;
	GdkRectangle       view_rect;
	EvRectangle        doc_rect;
	gint               width, height;

	child = ev_view_get_window_child (view, GTK_WIDGET (window));
	if (child->x == x && child->y == y)
		return;

	child->moved = TRUE;
	child->x = x;
	child->y = y;

	/* Window has been moved by the user,
	 * we have to set a new origin in doc coords
	 */
	gtk_window_get_size (GTK_WINDOW (window), &width, &height);
	view_rect.x = (x - child->parent_x) + view->scroll_x;
	view_rect.y = (y - child->parent_y) + view->scroll_y;
	view_rect.width = width;
	view_rect.height = height;

	ev_view_get_page_extents (view, child->page, &page_area, &border);
	view_rect_to_doc_rect (view, &view_rect, &page_area, &doc_rect);
	child->orig_x = doc_rect.x1;
	child->orig_y = doc_rect.y1;
}

static void
ev_view_annotation_save_contents (EvView       *view,
				  GParamSpec   *pspec,
				  EvAnnotation *annot)
{
	if (!view->document)
		return;

	ev_document_doc_mutex_lock ();
	ev_document_annotations_save_annotation (EV_DOCUMENT_ANNOTATIONS (view->document),
						 annot, EV_ANNOTATIONS_SAVE_CONTENTS);
	ev_document_doc_mutex_unlock ();
}

static GtkWidget *
ev_view_create_annotation_window (EvView       *view,
				  EvAnnotation *annot,
				  GtkWindow    *parent)
{
	GtkWidget   *window;
	EvRectangle  doc_rect;
	GdkRectangle view_rect;
	guint        page;

	window = ev_annotation_window_new (annot, parent);
	g_signal_connect (window, "grab_focus",
			  G_CALLBACK (annotation_window_grab_focus),
			  view);
	g_signal_connect (window, "closed",
			  G_CALLBACK (annotation_window_closed),
			  view);
	g_signal_connect (window, "moved",
			  G_CALLBACK (annotation_window_moved),
			  view);
	g_signal_connect_swapped (annot, "notify::contents",
				  G_CALLBACK (ev_view_annotation_save_contents),
				  view);
	g_object_set_data (G_OBJECT (annot), "popup", window);

	page = ev_annotation_get_page_index (annot);
	ev_annotation_window_get_rectangle (EV_ANNOTATION_WINDOW (window), &doc_rect);
	doc_rect_to_view_rect (view, page, &doc_rect, &view_rect);
	view_rect.x -= view->scroll_x;
	view_rect.y -= view->scroll_y;

	ev_view_window_child_put (view, window, page,
				  view_rect.x, view_rect.y,
				  doc_rect.x1, doc_rect.y1);

	return window;
}

static void
show_annotation_windows (EvView *view,
			 gint    page)
{
	EvMappingList *annots;
	GList         *l;
	GtkWindow     *parent;

	parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));

	annots = ev_page_cache_get_annot_mapping (view->page_cache, page);

	for (l = ev_mapping_list_get_list (annots); l && l->data; l = g_list_next (l)) {
		EvAnnotation      *annot;
		EvViewWindowChild *child;
		GtkWidget         *window;

		annot = ((EvMapping *)(l->data))->data;

		if (!EV_IS_ANNOTATION_MARKUP (annot))
			continue;

		if (!ev_annotation_markup_has_popup (EV_ANNOTATION_MARKUP (annot)))
			continue;

		window = g_object_get_data (G_OBJECT (annot), "popup");
		if (window) {
			ev_view_window_child_move_with_parent (view, window);
			continue;
		}

		/* Look if we already have a popup for this annot */
		child = ev_view_find_window_child_for_annot (view, page, annot);
		window = child ? child->window : NULL;
		if (window) {
			ev_annotation_window_set_annotation (EV_ANNOTATION_WINDOW (window), annot);
			g_object_set_data (G_OBJECT (annot), "popup", window);
			ev_view_window_child_move_with_parent (view, window);
		} else {
			ev_view_create_annotation_window (view, annot, parent);
		}
	}
}

static void
hide_annotation_windows (EvView *view,
			 gint    page)
{
	EvMappingList *annots;
	GList         *l;

	annots = ev_page_cache_get_annot_mapping (view->page_cache, page);

	for (l = ev_mapping_list_get_list (annots); l && l->data; l = g_list_next (l)) {
		EvAnnotation *annot;
		GtkWidget    *window;

		annot = ((EvMapping *)(l->data))->data;

		if (!EV_IS_ANNOTATION_MARKUP (annot))
			continue;

		window = g_object_get_data (G_OBJECT (annot), "popup");
		if (window)
			gtk_widget_hide (window);
	}
}

static EvAnnotation *
ev_view_get_annotation_at_location (EvView  *view,
				    gdouble  x,
				    gdouble  y)
{
	gint page = -1;
	gint x_new = 0, y_new = 0;
	EvMappingList *annotations_mapping;

	if (!EV_IS_DOCUMENT_ANNOTATIONS (view->document))
		return NULL;

	if (!get_doc_point_from_location (view, x, y, &page, &x_new, &y_new))
		return NULL;

	annotations_mapping = ev_page_cache_get_annot_mapping (view->page_cache, page);

	if (annotations_mapping)
		return ev_mapping_list_get_data (annotations_mapping, x_new, y_new);
	else
		return NULL;
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
		ev_view_window_child_move (view, child, child->x, child->y);
		gtk_widget_show (window);
	}
}

static void
ev_view_handle_annotation (EvView       *view,
			   EvAnnotation *annot,
			   gdouble       x,
			   gdouble       y,
			   guint32       timestamp)
{
	if (EV_IS_ANNOTATION_MARKUP (annot)) {
		GtkWidget *window;

		window = g_object_get_data (G_OBJECT (annot), "popup");
		ev_view_annotation_show_popup_window (view, window);
	}

	if (EV_IS_ANNOTATION_ATTACHMENT (annot)) {
		EvAttachment *attachment;

		attachment = ev_annotation_attachment_get_attachment (EV_ANNOTATION_ATTACHMENT (annot));
		if (attachment) {
			GError *error = NULL;

			ev_attachment_open (attachment,
					    gtk_widget_get_screen (GTK_WIDGET (view)),
					    timestamp,
					    &error);

			if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		}
	}
}

static void
ev_view_create_annotation (EvView          *view,
			   EvAnnotationType annot_type,
			   gint             x,
			   gint             y)
{
	EvAnnotation   *annot;
	GdkPoint        point;
	GdkRectangle    page_area;
	GtkBorder       border;
	EvRectangle     doc_rect, popup_rect;
	EvPage         *page;
	GdkColor        color = { 0, 65535, 65535, 0 };
	GdkRectangle    view_rect;
	cairo_region_t *region;

	point.x = x;
	point.y = y;
	ev_view_get_page_extents (view, view->current_page, &page_area, &border);
	view_point_to_doc_point (view, &point, &page_area,
				 &doc_rect.x1, &doc_rect.y1);
	doc_rect.x2 = doc_rect.x1 + 24;
	doc_rect.y2 = doc_rect.y1 + 24;

	ev_document_doc_mutex_lock ();
	page = ev_document_get_page (view->document, view->current_page);
	switch (annot_type) {
	case EV_ANNOTATION_TYPE_TEXT:
		annot = ev_annotation_text_new (page);
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

	ev_annotation_set_color (annot, &color);

	if (EV_IS_ANNOTATION_MARKUP (annot)) {
		popup_rect.x1 = doc_rect.x2;
		popup_rect.x2 = popup_rect.x1 + 200;
		popup_rect.y1 = doc_rect.y2;
		popup_rect.y2 = popup_rect.y1 + 150;
		g_object_set (annot,
			      "rectangle", &popup_rect,
			      "has_popup", TRUE,
			      "popup_is_open", FALSE,
			      "label", g_get_real_name (),
			      "opacity", 1.0,
			      NULL);
	}
	ev_document_annotations_add_annotation (EV_DOCUMENT_ANNOTATIONS (view->document),
						annot, &doc_rect);
	ev_document_doc_mutex_unlock ();

	/* If the page didn't have annots, mark the cache as dirty */
	if (!ev_page_cache_get_annot_mapping (view->page_cache, view->current_page))
		ev_page_cache_mark_dirty (view->page_cache, view->current_page);

	if (EV_IS_ANNOTATION_MARKUP (annot)) {
		GtkWindow *parent;
		GtkWidget *window;

		parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));
		window = ev_view_create_annotation_window (view, annot, parent);

		/* Show the annot window the first time */
		ev_view_annotation_show_popup_window (view, window);
	}

	doc_rect_to_view_rect (view, view->current_page, &doc_rect, &view_rect);
	view_rect.x -= view->scroll_x;
	view_rect.y -= view->scroll_y;
	region = cairo_region_create_rectangle (&view_rect);
	ev_view_reload_page (view, view->current_page, region);
	cairo_region_destroy (region);

	g_signal_emit (view, signals[SIGNAL_ANNOT_ADDED], 0, annot);
}

void
ev_view_focus_annotation (EvView    *view,
			  EvMapping *annot_mapping)
{
	GdkRectangle  view_rect;
	EvAnnotation *annot;
	guint         page;

	if (!EV_IS_DOCUMENT_ANNOTATIONS (view->document))
		return;

	if (view->focus_annotation == annot_mapping)
		return;

	view->focus_annotation = annot_mapping;
	annot = (EvAnnotation *)annot_mapping->data;

	page = ev_annotation_get_page_index (annot);
	ev_document_model_set_page (view->model, page);

	doc_rect_to_view_rect (view, page,
			       &annot_mapping->area, &view_rect);
	ensure_rectangle_is_visible (view, &view_rect);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_begin_add_annotation (EvView          *view,
			      EvAnnotationType annot_type)
{
	if (annot_type == EV_ANNOTATION_TYPE_UNKNOWN)
		return;

	if (view->adding_annot)
		return;

	view->adding_annot = TRUE;
	view->adding_annot_type = annot_type;
	ev_view_set_cursor (view, EV_VIEW_CURSOR_ADD);
}

void
ev_view_cancel_add_annotation (EvView *view)
{
	gint x, y;

	if (!view->adding_annot)
		return;

	view->adding_annot = FALSE;
	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y);
}

static gboolean
ev_view_synctex_backward_search (EvView *view,
				 gdouble x,
				 gdouble y)
{
	gint page = -1;
	gint x_new = 0, y_new = 0;
	EvSourceLink *link;

	if (!ev_document_has_synctex (view->document))
		return FALSE;

	if (!get_doc_point_from_location (view, x, y, &page, &x_new, &y_new))
		return FALSE;

	link = ev_document_synctex_backward_search (view->document, page, x_new, y_new);
	if (link) {
		g_signal_emit (view, signals[SIGNAL_SYNC_SOURCE], 0, link);
		ev_source_link_free (link);

		return TRUE;
	}

	return FALSE;
}

/*** GtkWidget implementation ***/

static void
ev_view_size_request_continuous_dual_page (EvView         *view,
			     	           GtkRequisition *requisition)
{
	gint n_pages;

	n_pages = ev_document_get_n_pages (view->document) + 1;
	get_page_y_offset (view, n_pages, &requisition->height);

	switch (view->sizing_mode) {
	        case EV_SIZING_FIT_WIDTH:
	        case EV_SIZING_BEST_FIT:
			requisition->width = 1;

			break;
	        case EV_SIZING_FREE: {
			gint max_width;
			GtkBorder border;

			ev_view_get_max_page_size (view, &max_width, NULL);
			compute_border (view, max_width, max_width, &border);
			requisition->width = (max_width + border.left + border.right) * 2 + (view->spacing * 3);
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

	n_pages = ev_document_get_n_pages (view->document);
	get_page_y_offset (view, n_pages, &requisition->height);

	switch (view->sizing_mode) {
	        case EV_SIZING_FIT_WIDTH:
	        case EV_SIZING_BEST_FIT:
			requisition->width = 1;

			break;
	        case EV_SIZING_FREE: {
			gint max_width;
			GtkBorder border;

			ev_view_get_max_page_size (view, &max_width, NULL);
			compute_border (view, max_width, max_width, &border);
			requisition->width = max_width + (view->spacing * 2) + border.left + border.right;
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

	if (view->sizing_mode == EV_SIZING_BEST_FIT) {
		requisition->width = 1;
		requisition->height = 1;

		return;
	}

	/* Find the largest of the two. */
	ev_view_get_page_size (view,
			       view->current_page,
			       &width, &height);
	if (view->current_page + 1 < ev_document_get_n_pages (view->document)) {
		gint width_2, height_2;
		ev_view_get_page_size (view,
				       view->current_page + 1,
				       &width_2, &height_2);
		if (width_2 > width) {
			width = width_2;
			height = height_2;
		}
	}
	compute_border (view, width, height, &border);

	requisition->width = view->sizing_mode == EV_SIZING_FIT_WIDTH ? 1 :
		((width + border.left + border.right) * 2) + (view->spacing * 3);
	requisition->height = (height + border.top + border.bottom) + (view->spacing * 2);
}

static void
ev_view_size_request_single_page (EvView         *view,
				  GtkRequisition *requisition)
{
	GtkBorder border;
	gint width, height;

	if (view->sizing_mode == EV_SIZING_BEST_FIT) {
		requisition->width = 1;
		requisition->height = 1;

		return;
	}

	ev_view_get_page_size (view, view->current_page, &width, &height);
	compute_border (view, width, height, &border);

	requisition->width = view->sizing_mode == EV_SIZING_FIT_WIDTH ? 1 :
		width + border.left + border.right + (2 * view->spacing);
	requisition->height = height + border.top + border.bottom + (2 * view->spacing);
}

static void
ev_view_size_request (GtkWidget      *widget,
		      GtkRequisition *requisition)
{
	EvView *view = EV_VIEW (widget);

	if (view->document == NULL) {
		view->requisition.width = 1;
		view->requisition.height = 1;

		*requisition = view->requisition;

		return;
	}

	/* Get zoom for size here when not called from
	 * ev_view_size_allocate()
	 */
	if (!view->internal_size_request &&
	    (view->sizing_mode == EV_SIZING_FIT_WIDTH ||
	     view->sizing_mode == EV_SIZING_BEST_FIT)) {
		GtkAllocation allocation;

		gtk_widget_get_allocation (widget, &allocation);
		ev_view_zoom_for_size (view,
				       allocation.width,
				       allocation.height);
	}

	if (view->continuous && view->dual_page)
		ev_view_size_request_continuous_dual_page (view, &view->requisition);
	else if (view->continuous)
		ev_view_size_request_continuous (view, &view->requisition);
	else if (view->dual_page)
		ev_view_size_request_dual_page (view, &view->requisition);
	else
		ev_view_size_request_single_page (view, &view->requisition);

	*requisition = view->requisition;
}

static void
ev_view_get_preferred_width (GtkWidget *widget,
                             gint      *minimum,
                             gint      *natural)
{
        GtkRequisition requisition;

        ev_view_size_request (widget, &requisition);

        *minimum = *natural = requisition.width;
}

static void
ev_view_get_preferred_height (GtkWidget *widget,
                              gint      *minimum,
                              gint      *natural)
{
        GtkRequisition requisition;

        ev_view_size_request (widget, &requisition);

        *minimum = *natural = requisition.height;
}

static void
ev_view_size_allocate (GtkWidget      *widget,
		       GtkAllocation  *allocation)
{
	EvView *view = EV_VIEW (widget);
	GList  *l;
	gint    root_x, root_y;

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget))
		gdk_window_move_resize (gtk_widget_get_window (widget),
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);

	if (!view->document)
		return;

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH ||
	    view->sizing_mode == EV_SIZING_BEST_FIT) {
		GtkRequisition req;

		ev_view_zoom_for_size (view,
				       allocation->width,
				       allocation->height);
		view->internal_size_request = TRUE;
		ev_view_size_request (widget, &req);
		view->internal_size_request = FALSE;
	}

	ev_view_set_adjustment_values (view, GTK_ORIENTATION_HORIZONTAL);
	ev_view_set_adjustment_values (view, GTK_ORIENTATION_VERTICAL);

	if (view->document)
		view_update_range_and_current_page (view);

	view->pending_scroll = SCROLL_TO_KEEP_POSITION;
	view->pending_resize = FALSE;
	view->pending_point.x = 0;
	view->pending_point.y = 0;

	for (l = view->children; l && l->data; l = g_list_next (l)) {
		GdkRectangle view_area;
		EvViewChild *child = (EvViewChild *)l->data;

		if (!gtk_widget_get_visible (child->widget))
			continue;

		doc_rect_to_view_rect (view, child->page, &child->doc_rect, &view_area);
		view_area.x -= view->scroll_x;
		view_area.y -= view->scroll_y;

		gtk_widget_set_size_request (child->widget, view_area.width, view_area.height);
		gtk_widget_size_allocate (child->widget, &view_area);
	}

	if (view->window_children)
		gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (view)),
				       &root_x, &root_y);

	for (l = view->window_children; l && l->data; l = g_list_next (l)) {
		EvViewWindowChild *child;
		EvRectangle        doc_rect;
		GdkRectangle       view_rect;

		child = (EvViewWindowChild *)l->data;

		ev_annotation_window_get_rectangle (EV_ANNOTATION_WINDOW (child->window), &doc_rect);
		if (child->moved) {
			doc_rect.x1 = child->orig_x;
			doc_rect.y1 = child->orig_y;
		}
		doc_rect_to_view_rect (view, child->page, &doc_rect, &view_rect);
		view_rect.x -= view->scroll_x;
		view_rect.y -= view->scroll_y;

		if (view_rect.x != child->orig_x || view_rect.y != child->orig_y) {
			child->parent_x = root_x;
			child->parent_y = root_y;
			ev_view_window_child_move (view, child, view_rect.x + root_x, view_rect.y + root_y);
		}
	}
}

static gboolean
ev_view_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
	EvView *view = EV_VIEW (widget);
	guint state;

        if (event->direction == GDK_SCROLL_SMOOTH)
                return FALSE;

	state = event->state & gtk_accelerator_get_default_mod_mask ();

	if (state == GDK_CONTROL_MASK) {
		ev_document_model_set_sizing_mode (view->model, EV_SIZING_FREE);
		if (event->direction == GDK_SCROLL_UP ||
		    event->direction == GDK_SCROLL_LEFT) {
			if (ev_view_can_zoom_in (view)) {
				ev_view_zoom_in (view);
			}
		} else {
			if (ev_view_can_zoom_out (view)) {
				ev_view_zoom_out (view);
			}
		}

		return TRUE;
	}

	view->jump_to_find_result = FALSE;

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

		event->state &= ~GDK_SHIFT_MASK;
		state &= ~GDK_SHIFT_MASK;
	}

	if (state == 0 && view->sizing_mode == EV_SIZING_BEST_FIT && !view->continuous) {
		switch (event->direction) {
		        case GDK_SCROLL_DOWN:
		        case GDK_SCROLL_RIGHT:
				ev_view_next_page (view);	
				break;
		        case GDK_SCROLL_UP:
		        case GDK_SCROLL_LEFT:
				ev_view_previous_page (view);
				break;
                        case GDK_SCROLL_SMOOTH:
                                g_assert_not_reached ();
		}

		return TRUE;
	}

	return FALSE;
}

static EvViewSelection *
find_selection_for_page (EvView *view,
			 gint    page)
{
	GList *list;

	for (list = view->selection_info.selections; list != NULL; list = list->next) {
		EvViewSelection *selection;

		selection = (EvViewSelection *) list->data;

		if (selection->page == page)
			return selection;
	}

	return NULL;
}

static void
ev_view_realize (GtkWidget *widget)
{
	GtkAllocation allocation;
	GdkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	gtk_widget_set_realized (widget, TRUE);

	gtk_widget_get_allocation (widget, &allocation);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.event_mask = gtk_widget_get_events (widget);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

	window = gdk_window_new (gtk_widget_get_parent_window (widget),
				 &attributes, attributes_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, widget);

	gtk_style_context_set_background (gtk_widget_get_style_context (widget),
					  window);
}

static gboolean
ev_view_draw (GtkWidget *widget,
              cairo_t   *cr)
{
	EvView      *view = EV_VIEW (widget);
	gint         i;
	GdkRectangle clip_rect;

	if (view->loading) {
		show_loading_window (view);
	} else if (view->loading_window &&
		   gtk_widget_get_visible (view->loading_window)) {
		ev_view_loading_window_move (view);
	}

	if (view->document == NULL)
		return FALSE;

        if (!gdk_cairo_get_clip_rectangle (cr, &clip_rect))
                return FALSE;

	for (i = view->start_page; i >= 0 && i <= view->end_page; i++) {
		GdkRectangle page_area;
		GtkBorder border;
		gboolean page_ready = TRUE;

		if (!ev_view_get_page_extents (view, i, &page_area, &border))
			continue;

		page_area.x -= view->scroll_x;
		page_area.y -= view->scroll_y;

		draw_one_page (view, i, cr, &page_area, &border, &clip_rect, &page_ready);

		if (page_ready && view->find_pages && view->highlight_find_results)
			highlight_find_results (view, cr, i);
		if (page_ready && EV_IS_DOCUMENT_ANNOTATIONS (view->document))
			show_annotation_windows (view, i);
		if (page_ready && view->focus_annotation)
                        focus_annotation (view, cr, i, &clip_rect);
		if (page_ready && view->synctex_result)
			highlight_forward_search_results (view, cr, i);
	}

        if (GTK_WIDGET_CLASS (ev_view_parent_class)->draw)
                GTK_WIDGET_CLASS (ev_view_parent_class)->draw (widget, cr);

	return FALSE;
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

	g_signal_emit (view, signals[SIGNAL_POPUP_MENU], 0, items);

	g_list_free (items);

	return TRUE;
}

static gboolean
ev_view_popup_menu (GtkWidget *widget)
{
	gint x, y;

	ev_document_misc_get_pointer_position (widget, &x, &y);
	return ev_view_do_popup_menu (EV_VIEW (widget), x, y);
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

	x += view->scroll_x;
	y += view->scroll_y;
	
	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);
	
	link_mapping = ev_page_cache_get_link_mapping (view->page_cache, page);
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

	x += view->scroll_x;
	y += view->scroll_y;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	annot_mapping = ev_page_cache_get_annot_mapping (view->page_cache, page);
	ev_view_get_area_from_mapping (view, page,
				       annot_mapping,
				       annot, area);
}

static gboolean
ev_view_query_tooltip (GtkWidget  *widget,
		       gint        x,
		       gint        y,
		       gboolean    keyboard_tip,
		       GtkTooltip *tooltip)
{
	EvView       *view = EV_VIEW (widget);
	EvLink       *link;
	EvAnnotation *annot;
	gchar        *text;

	annot = ev_view_get_annotation_at_location (view, x, y);
	if (annot) {
		const gchar *contents;

		if ((contents = ev_annotation_get_contents (annot))) {
			GdkRectangle annot_area;

			get_annot_area (view, x, y, annot, &annot_area);
			gtk_tooltip_set_text (tooltip, contents);
			gtk_tooltip_set_tip_area (tooltip, &annot_area);

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
start_selection_for_event (EvView         *view,
			   GdkEventButton *event)
{
	clear_selection (view);

	view->selection_info.start.x = event->x + view->scroll_x;
	view->selection_info.start.y = event->y + view->scroll_y;

	switch (event->type) {
	        case GDK_2BUTTON_PRESS:
			view->selection_info.style = EV_SELECTION_STYLE_WORD;
			break;
	        case GDK_3BUTTON_PRESS:
			view->selection_info.style = EV_SELECTION_STYLE_LINE;
			break;
	        default:
			view->selection_info.style = EV_SELECTION_STYLE_GLYPH;
			return;
	}

	/* In case of WORD or LINE, compute selections now */
	compute_selections (view,
			    view->selection_info.style,
			    &(view->selection_info.start),
			    &(view->selection_info.start));
}

static gboolean
ev_view_button_press_event (GtkWidget      *widget,
			    GdkEventButton *event)
{
	EvView *view = EV_VIEW (widget);

	if (!view->document)
		return FALSE;
	
	if (!gtk_widget_has_focus (widget)) {
		gtk_widget_grab_focus (widget);
	}

	if (view->window_child_focus) {
		EvAnnotationWindow *window;

		window = EV_ANNOTATION_WINDOW (view->window_child_focus->window);
		ev_annotation_window_ungrab_focus (window);
		view->window_child_focus = NULL;
	}
	
	view->pressed_button = event->button;
	view->selection_info.in_drag = FALSE;

	if (view->adding_annot)
		return FALSE;

	if (view->scroll_info.autoscrolling)
		return TRUE;
	
	switch (event->button) {
	        case 1: {
			EvImage *image;
			EvAnnotation *annot;
			EvFormField *field;

			if (event->state & GDK_CONTROL_MASK)
				return ev_view_synctex_backward_search (view, event->x , event->y);

			if (EV_IS_SELECTION (view->document) && view->selection_info.selections) {
				if (event->type == GDK_3BUTTON_PRESS) {
					start_selection_for_event (view, event);
				} else if (location_in_selected_text (view,
							       event->x + view->scroll_x,
							       event->y + view->scroll_y)) {
					view->selection_info.in_drag = TRUE;
				} else {
					start_selection_for_event (view, event);
				}

				gtk_widget_queue_draw (widget);
			} else if ((annot = ev_view_get_annotation_at_location (view, event->x, event->y))) {
				ev_view_handle_annotation (view, annot, event->x, event->y, event->time);
			} else if ((field = ev_view_get_form_field_at_location (view, event->x, event->y))) {
				ev_view_remove_all (view);
				ev_view_handle_form_field (view, field, event->x, event->y);
			} else if (!location_in_text (view, event->x + view->scroll_x, event->y + view->scroll_y) &&
				   (image = ev_view_get_image_at_location (view, event->x, event->y))) {
				if (view->image_dnd_info.image)
					g_object_unref (view->image_dnd_info.image);
				view->image_dnd_info.image = g_object_ref (image);
				view->image_dnd_info.in_drag = TRUE;

				view->image_dnd_info.start.x = event->x + view->scroll_x;
				view->image_dnd_info.start.y = event->y + view->scroll_y;
			} else {
				ev_view_remove_all (view);

				if (view->synctex_result) {
					g_free (view->synctex_result);
					view->synctex_result = NULL;
					gtk_widget_queue_draw (widget);
				}

				if (view->focus_annotation)
					view->focus_annotation = NULL;

				if (EV_IS_SELECTION (view->document))
					start_selection_for_event (view, event);
			}
		}			
			return TRUE;
		case 2:
			/* use root coordinates as reference point because
			 * scrolling changes window relative coordinates */
			view->drag_info.start.x = event->x_root;
			view->drag_info.start.y = event->y_root;
			view->drag_info.hadj = gtk_adjustment_get_value (view->hadjustment);
			view->drag_info.vadj = gtk_adjustment_get_value (view->vadjustment);

			ev_view_set_cursor (view, EV_VIEW_CURSOR_DRAG);

			return TRUE;
		case 3:
			view->scroll_info.start_y = event->y;
			return ev_view_do_popup_menu (view, event->x, event->y);
	}
	
	return FALSE;
}

static void
ev_view_remove_all (EvView *view)
{
	gtk_container_foreach (GTK_CONTAINER (view), (GtkCallback) gtk_widget_destroy, NULL);
}

/*** Drag and Drop ***/
static void
ev_view_drag_data_get (GtkWidget        *widget,
		       GdkDragContext   *context,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time)
{
	EvView *view = EV_VIEW (widget);

	switch (info) {
	        case TARGET_DND_TEXT:
			if (EV_IS_SELECTION (view->document) &&
			    view->selection_info.selections) {
				gchar *text;

				text = get_selected_text (view);
				gtk_selection_data_set_text (selection_data,
							     text,
							     strlen (text));
				g_free (text);
			}
			break;
	        case TARGET_DND_IMAGE:
			if (view->image_dnd_info.image) {
				GdkPixbuf *pixbuf;

				ev_document_doc_mutex_lock ();
				pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (view->document),
								       view->image_dnd_info.image);
				ev_document_doc_mutex_unlock ();
				
				gtk_selection_data_set_pixbuf (selection_data, pixbuf);
				g_object_unref (pixbuf);
			}
			break;
	        case TARGET_DND_URI:
			if (view->image_dnd_info.image) {
				GdkPixbuf   *pixbuf;
				const gchar *tmp_uri;
				gchar       *uris[2];

				ev_document_doc_mutex_lock ();
				pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (view->document),
								       view->image_dnd_info.image);
				ev_document_doc_mutex_unlock ();
				
				tmp_uri = ev_image_save_tmp (view->image_dnd_info.image, pixbuf);
				g_object_unref (pixbuf);

				uris[0] = (gchar *)tmp_uri;
                                uris[1] = NULL;
				gtk_selection_data_set_uris (selection_data, uris);
			}
	}
}

static gboolean
ev_view_drag_motion (GtkWidget      *widget,
		     GdkDragContext *context,
		     gint            x,
		     gint            y,
		     guint           time)
{
	if (gtk_drag_get_source_widget (context) == widget)
		gdk_drag_status (context, 0, time);
	else
		gdk_drag_status (context, gdk_drag_context_get_suggested_action (context), time);
	
	return TRUE;
}
		     
static gboolean
selection_update_idle_cb (EvView *view)
{
	compute_selections (view,
			    view->selection_info.style,
			    &view->selection_info.start,
			    &view->motion);
	view->selection_update_id = 0;
	return FALSE;
}

static gboolean
selection_scroll_timeout_cb (EvView *view)
{	
	gint x, y, shift = 0;
	GtkWidget *widget = GTK_WIDGET (view);
	GtkAllocation allocation;

	gtk_widget_get_allocation (widget, &allocation);
	ev_document_misc_get_pointer_position (widget, &x, &y);

	if (y > allocation.height) {
		shift = (y - allocation.height) / 2;
	} else if (y < 0) {
		shift = y / 2;
	}

	if (shift)
		gtk_adjustment_set_value (view->vadjustment,
					  CLAMP (gtk_adjustment_get_value (view->vadjustment) + shift,
						 gtk_adjustment_get_lower (view->vadjustment),
						 gtk_adjustment_get_upper (view->vadjustment) -
						 gtk_adjustment_get_page_size (view->vadjustment)));

	if (x > allocation.width) {
		shift = (x - allocation.width) / 2;
	} else if (x < 0) {
		shift = x / 2;
	}

	if (shift)
		gtk_adjustment_set_value (view->hadjustment,
					  CLAMP (gtk_adjustment_get_value (view->hadjustment) + shift,
						 gtk_adjustment_get_lower (view->hadjustment),
						 gtk_adjustment_get_upper (view->hadjustment) -
						 gtk_adjustment_get_page_size (view->hadjustment)));

	return TRUE;
}

static gboolean
ev_view_drag_update_momentum (EvView *view)
{
	int i;
	if (!view->drag_info.in_drag)
		return FALSE;
	
	for (i = DRAG_HISTORY - 1; i > 0; i--) {
		view->drag_info.buffer[i].x = view->drag_info.buffer[i-1].x;
		view->drag_info.buffer[i].y = view->drag_info.buffer[i-1].y;
	}

	/* Momentum is a moving average of 10ms granularity over
	 * the last 100ms with each 10ms stored in buffer. 
	 */
	
	view->drag_info.momentum.x = (view->drag_info.buffer[DRAG_HISTORY - 1].x - view->drag_info.buffer[0].x);
	view->drag_info.momentum.y = (view->drag_info.buffer[DRAG_HISTORY - 1].y - view->drag_info.buffer[0].y);

	return TRUE;
}

static gboolean
ev_view_scroll_drag_release (EvView *view)
{
	gdouble dhadj_value, dvadj_value;
	gdouble oldhadjustment, oldvadjustment;
	gdouble h_page_size, v_page_size;
	gdouble h_upper, v_upper;
	GtkAllocation allocation;

	view->drag_info.momentum.x /= 1.2;
	view->drag_info.momentum.y /= 1.2; /* Alter these constants to change "friction" */

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	h_page_size = gtk_adjustment_get_page_size (view->hadjustment);
	v_page_size = gtk_adjustment_get_page_size (view->vadjustment);

	dhadj_value = h_page_size *
		      (gdouble)view->drag_info.momentum.x / allocation.width;
	dvadj_value = v_page_size *
		      (gdouble)view->drag_info.momentum.y / allocation.height;

	oldhadjustment = gtk_adjustment_get_value (view->hadjustment);
	oldvadjustment = gtk_adjustment_get_value (view->vadjustment);

	h_upper = gtk_adjustment_get_upper (view->hadjustment);
	v_upper = gtk_adjustment_get_upper (view->vadjustment);

	/* When we reach the edges, we need either to absorb some momentum and bounce by
	 * multiplying it on -0.5 or stop scrolling by setting momentum to 0. */
	if (((oldhadjustment + dhadj_value) > (h_upper - h_page_size)) ||
	    ((oldhadjustment + dhadj_value) < 0))
		view->drag_info.momentum.x = 0;
	if (((oldvadjustment + dvadj_value) > (v_upper - v_page_size)) ||
	    ((oldvadjustment + dvadj_value) < 0))
		view->drag_info.momentum.y = 0;

	gtk_adjustment_set_value (view->hadjustment,
				  MIN (oldhadjustment + dhadj_value,
				       h_upper - h_page_size));
	gtk_adjustment_set_value (view->vadjustment,
				  MIN (oldvadjustment + dvadj_value,
				       v_upper - v_page_size));

	if (((view->drag_info.momentum.x < 1) && (view->drag_info.momentum.x > -1)) &&
	    ((view->drag_info.momentum.y < 1) && (view->drag_info.momentum.y > -1)))
		return FALSE;
	else
		return TRUE;
}

static gboolean
ev_view_motion_notify_event (GtkWidget      *widget,
			     GdkEventMotion *event)
{
	EvView    *view = EV_VIEW (widget);
	GdkWindow *window;
	gint       x, y;

	if (!view->document)
		return FALSE;

	window = gtk_widget_get_window (widget);

        if (event->is_hint || event->window != window) {
	    ev_document_misc_get_pointer_position (widget, &x, &y);
        } else {
	    x = event->x;
	    y = event->y;
	}

	if (view->scroll_info.autoscrolling) {
		view->scroll_info.last_y = y;
		return TRUE;
	}

	if (view->selection_info.in_drag) {
		if (gtk_drag_check_threshold (widget,
					      view->selection_info.start.x,
					      view->selection_info.start.y,
					      x, y)) {
			GtkTargetList *target_list = gtk_target_list_new (NULL, 0);

			gtk_target_list_add_text_targets (target_list, TARGET_DND_TEXT);

			gtk_drag_begin (widget, target_list,
					GDK_ACTION_COPY,
					1, (GdkEvent *)event);

			view->selection_info.in_drag = FALSE;

			gtk_target_list_unref (target_list);

			return TRUE;
		}
	} else if (view->image_dnd_info.in_drag) {
		if (gtk_drag_check_threshold (widget,
					      view->selection_info.start.x,
					      view->selection_info.start.y,
					      x, y)) {
			GtkTargetList *target_list = gtk_target_list_new (NULL, 0);

			gtk_target_list_add_uri_targets (target_list, TARGET_DND_URI);
			gtk_target_list_add_image_targets (target_list, TARGET_DND_IMAGE, TRUE);

			gtk_drag_begin (widget, target_list,
					GDK_ACTION_COPY,
					1, (GdkEvent *)event);

			view->image_dnd_info.in_drag = FALSE;

			gtk_target_list_unref (target_list);

			return TRUE;
		}
	}
	
	switch (view->pressed_button) {
	case 1:
		/* For the Evince 0.4.x release, we limit selection to un-rotated
		 * documents only.
		 */
		if (view->rotation != 0)
			return FALSE;

		/* Schedule timeout to scroll during selection and additionally 
		 * scroll once to allow arbitrary speed. */
		if (!view->selection_scroll_id)
		    view->selection_scroll_id = g_timeout_add (SCROLL_TIME,
							       (GSourceFunc)selection_scroll_timeout_cb,
							       view);
		else 
		    selection_scroll_timeout_cb (view);

		view->selection_info.in_selection = TRUE;
		view->motion.x = x + view->scroll_x;
		view->motion.y = y + view->scroll_y;

		/* Queue an idle to handle the motion.  We do this because	
		 * handling any selection events in the motion could be slower	
		 * than new motion events reach us.  We always put it in the	
		 * idle to make sure we catch up and don't visibly lag the	
		 * mouse. */
		if (!view->selection_update_id)
			view->selection_update_id = g_idle_add ((GSourceFunc)selection_update_idle_cb, view);

		return TRUE;
	case 2:
		if (!view->drag_info.in_drag) {
			gboolean start;
			int i;

			start = gtk_drag_check_threshold (widget,
							  view->drag_info.start.x,
							  view->drag_info.start.y,
							  event->x_root,
							  event->y_root);
			view->drag_info.in_drag = start;
			view->drag_info.drag_timeout_id = g_timeout_add (10,
				(GSourceFunc)ev_view_drag_update_momentum, view);
			/* Set 100 to choose how long it takes to build up momentum */
			/* Clear out previous momentum info: */
			for (i = 0; i < DRAG_HISTORY; i++) {
				view->drag_info.buffer[i].x = event->x;
				view->drag_info.buffer[i].y = event->y;
			}
			view->drag_info.momentum.x = 0;
			view->drag_info.momentum.y = 0;
		}

		if (view->drag_info.in_drag) {
			int dx, dy;
			gdouble dhadj_value, dvadj_value;
			GtkAllocation allocation;

			view->drag_info.buffer[0].x = event->x;
			view->drag_info.buffer[0].y = event->y;

			dx = event->x_root - view->drag_info.start.x;
			dy = event->y_root - view->drag_info.start.y;

			gtk_widget_get_allocation (widget, &allocation);

			dhadj_value = gtk_adjustment_get_page_size (view->hadjustment) *
				      (gdouble)dx / allocation.width;
			dvadj_value = gtk_adjustment_get_page_size (view->vadjustment) *
				      (gdouble)dy / allocation.height;

			/* clamp scrolling to visible area */
			gtk_adjustment_set_value (view->hadjustment,
						  MIN (view->drag_info.hadj - dhadj_value,
						       gtk_adjustment_get_upper (view->hadjustment) -
						       gtk_adjustment_get_page_size (view->hadjustment)));
			gtk_adjustment_set_value (view->vadjustment,
						  MIN (view->drag_info.vadj - dvadj_value,
						       gtk_adjustment_get_upper (view->vadjustment) -
						       gtk_adjustment_get_page_size (view->vadjustment)));

			return TRUE;
		}

		break;
	default:
		ev_view_handle_cursor_over_xy (view, x, y);
	} 

	return FALSE;
}

static gboolean
ev_view_button_release_event (GtkWidget      *widget,
			      GdkEventButton *event)
{
	EvView *view = EV_VIEW (widget);
	EvLink *link = NULL;

	view->image_dnd_info.in_drag = FALSE;

	if (view->scroll_info.autoscrolling) {
		ev_view_autoscroll_stop (view);
		view->pressed_button = -1;

		return TRUE;
	}

	if (view->pressed_button == 1 && event->state & GDK_CONTROL_MASK) {
		view->pressed_button = -1;
		return TRUE;
	}

	if (view->drag_info.in_drag) {
		view->drag_info.release_timeout_id =
			g_timeout_add (20,
				       (GSourceFunc)ev_view_scroll_drag_release, view);
	}

	if (view->document && !view->drag_info.in_drag && view->pressed_button != 3) {
		link = ev_view_get_link_at_location (view, event->x, event->y);
	}

	view->drag_info.in_drag = FALSE;

	if (view->adding_annot && view->pressed_button == 1) {
		view->adding_annot = FALSE;
		ev_view_handle_cursor_over_xy (view, event->x, event->y);
		view->pressed_button = -1;

		ev_view_create_annotation (view,
					   view->adding_annot_type,
					   event->x + view->scroll_x,
					   event->y + view->scroll_y);

		return FALSE;
	}

	if (view->pressed_button == 2) {
		ev_view_handle_cursor_over_xy (view, event->x, event->y);
	}

	view->pressed_button = -1;

	if (view->selection_scroll_id) {
	    g_source_remove (view->selection_scroll_id);
	    view->selection_scroll_id = 0;
	}
	if (view->selection_update_id) {
	    g_source_remove (view->selection_update_id);
	    view->selection_update_id = 0;
	}

	if (view->selection_info.selections) {
		clear_link_selected (view);
		ev_view_update_primary_selection (view);
		
		if (view->selection_info.in_drag) {
			clear_selection (view);
			gtk_widget_queue_draw (widget);
		}
		
		view->selection_info.in_drag = FALSE;
	} else if (link) {
		if (event->button == 2) {
			EvLinkAction    *action;
			EvLinkActionType type;

			action = ev_link_get_action (link);
			if (!action)
				return FALSE;

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

	return FALSE;
}

static gboolean
ev_view_key_press_event (GtkWidget   *widget,
			 GdkEventKey *event)
{
	EvView *view = EV_VIEW (widget);

	if (!view->document)
		return FALSE;

	if (!gtk_widget_has_focus (widget)) {
		/* Forward key events to current focused window child */
		if (view->window_child_focus) {
			GdkEventKey *new_event;
			gboolean     handled;

			new_event = (GdkEventKey *) gdk_event_copy ((GdkEvent *)event);
			g_object_unref (new_event->window);
			new_event->window = gtk_widget_get_window (view->window_child_focus->window);
			if (new_event->window)
				g_object_ref (new_event->window);
			gtk_widget_realize (view->window_child_focus->window);
			handled = gtk_widget_event (view->window_child_focus->window, (GdkEvent *)new_event);
			gdk_event_free ((GdkEvent *)new_event);

			return handled;
		}

		return FALSE;
	}

	return gtk_bindings_activate_event (G_OBJECT (widget), event);
}

static gint
ev_view_focus_in (GtkWidget     *widget,
		  GdkEventFocus *event)
{
	if (EV_VIEW (widget)->pixbuf_cache)
		ev_pixbuf_cache_style_changed (EV_VIEW (widget)->pixbuf_cache);
	gtk_widget_queue_draw (widget);

	return FALSE;
}

static gint
ev_view_focus_out (GtkWidget     *widget,
		   GdkEventFocus *event)
{
	if (EV_VIEW (widget)->pixbuf_cache)
		ev_pixbuf_cache_style_changed (EV_VIEW (widget)->pixbuf_cache);
	gtk_widget_queue_draw (widget);

	return FALSE;
}

static gboolean
ev_view_leave_notify_event (GtkWidget *widget, GdkEventCrossing   *event)
{
	EvView *view = EV_VIEW (widget);

	if (view->cursor != EV_VIEW_CURSOR_NORMAL)
		ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);

	return FALSE;
}

static gboolean
ev_view_enter_notify_event (GtkWidget *widget, GdkEventCrossing   *event)
{
	EvView *view = EV_VIEW (widget);

	ev_view_handle_cursor_over_xy (view, event->x, event->y);
    
	return FALSE;
}

static void
ev_view_style_updated (GtkWidget *widget)
{
	if (EV_VIEW (widget)->pixbuf_cache)
		ev_pixbuf_cache_style_changed (EV_VIEW (widget)->pixbuf_cache);

	GTK_WIDGET_CLASS (ev_view_parent_class)->style_updated (widget);
}

/*** Drawing ***/

static void
draw_rubberband (EvView             *view,
		 cairo_t            *cr,
		 const GdkRectangle *rect,
		 gdouble             alpha)
{
	GtkStyleContext *context;
	GdkRGBA          color;

	context = gtk_widget_get_style_context (GTK_WIDGET (view));
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_SELECTED, &color);
        cairo_save (cr);

	cairo_set_source_rgba (cr, color.red, color.green, color.blue, alpha);
	cairo_rectangle (cr,
			 rect->x - view->scroll_x,
			 rect->y - view->scroll_y,
			 rect->width, rect->height);
	cairo_fill_preserve (cr);

	cairo_set_line_width (cr, 0.5);
	cairo_set_source_rgb (cr, color.red, color.green, color.blue);
	cairo_stroke (cr);

	cairo_restore (cr);
}


static void
highlight_find_results (EvView *view,
                        cairo_t *cr,
                        int page)
{
	gint       i, n_results = 0;
	n_results = ev_view_find_get_n_results (view, page);

	for (i = 0; i < n_results; i++) {
		EvRectangle *rectangle;
		GdkRectangle view_rectangle;
		gdouble      alpha;

		if (i == view->find_result && page == view->current_page) {
			alpha = 0.6;
		} else {
			alpha = 0.3;
		}

		rectangle = ev_view_find_get_result (view, page, i);
		doc_rect_to_view_rect (view, page, rectangle, &view_rectangle);
		draw_rubberband (view, cr, &view_rectangle, alpha);
        }
}

static void
highlight_forward_search_results (EvView *view,
                                  cairo_t *cr,
                                  int page)
{
	GdkRectangle rect;
	EvMapping   *mapping = view->synctex_result;

	if (GPOINTER_TO_INT (mapping->data) != page)
		return;

	doc_rect_to_view_rect (view, page, &mapping->area, &rect);

        cairo_save (cr);
	cairo_set_source_rgb (cr, 1., 0., 0.);
	cairo_rectangle (cr,
			 rect.x - view->scroll_x,
			 rect.y - view->scroll_y,
			 rect.width, rect.height);
	cairo_stroke (cr);
	cairo_restore (cr);
}

static void
focus_annotation (EvView       *view,
                  cairo_t      *cr,
		  gint          page,
		  GdkRectangle *clip)
{
	GtkWidget    *widget = GTK_WIDGET (view);
	GdkRectangle  rect;
	EvMapping    *mapping = view->focus_annotation;
	EvAnnotation *annot = (EvAnnotation *)mapping->data;

	if (ev_annotation_get_page_index (annot) != page)
		return;

	doc_rect_to_view_rect (view, page, &mapping->area, &rect);

        gtk_render_focus (gtk_widget_get_style_context (widget),
                          cr,
                          rect.x - view->scroll_x,
                          rect.y - view->scroll_y,
                          rect.width + 1, rect.height + 1);
}

static void
ev_view_loading_window_move (EvView *view)
{
	GtkWidget       *widget = GTK_WIDGET (view);
	EvLoadingWindow *window = EV_LOADING_WINDOW (view->loading_window);
	gint             root_x, root_y;
	gint             window_width;
	GtkAllocation    allocation;

	gtk_widget_get_allocation (widget, &allocation);
	gdk_window_get_origin (gtk_widget_get_window (widget), &root_x, &root_y);
	ev_loading_window_get_size (window, &window_width, NULL);

	root_x += allocation.width - window_width - 10;
	root_y += 10;

	ev_loading_window_move (window, root_x, root_y);
}

static gboolean
show_loading_window_cb (EvView *view)
{
	if (!view->loading_window) {
		GtkWindow *parent;
		GdkScreen *screen;

		parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));
		view->loading_window = ev_loading_window_new (parent);

		/* Show the window off screen to get a valid size asap */
		screen = gtk_widget_get_screen (GTK_WIDGET (view));
		gtk_window_move (GTK_WINDOW (view->loading_window),
				 gdk_screen_get_width (screen) + 1,
				 gdk_screen_get_height (screen) + 1);
		gtk_widget_show (view->loading_window);
	}

	ev_view_loading_window_move (view);

	gtk_widget_show (view->loading_window);

	view->loading_timeout = 0;

	return FALSE;
}

static void
show_loading_window (EvView *view)
{
	if (view->loading_window && gtk_widget_get_visible (view->loading_window)) {
		ev_view_loading_window_move (view);
		return;
	}

	if (!view->loading_timeout) {
		view->loading_timeout =
			g_timeout_add_full (G_PRIORITY_LOW,
					    0.5, (GSourceFunc)show_loading_window_cb,
					    view, NULL);
	}
}

static void
hide_loading_window (EvView *view)
{
	if (view->loading_timeout) {
		g_source_remove (view->loading_timeout);
		view->loading_timeout = 0;
	}

	if (view->loading_window && gtk_widget_get_visible (view->loading_window)) {
		gtk_widget_hide (view->loading_window);
	}
}

static void
draw_one_page (EvView       *view,
	       gint          page,
	       cairo_t      *cr,
	       GdkRectangle *page_area,
	       GtkBorder    *border,
	       GdkRectangle *expose_area,
	       gboolean     *page_ready)
{
	GdkRectangle overlap;
	GdkRectangle real_page_area;
	gint         current_page;
	gboolean     inverted_colors;

	g_assert (view->document);

	if (! gdk_rectangle_intersect (page_area, expose_area, &overlap))
		return;

	/* Render the document itself */
	real_page_area = *page_area;

	real_page_area.x += border->left;
	real_page_area.y += border->top;
	real_page_area.width -= (border->left + border->right);
	real_page_area.height -= (border->top + border->bottom);
	*page_ready = TRUE;

	current_page = ev_document_model_get_page (view->model);
	inverted_colors = ev_document_model_get_inverted_colors (view->model);
	ev_document_misc_paint_one_page (cr,
					 GTK_WIDGET (view),
					 page_area, border,
					 page == current_page,
					 inverted_colors);

	if (gdk_rectangle_intersect (&real_page_area, expose_area, &overlap)) {
		gint             width, height;
		gint             page_width, page_height;
		cairo_surface_t *page_surface = NULL;
		gint             selection_width, selection_height;
		cairo_surface_t *selection_surface = NULL;

		page_surface = ev_pixbuf_cache_get_surface (view->pixbuf_cache, page);

		if (!page_surface) {
			if (page == current_page)
				show_loading_window (view);

			*page_ready = FALSE;

			return;
		}

		if (page == current_page)
			hide_loading_window (view);

		ev_view_get_page_size (view, page, &width, &height);

		page_width = cairo_image_surface_get_width (page_surface);
		page_height = cairo_image_surface_get_height (page_surface);

		cairo_save (cr);
		cairo_translate (cr, overlap.x, overlap.y);

		if (width != page_width || height != page_height) {
			cairo_pattern_set_filter (cairo_get_source (cr),
						  CAIRO_FILTER_FAST);
			cairo_scale (cr,
				     (gdouble)width / page_width,
				     (gdouble)height / page_height);
		}

		cairo_surface_set_device_offset (page_surface,
						 overlap.x - real_page_area.x,
						 overlap.y - real_page_area.y);
		cairo_set_source_surface (cr, page_surface, 0, 0);
		cairo_paint (cr);
		cairo_restore (cr);
		
		/* Get the selection pixbuf iff we have something to draw */
		if (find_selection_for_page (view, page) &&
		    view->selection_mode == EV_VIEW_SELECTION_TEXT) {
			selection_surface =
				ev_pixbuf_cache_get_selection_surface (view->pixbuf_cache,
								       page,
								       view->scale,
								       NULL);
		}

		if (!selection_surface) {
			return;
		}

		selection_width = cairo_image_surface_get_width (selection_surface);
		selection_height = cairo_image_surface_get_height (selection_surface);

		cairo_save (cr);
		cairo_translate (cr, overlap.x, overlap.y);

		if (width != selection_width || height != selection_height) {
			cairo_pattern_set_filter (cairo_get_source (cr),
						  CAIRO_FILTER_FAST);
			cairo_scale (cr,
				     (gdouble)width / selection_width,
				     (gdouble)height / selection_height);
		}

		cairo_surface_set_device_offset (selection_surface,
						 overlap.x - real_page_area.x,
						 overlap.y - real_page_area.y);

		cairo_set_source_surface (cr, selection_surface, 0, 0);
		cairo_paint (cr);
		cairo_restore (cr);
	}
}

/*** GObject functions ***/

static void
ev_view_finalize (GObject *object)
{
	EvView *view = EV_VIEW (object);

	clear_selection (view);
	clear_link_selected (view);

	if (view->synctex_result) {
		g_free (view->synctex_result);
		view->synctex_result = NULL;
	}

	if (view->image_dnd_info.image)
		g_object_unref (view->image_dnd_info.image);
	view->image_dnd_info.image = NULL;

	G_OBJECT_CLASS (ev_view_parent_class)->finalize (object);
}

static void
ev_view_dispose (GObject *object)
{
	EvView *view = EV_VIEW (object);

	if (view->model) {
		g_signal_handlers_disconnect_by_data (view->model, view);
		g_object_unref (view->model);
		view->model = NULL;
	}

	if (view->pixbuf_cache) {
		g_object_unref (view->pixbuf_cache);
		view->pixbuf_cache = NULL;
	}

	if (view->document) {
		g_object_unref (view->document);
		view->document = NULL;
	}

	if (view->page_cache) {
		g_object_unref (view->page_cache);
		view->page_cache = NULL;
	}

	ev_view_find_cancel (view);

	ev_view_window_children_free (view);

	if (view->selection_scroll_id) {
	    g_source_remove (view->selection_scroll_id);
	    view->selection_scroll_id = 0;
	}

	if (view->selection_update_id) {
	    g_source_remove (view->selection_update_id);
	    view->selection_update_id = 0;
	}

	if (view->scroll_info.timeout_id) {
	    g_source_remove (view->scroll_info.timeout_id);
	    view->scroll_info.timeout_id = 0;
	}

	if (view->drag_info.drag_timeout_id) {
		g_source_remove (view->drag_info.drag_timeout_id);
		view->drag_info.drag_timeout_id = 0;
	}

	if (view->drag_info.release_timeout_id) {
		g_source_remove (view->drag_info.release_timeout_id);
		view->drag_info.release_timeout_id = 0;
	}

	if (view->loading_timeout) {
		g_source_remove (view->loading_timeout);
		view->loading_timeout = 0;
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

	switch (prop_id) {
	case PROP_HADJUSTMENT:
		g_value_set_object (value, view->hadjustment);
		break;
	case PROP_VADJUSTMENT:
		g_value_set_object (value, view->vadjustment);
		break;
	case PROP_HSCROLL_POLICY:
		g_value_set_enum (value, view->hscroll_policy);
		break;
	case PROP_VSCROLL_POLICY:
		g_value_set_enum (value, view->vscroll_policy);
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

	switch (prop_id) {
	case PROP_HADJUSTMENT:
		ev_view_set_scroll_adjustment (view, GTK_ORIENTATION_HORIZONTAL,
					       (GtkAdjustment *) g_value_get_object (value));
		break;
	case PROP_VADJUSTMENT:
		ev_view_set_scroll_adjustment (view, GTK_ORIENTATION_VERTICAL,
					       (GtkAdjustment *) g_value_get_object (value));
		break;
	case PROP_HSCROLL_POLICY:
		view->hscroll_policy = g_value_get_enum (value);
		gtk_widget_queue_resize (GTK_WIDGET (view));
		break;
	case PROP_VSCROLL_POLICY:
		view->vscroll_policy = g_value_get_enum (value);
		gtk_widget_queue_resize (GTK_WIDGET (view));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Accessibility */
static void
ev_view_init_accessibility (EvView *view)
{
	static gboolean first_time = TRUE;

	if (first_time)	{
		AtkObjectFactory *factory;
		AtkRegistry *registry;
		GType derived_type;
		GType derived_atk_type;

		/*
		 * Figure out whether accessibility is enabled by looking at the
		 * type of the accessible object which would be created for
		 * the parent type of EvView.
		 */
		derived_type = g_type_parent (EV_TYPE_VIEW);

		registry = atk_get_default_registry ();
		factory = atk_registry_get_factory (registry,
						    derived_type);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE)) {
			atk_registry_set_factory_type (registry,
						       EV_TYPE_VIEW,
						       ev_view_accessible_factory_get_type ());
			view->a11y_enabled = TRUE;
		}
		first_time = FALSE;
	}
}

static AtkObject *
ev_view_get_accessible (GtkWidget *widget)
{
	ev_view_init_accessibility (EV_VIEW (widget));
	return GTK_WIDGET_CLASS (ev_view_parent_class)->get_accessible (widget);
}

static gboolean
ev_view_is_a11y_enabled (EvView *view)
{
	ev_view_init_accessibility (view);
	return view->a11y_enabled;
}

/* GtkContainer */
static void
ev_view_remove (GtkContainer *container,
		GtkWidget    *widget)
{
	EvView *view = EV_VIEW (container);
	GList *tmp_list = view->children;
	EvViewChild *child;

	while (tmp_list) {
		child = tmp_list->data;

		if (child->widget == widget) {
			gtk_widget_unparent (widget);

			view->children = g_list_remove_link (view->children, tmp_list);
			g_list_free_1 (tmp_list);
			g_slice_free (EvViewChild, child);

			return;
		}

		tmp_list = tmp_list->next;
	}
}

static void
ev_view_forall (GtkContainer *container,
		gboolean      include_internals,
		GtkCallback   callback,
		gpointer      callback_data)
{
	EvView *view = EV_VIEW (container);
	GList *tmp_list = view->children;
	EvViewChild *child;

	while (tmp_list) {
		child = tmp_list->data;
		tmp_list = tmp_list->next;

		(* callback) (child->widget, callback_data);
	}
}

static void
ev_view_class_init (EvViewClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
	GtkBindingSet *binding_set;

	object_class->get_property = ev_view_get_property;
	object_class->set_property = ev_view_set_property;
        object_class->dispose = ev_view_dispose;
	object_class->finalize = ev_view_finalize;

	widget_class->realize = ev_view_realize;
        widget_class->draw = ev_view_draw;
	widget_class->button_press_event = ev_view_button_press_event;
	widget_class->motion_notify_event = ev_view_motion_notify_event;
	widget_class->button_release_event = ev_view_button_release_event;
	widget_class->key_press_event = ev_view_key_press_event;
	widget_class->focus_in_event = ev_view_focus_in;
	widget_class->focus_out_event = ev_view_focus_out;
 	widget_class->get_accessible = ev_view_get_accessible;
	widget_class->get_preferred_width = ev_view_get_preferred_width;
	widget_class->get_preferred_height = ev_view_get_preferred_height;
	widget_class->size_allocate = ev_view_size_allocate;
	widget_class->scroll_event = ev_view_scroll_event;
	widget_class->enter_notify_event = ev_view_enter_notify_event;
	widget_class->leave_notify_event = ev_view_leave_notify_event;
	widget_class->style_updated = ev_view_style_updated;
	widget_class->drag_data_get = ev_view_drag_data_get;
	widget_class->drag_motion = ev_view_drag_motion;
	widget_class->popup_menu = ev_view_popup_menu;
	widget_class->query_tooltip = ev_view_query_tooltip;

	container_class->remove = ev_view_remove;
	container_class->forall = ev_view_forall;

	class->binding_activated = ev_view_scroll;

	/* Scrollable interface */
	g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	signals[SIGNAL_BINDING_ACTIVATED] = g_signal_new ("binding_activated",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, binding_activated),
		         NULL, NULL,
		         ev_view_marshal_VOID__ENUM_BOOLEAN,
		         G_TYPE_NONE, 2,
		         GTK_TYPE_SCROLL_TYPE,
		         G_TYPE_BOOLEAN);
	signals[SIGNAL_HANDLE_LINK] = g_signal_new ("handle-link",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, handle_link),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__OBJECT,
		         G_TYPE_NONE, 1,
			 G_TYPE_OBJECT);
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
		         g_cclosure_marshal_VOID__POINTER,
		         G_TYPE_NONE, 1,
			 G_TYPE_POINTER);
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
		         g_cclosure_marshal_VOID__POINTER,
		         G_TYPE_NONE, 1,
			 G_TYPE_POINTER);
	signals[SIGNAL_ANNOT_ADDED] = g_signal_new ("annot-added",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, annot_added),
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

	binding_set = gtk_binding_set_by_class (class);

        add_scroll_binding_keypad (binding_set, GDK_KEY_Left,  0, GTK_SCROLL_STEP_BACKWARD, TRUE);
        add_scroll_binding_keypad (binding_set, GDK_KEY_Right, 0, GTK_SCROLL_STEP_FORWARD,  TRUE);
        add_scroll_binding_keypad (binding_set, GDK_KEY_Left,  GDK_MOD1_MASK, GTK_SCROLL_STEP_DOWN, TRUE);
        add_scroll_binding_keypad (binding_set, GDK_KEY_Right, GDK_MOD1_MASK, GTK_SCROLL_STEP_UP,  TRUE);
        add_scroll_binding_keypad (binding_set, GDK_KEY_Up,    0, GTK_SCROLL_STEP_BACKWARD, FALSE);
        add_scroll_binding_keypad (binding_set, GDK_KEY_Down,  0, GTK_SCROLL_STEP_FORWARD,  FALSE);
        add_scroll_binding_keypad (binding_set, GDK_KEY_Up,    GDK_MOD1_MASK, GTK_SCROLL_STEP_DOWN, FALSE);
        add_scroll_binding_keypad (binding_set, GDK_KEY_Down,  GDK_MOD1_MASK, GTK_SCROLL_STEP_UP,  FALSE);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_H, 0, "binding_activated", 2, GTK_TYPE_SCROLL_TYPE,
				      GTK_SCROLL_STEP_BACKWARD, G_TYPE_BOOLEAN, TRUE);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_J, 0, "binding_activated", 2, GTK_TYPE_SCROLL_TYPE,
				      GTK_SCROLL_STEP_FORWARD, G_TYPE_BOOLEAN, FALSE);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_K, 0, "binding_activated", 2, GTK_TYPE_SCROLL_TYPE,
				      GTK_SCROLL_STEP_BACKWARD, G_TYPE_BOOLEAN, FALSE);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_L, 0, "binding_activated", 2, GTK_TYPE_SCROLL_TYPE,
				      GTK_SCROLL_STEP_FORWARD, G_TYPE_BOOLEAN, TRUE);
	
}

static void
ev_view_init (EvView *view)
{
	gtk_widget_set_has_window (GTK_WIDGET (view), TRUE);
	gtk_widget_set_can_focus (GTK_WIDGET (view), TRUE);
	gtk_widget_set_redraw_on_allocate (GTK_WIDGET (view), FALSE);
	gtk_container_set_resize_mode (GTK_CONTAINER (view), GTK_RESIZE_QUEUE);

	gtk_widget_set_events (GTK_WIDGET (view),
			       GDK_EXPOSURE_MASK |
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK |
			       GDK_SCROLL_MASK |
			       GDK_KEY_PRESS_MASK |
			       GDK_POINTER_MOTION_MASK |
			       GDK_POINTER_MOTION_HINT_MASK |
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);

	view->start_page = -1;
	view->end_page = -1;
	view->spacing = 5;
	view->scale = 1.0;
	view->current_page = 0;
	view->pressed_button = -1;
	view->cursor = EV_VIEW_CURSOR_NORMAL;
	view->drag_info.in_drag = FALSE;
	view->scroll_info.autoscrolling = FALSE;
	view->selection_info.selections = NULL;
	view->selection_info.in_selection = FALSE;
	view->selection_info.in_drag = FALSE;
	view->selection_mode = EV_VIEW_SELECTION_TEXT;
	view->continuous = TRUE;
	view->dual_page = FALSE;
	view->dual_even_left = TRUE;
	view->fullscreen = FALSE;
	view->sizing_mode = EV_SIZING_FIT_WIDTH;
	view->pending_scroll = SCROLL_TO_KEEP_POSITION;
	view->jump_to_find_result = TRUE;
	view->highlight_find_results = FALSE;
}

/*** Callbacks ***/

static void
ev_view_change_page (EvView *view,
		     gint    new_page)
{
	gint x, y;

	view->current_page = new_page;
	view->pending_scroll = SCROLL_TO_PAGE_POSITION;

	hide_loading_window (view);

	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y);

	gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
job_finished_cb (EvPixbufCache  *pixbuf_cache,
		 cairo_region_t *region,
		 EvView         *view)
{
	if (region) {
		gdk_window_invalidate_region (gtk_widget_get_window (GTK_WIDGET (view)), region, TRUE);
	} else {
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}

static void
ev_view_page_changed_cb (EvDocumentModel *model,
			 gint             old_page,
			 gint             new_page,
			 EvView          *view)
{
	if (!view->document)
		return;

	if (view->current_page != new_page) {
		ev_view_change_page (view, new_page);
	} else {
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}

	view->find_result = 0;
}

static void
on_adjustment_value_changed (GtkAdjustment *adjustment,
			     EvView        *view)
{
	GtkWidget *widget = GTK_WIDGET (view);
	int dx = 0, dy = 0;
	gint x, y;
	gint value;
	GList *l;

	if (!gtk_widget_get_realized (widget))
		return;

	if (view->hadjustment) {
		value = (gint) gtk_adjustment_get_value (view->hadjustment);
		dx = view->scroll_x - value;
		view->scroll_x = value;
	} else {
		view->scroll_x = 0;
	}

	if (view->vadjustment) {
		value = (gint) gtk_adjustment_get_value (view->vadjustment);
		dy = view->scroll_y - value;
		view->scroll_y = value;
	} else {
		view->scroll_y = 0;
	}

	for (l = view->children; l && l->data; l = g_list_next (l)) {
		EvViewChild *child = (EvViewChild *)l->data;

		child->x += dx;
		child->y += dy;
		if (gtk_widget_get_visible (child->widget) && gtk_widget_get_visible (widget))
			gtk_widget_queue_resize (widget);
	}

	for (l = view->window_children; l && l->data; l = g_list_next (l)) {
		EvViewWindowChild *child;

		child = (EvViewWindowChild *)l->data;

		ev_view_window_child_move (view, child, child->x + dx, child->y + dy);
	}

	if (view->pending_resize) {
		gtk_widget_queue_draw (widget);
	} else {
		gdk_window_scroll (gtk_widget_get_window (widget), dx, dy);
	}

	ev_document_misc_get_pointer_position (widget, &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y);

	if (view->document)
		view_update_range_and_current_page (view);
}

GtkWidget*
ev_view_new (void)
{
	GtkWidget *view;

	view = g_object_new (EV_TYPE_VIEW, NULL);

	return view;
}

static void
setup_caches (EvView *view)
{
	gboolean inverted_colors;

	view->height_to_page_cache = ev_view_get_height_to_page_cache (view);
	view->pixbuf_cache = ev_pixbuf_cache_new (GTK_WIDGET (view), view->model, view->pixbuf_cache_size);
	view->page_cache = ev_page_cache_new (view->document);
	if (ev_view_is_a11y_enabled (view)) {
		EvJobPageDataFlags flags = ev_page_cache_get_flags (view->page_cache);

		ev_page_cache_set_flags (view->page_cache,
					 flags |
					 EV_PAGE_DATA_INCLUDE_TEXT_LAYOUT |
					 EV_PAGE_DATA_INCLUDE_TEXT);
	}
	inverted_colors = ev_document_model_get_inverted_colors (view->model);
	ev_pixbuf_cache_set_inverted_colors (view->pixbuf_cache, inverted_colors);
	g_signal_connect (view->pixbuf_cache, "job-finished", G_CALLBACK (job_finished_cb), view);
}

static void
clear_caches (EvView *view)
{
	if (view->pixbuf_cache) {
		g_object_unref (view->pixbuf_cache);
		view->pixbuf_cache = NULL;
	}

	if (view->page_cache) {
		g_object_unref (view->page_cache);
		view->page_cache = NULL;
	}
}

/**
 * ev_view_set_page_cache_size:
 * @view:
 * @cache_size:
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
	if (view->pixbuf_cache_size == cache_size)
		return;

	view->pixbuf_cache_size = cache_size;
	if (view->pixbuf_cache)
		ev_pixbuf_cache_set_max_size (view->pixbuf_cache, cache_size);
}

void
ev_view_set_loading (EvView 	  *view,
		     gboolean      loading)
{
	if (view->loading && !loading)
		hide_loading_window (view);
	view->loading = loading;
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

static gboolean
ev_view_autoscroll_cb (EvView *view)
{
	gdouble speed, value;

	/* If the user stops autoscrolling, autoscrolling will be
	 * set to false but the timeout will continue; stop the timeout: */
	if (!view->scroll_info.autoscrolling) {
		view->scroll_info.timeout_id = 0;
		return FALSE;
	}
	
	if (view->scroll_info.last_y > view->scroll_info.start_y && 
		(view->scroll_info.last_y < view->scroll_info.start_y))
		return TRUE; 

	/* Replace 100 with your speed of choice: The lower the faster.
	 * Replace 3 with another speed of choice: The higher, the faster it accelerated
	 * 	based on the distance of the starting point from the mouse
	 * (All also effected by the timeout interval of this callback) */

	if (view->scroll_info.start_y > view->scroll_info.last_y)
		speed = -pow ((((gdouble)view->scroll_info.start_y - view->scroll_info.last_y) / 100), 3);
	else
		speed = pow ((((gdouble)view->scroll_info.last_y - view->scroll_info.start_y) / 100), 3);
	
	value = gtk_adjustment_get_value (view->vadjustment);
	value = CLAMP (value + speed, 0,
		       gtk_adjustment_get_upper (view->vadjustment) -
		       gtk_adjustment_get_page_size (view->vadjustment));
	gtk_adjustment_set_value (view->vadjustment, value);
	
	return TRUE;

}

void
ev_view_autoscroll_start (EvView *view)
{
	gint x, y;
	
	g_return_if_fail (EV_IS_VIEW (view));

	if (view->scroll_info.autoscrolling)
		return;
	
	view->scroll_info.autoscrolling = TRUE;
	view->scroll_info.timeout_id =
		g_timeout_add (20, (GSourceFunc)ev_view_autoscroll_cb,
			       view);
	
	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y);
}

void
ev_view_autoscroll_stop (EvView *view)
{
	gint x, y;
	
	g_return_if_fail (EV_IS_VIEW (view));

	if (!view->scroll_info.autoscrolling)
		return;

	view->scroll_info.autoscrolling = FALSE;
	if (view->scroll_info.timeout_id) {
		g_source_remove (view->scroll_info.timeout_id);
		view->scroll_info.timeout_id = 0;
	}

	ev_document_misc_get_pointer_position (GTK_WIDGET (view), &x, &y);
	ev_view_handle_cursor_over_xy (view, x, y);
}

static void
ev_view_document_changed_cb (EvDocumentModel *model,
			     GParamSpec      *pspec,
			     EvView          *view)
{
	EvDocument *document = ev_document_model_get_document (model);

	if (document != view->document) {
		gint current_page;

		ev_view_remove_all (view);
		clear_caches (view);

		if (view->document) {
			g_object_unref (view->document);
                }

		view->document = document;
		view->find_result = 0;

		if (view->document) {
			view->loading = FALSE;
			g_object_ref (view->document);
			setup_caches (view);
                }

		current_page = ev_document_model_get_page (model);
		if (view->current_page != current_page) {
			ev_view_change_page (view, current_page);
		} else {
			view->pending_scroll = SCROLL_TO_KEEP_POSITION;
			gtk_widget_queue_resize (GTK_WIDGET (view));
		}
	}
}

static void
ev_view_rotation_changed_cb (EvDocumentModel *model,
			     GParamSpec      *pspec,
			     EvView          *view)
{
	gint rotation = ev_document_model_get_rotation (model);

	view->rotation = rotation;

	if (view->pixbuf_cache) {
		ev_pixbuf_cache_clear (view->pixbuf_cache);
		if (!ev_document_is_page_size_uniform (view->document))
			view->pending_scroll = SCROLL_TO_PAGE_POSITION;
		gtk_widget_queue_resize (GTK_WIDGET (view));
	}

	ev_view_remove_all (view);

	if (rotation != 0)
		clear_selection (view);
}

static void
ev_view_inverted_colors_changed_cb (EvDocumentModel *model,
				    GParamSpec      *pspec,
				    EvView          *view)
{
	if (view->pixbuf_cache) {
		gboolean inverted_colors;

		inverted_colors = ev_document_model_get_inverted_colors (model);
		ev_pixbuf_cache_set_inverted_colors (view->pixbuf_cache, inverted_colors);
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}

static void
ev_view_sizing_mode_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvView          *view)
{
	EvSizingMode mode = ev_document_model_get_sizing_mode (model);

	view->sizing_mode = mode;
	if (mode != EV_SIZING_FREE)
		gtk_widget_queue_resize (GTK_WIDGET (view));
}

#define EPSILON 0.0000001
static void
ev_view_scale_changed_cb (EvDocumentModel *model,
			  GParamSpec      *pspec,
			  EvView          *view)
{
	gdouble scale = ev_document_model_get_scale (model);

	if (ABS (view->scale - scale) < EPSILON)
		return;

	view->scale = scale;

	view->pending_resize = TRUE;
	if (view->sizing_mode == EV_SIZING_FREE)
		gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
ev_view_continuous_changed_cb (EvDocumentModel *model,
			       GParamSpec      *pspec,
			       EvView          *view)
{
	gboolean continuous = ev_document_model_get_continuous (model);

	view->continuous = continuous;
	view->pending_scroll = SCROLL_TO_PAGE_POSITION;
	gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
ev_view_dual_page_changed_cb (EvDocumentModel *model,
			      GParamSpec      *pspec,
			      EvView          *view)
{
	gboolean dual_page = ev_document_model_get_dual_page (model);

	view->dual_page = dual_page || ev_document_model_get_dual_page_odd_pages_left (model);
	view->pending_scroll = SCROLL_TO_PAGE_POSITION;
	/* FIXME: if we're keeping the pixbuf cache around, we should extend the
	 * preload_cache_size to be 2 if dual_page is set.
	 */
	gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
ev_view_dual_odd_left_changed_cb (EvDocumentModel *model,
				  GParamSpec      *pspec,
				  EvView          *view)
{
	gboolean dual_odd_left = ev_document_model_get_dual_page_odd_pages_left (model);

	view->dual_page = dual_odd_left || ev_document_model_get_dual_page (model);
	view->dual_even_left = !dual_odd_left;

	view->pending_scroll = SCROLL_TO_PAGE_POSITION;
	gtk_widget_queue_resize (GTK_WIDGET (view));
}

static void
ev_view_fullscreen_changed_cb (EvDocumentModel *model,
			       GParamSpec      *pspec,
			       EvView          *view)
{
	gboolean fullscreen = ev_document_model_get_fullscreen (model);

	view->fullscreen = fullscreen;
	gtk_widget_queue_resize (GTK_WIDGET (view));
}

void
ev_view_set_model (EvView          *view,
		   EvDocumentModel *model)
{
	g_return_if_fail (EV_IS_VIEW (view));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (model == view->model)
		return;

	if (view->model) {
		g_signal_handlers_disconnect_by_data (view->model, view);
		g_object_unref (view->model);
	}
	view->model = g_object_ref (model);

	/* Initialize view from model */
	view->rotation = ev_document_model_get_rotation (view->model);
	view->sizing_mode = ev_document_model_get_sizing_mode (view->model);
	view->scale = ev_document_model_get_scale (view->model);
	view->continuous = ev_document_model_get_continuous (view->model);
	view->dual_page = ev_document_model_get_dual_page (view->model);
	view->fullscreen = ev_document_model_get_fullscreen (view->model);
	ev_view_document_changed_cb (view->model, NULL, view);

	g_signal_connect (view->model, "notify::document",
			  G_CALLBACK (ev_view_document_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::rotation",
			  G_CALLBACK (ev_view_rotation_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::inverted-colors",
			  G_CALLBACK (ev_view_inverted_colors_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::sizing-mode",
			  G_CALLBACK (ev_view_sizing_mode_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::scale",
			  G_CALLBACK (ev_view_scale_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::continuous",
			  G_CALLBACK (ev_view_continuous_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::dual-page",
			  G_CALLBACK (ev_view_dual_page_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::dual-odd-left",
			  G_CALLBACK (ev_view_dual_odd_left_changed_cb),
			  view);
	g_signal_connect (view->model, "notify::fullscreen",
			  G_CALLBACK (ev_view_fullscreen_changed_cb),
			  view);
	g_signal_connect (view->model, "page-changed",
			  G_CALLBACK (ev_view_page_changed_cb),
			  view);
}

static void
ev_view_reload_page (EvView         *view,
		     gint            page,
		     cairo_region_t *region)
{
	ev_pixbuf_cache_reload_page (view->pixbuf_cache,
				     region,
				     page,
				     view->rotation,
				     view->scale);
}

void
ev_view_reload (EvView *view)
{
	ev_pixbuf_cache_clear (view->pixbuf_cache);
	view_update_range_and_current_page (view);
}

/*** Zoom and sizing mode ***/
static gboolean
ev_view_can_zoom (EvView *view, gdouble factor)
{
	gdouble scale = view->scale * factor;

	if (factor == 1.0)
		return TRUE;

	else if (factor < 1.0)
		return ev_document_model_get_min_scale (view->model) <= scale;

	else
		return scale <= ev_document_model_get_max_scale (view->model);
}

gboolean
ev_view_can_zoom_in (EvView *view)
{
	return ev_view_can_zoom (view, ZOOM_IN_FACTOR);
}

gboolean
ev_view_can_zoom_out (EvView *view)
{
	return ev_view_can_zoom (view, ZOOM_OUT_FACTOR);
}

static void
ev_view_zoom (EvView *view, gdouble factor)
{
	gdouble scale;

	g_return_if_fail (view->sizing_mode == EV_SIZING_FREE);

	view->pending_scroll = SCROLL_TO_CENTER;
	scale = ev_document_model_get_scale (view->model) * factor;
	ev_document_model_set_scale (view->model, scale);
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
zoom_for_size_best_fit (gdouble doc_width,
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

static void
ev_view_zoom_for_size_continuous_and_dual_page (EvView *view,
						int     width,
						int     height)
{
	gdouble doc_width, doc_height;
	GtkBorder border;
	gdouble scale;
	gint sb_size;

	ev_document_get_max_page_size (view->document, &doc_width, &doc_height);
	if (view->rotation == 90 || view->rotation == 270) {
		gdouble tmp;

		tmp = doc_width;
		doc_width = doc_height;
		doc_height = tmp;
	}

	compute_border (view, doc_width, doc_height, &border);

	doc_width *= 2;
	width -= (2 * (border.left + border.right) + 3 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing - 1);

	sb_size = ev_view_get_scrollbar_size (view, GTK_ORIENTATION_VERTICAL);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		scale = zoom_for_size_fit_width (doc_width, doc_height, width - sb_size, height);
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width - sb_size, height);
	else
		g_assert_not_reached ();

	ev_document_model_set_scale (view->model, scale);
}

static void
ev_view_zoom_for_size_continuous (EvView *view,
				  int     width,
				  int     height)
{
	gdouble doc_width, doc_height;
	GtkBorder border;
	gdouble scale;
	gint sb_size;

	ev_document_get_max_page_size (view->document, &doc_width, &doc_height);
	if (view->rotation == 90 || view->rotation == 270) {
		gdouble tmp;

		tmp = doc_width;
		doc_width = doc_height;
		doc_height = tmp;
	}

	compute_border (view, doc_width, doc_height, &border);

	width -= (border.left + border.right + 2 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing - 1);

	sb_size = ev_view_get_scrollbar_size (view, GTK_ORIENTATION_VERTICAL);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		scale = zoom_for_size_fit_width (doc_width, doc_height, width - sb_size, height);
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width - sb_size, height);
	else
		g_assert_not_reached ();

	ev_document_model_set_scale (view->model, scale);
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

	other_page = view->current_page ^ 1;

	/* Find the largest of the two. */
	get_doc_page_size (view, view->current_page, &doc_width, &doc_height);
	if (other_page < ev_document_get_n_pages (view->document)) {
		gdouble width_2, height_2;

		get_doc_page_size (view, other_page, &width_2, &height_2);
		if (width_2 > doc_width)
			doc_width = width_2;
		if (height_2 > doc_height)
			doc_height = height_2;
	}
	compute_border (view, width, height, &border);

	doc_width = doc_width * 2;
	width -= ((border.left + border.right)* 2 + 3 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		gint sb_size;

		sb_size = ev_view_get_scrollbar_size (view, GTK_ORIENTATION_VERTICAL);
		scale = zoom_for_size_fit_width (doc_width, doc_height, width - sb_size, height);
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width, height);
	else
		g_assert_not_reached ();

	ev_document_model_set_scale (view->model, scale);
}

static void
ev_view_zoom_for_size_single_page (EvView *view,
				   int     width,
				   int     height)
{
	gdouble doc_width, doc_height;
	GtkBorder border;
	gdouble scale;

	get_doc_page_size (view, view->current_page, &doc_width, &doc_height);

	/* Get an approximate border */
	compute_border (view, width, height, &border);

	width -= (border.left + border.right + 2 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		gint sb_size;

		sb_size = ev_view_get_scrollbar_size (view, GTK_ORIENTATION_VERTICAL);
		scale = zoom_for_size_fit_width (doc_width, doc_height, width - sb_size, height);
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width, height);
	else
		g_assert_not_reached ();

	ev_document_model_set_scale (view->model, scale);
}

static void
ev_view_zoom_for_size (EvView *view,
		       int     width,
		       int     height)
{
	g_return_if_fail (EV_IS_VIEW (view));
	g_return_if_fail (view->sizing_mode == EV_SIZING_FIT_WIDTH ||
			  view->sizing_mode == EV_SIZING_BEST_FIT);
	g_return_if_fail (width >= 0);
	g_return_if_fail (height >= 0);

	if (view->document == NULL)
		return;

	if (view->continuous && view->dual_page)
		ev_view_zoom_for_size_continuous_and_dual_page (view, width, height);
	else if (view->continuous)
		ev_view_zoom_for_size_continuous (view, width, height);
	else if (view->dual_page)
		ev_view_zoom_for_size_dual_page (view, width, height);
	else
		ev_view_zoom_for_size_single_page (view, width, height);
}

/*** Find ***/
static gint
ev_view_find_get_n_results (EvView *view, gint page)
{
	return view->find_pages ? g_list_length (view->find_pages[page]) : 0;
}

static EvRectangle *
ev_view_find_get_result (EvView *view, gint page, gint result)
{
	return view->find_pages ? (EvRectangle *) g_list_nth_data (view->find_pages[page], result) : NULL;
}

static void
jump_to_find_result (EvView *view)
{
	gint n_results;
	gint page = view->current_page;

	n_results = ev_view_find_get_n_results (view, page);

	if (n_results > 0 && view->find_result < n_results) {
		EvRectangle *rect;
		GdkRectangle view_rect;

		rect = ev_view_find_get_result (view, page, view->find_result);
		doc_rect_to_view_rect (view, page, rect, &view_rect);
		ensure_rectangle_is_visible (view, &view_rect);
		view->jump_to_find_result = FALSE;
	}
}

/**
 * jump_to_find_page
 * @view: #EvView instance
 * @direction: Direction to look
 * @shift: Shift from current page
 *
 * Jumps to the first page that has occurences of searched word.
 * Uses a direction where to look and a shift from current page. 
 *
 */
static void
jump_to_find_page (EvView *view, EvViewFindDirection direction, gint shift)
{
	int n_pages, i;

	n_pages = ev_document_get_n_pages (view->document);

	for (i = 0; i < n_pages; i++) {
		int page;
		
		if (direction == EV_VIEW_FIND_NEXT)
			page = view->current_page + i;
		else
			page = view->current_page - i;		
		page += shift;
		
		if (page >= n_pages) {
			page = page - n_pages;
		} else if (page < 0) 
			page = page + n_pages;

		if (ev_view_find_get_n_results (view, page) > 0) {
			ev_document_model_set_page (view->model, page);
			break;
		}
	}
}

static void
find_job_updated_cb (EvJobFind *job, gint page, EvView *view)
{
	ev_view_find_changed (view, ev_job_find_get_results (job), page);
}

void
ev_view_find_started (EvView *view, EvJobFind *job)
{
	if (view->find_job == job)
		return;

	ev_view_find_cancel (view);
	view->find_job = g_object_ref (job);

	g_signal_connect (job, "updated", G_CALLBACK (find_job_updated_cb), view);
}

void
ev_view_find_changed (EvView *view, GList **results, gint page)
{
	view->find_pages = results;
	
	if (view->jump_to_find_result == TRUE) {
		jump_to_find_page (view, EV_VIEW_FIND_NEXT, 0);
		jump_to_find_result (view);
	}

	if (view->current_page == page)
		gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_find_next (EvView *view)
{
	gint n_results;

	n_results = ev_view_find_get_n_results (view, view->current_page);
	view->find_result++;

	if (view->find_result >= n_results) {
		view->find_result = 0;
		jump_to_find_page (view, EV_VIEW_FIND_NEXT, 1);
		jump_to_find_result (view);
	} else {
		jump_to_find_result (view);
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}

void
ev_view_find_previous (EvView *view)
{
	view->find_result--;

	if (view->find_result < 0) {
		jump_to_find_page (view, EV_VIEW_FIND_PREV, -1);
		view->find_result = MAX (0, ev_view_find_get_n_results (view, view->current_page) - 1);
		jump_to_find_result (view);
	} else {
		jump_to_find_result (view);
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}

void
ev_view_find_search_changed (EvView *view)
{
	/* search string has changed, focus on new search result */
	view->jump_to_find_result = TRUE;
	ev_view_find_cancel (view);
}

void
ev_view_find_set_highlight_search (EvView *view, gboolean value)
{
	view->highlight_find_results = value;
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_find_cancel (EvView *view)
{
	view->find_pages = NULL;

	if (!view->find_job)
		return;

	g_signal_handlers_disconnect_by_func (view->find_job, find_job_updated_cb, view);
	g_object_unref (view->find_job);
	view->find_job = NULL;
}

/*** Synctex ***/
void
ev_view_highlight_forward_search (EvView       *view,
				  EvSourceLink *link)
{
	EvMapping   *mapping;
	gint         page;
	GdkRectangle view_rect;

	if (!ev_document_has_synctex (view->document))
		return;

	mapping = ev_document_synctex_forward_search (view->document, link);
	if (!mapping)
		return;

	if (view->synctex_result)
		g_free (view->synctex_result);
	view->synctex_result = mapping;

	page = GPOINTER_TO_INT (mapping->data);
	ev_document_model_set_page (view->model, page);

	doc_rect_to_view_rect (view, page, &mapping->area, &view_rect);
	ensure_rectangle_is_visible (view, &view_rect);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

/*** Selections ***/

/* compute_new_selection_rect/text calculates the area currently selected by
 * view_rect.  each handles a different mode;
 */
static GList *
compute_new_selection_rect (EvView       *view,
			    GdkPoint     *start,
			    GdkPoint     *stop)
{
	GdkRectangle view_rect;
	int n_pages, i;
	GList *list = NULL;

	g_assert (view->selection_mode == EV_VIEW_SELECTION_RECTANGLE);
	
	view_rect.x = MIN (start->x, stop->x);
	view_rect.y = MIN (start->y, stop->y);
	view_rect.width = MAX (start->x, stop->x) - view_rect.x;
	view_rect.width = MAX (start->y, stop->y) - view_rect.y;

	n_pages = ev_document_get_n_pages (view->document);

	for (i = 0; i < n_pages; i++) {
		GdkRectangle page_area;
		GtkBorder border;
		
		if (ev_view_get_page_extents (view, i, &page_area, &border)) {
			GdkRectangle overlap;

			if (gdk_rectangle_intersect (&page_area, &view_rect, &overlap)) {
				EvViewSelection *selection;

				selection = g_new0 (EvViewSelection, 1);
				selection->page = i;
				view_rect_to_doc_rect (view, &overlap, &page_area,
						       &(selection->rect));

				list = g_list_append (list, selection);
			}
		}
	}

	return list;
}

static gboolean
gdk_rectangle_point_in (GdkRectangle *rectangle,
			GdkPoint     *point)
{
	return rectangle->x <= point->x &&
		rectangle->y <= point->y &&
		point->x < rectangle->x + rectangle->width &&
		point->y < rectangle->y + rectangle->height;
}

static GList *
compute_new_selection_text (EvView          *view,
			    EvSelectionStyle style,
			    GdkPoint        *start,
			    GdkPoint        *stop)
{
	int n_pages, i, first, last;
	GList *list = NULL;
	EvViewSelection *selection;
	gdouble width, height;
	int start_page, end_page;

	g_assert (view->selection_mode == EV_VIEW_SELECTION_TEXT);

	n_pages = ev_document_get_n_pages (view->document);

	/* First figure out the range of pages the selection
	 * affects. */
	first = n_pages;
	last = 0;
	if (view->continuous) {
		start_page = 0;
		end_page = n_pages;
	} else if (view->dual_page) {
		start_page = view->start_page;
		end_page = view->end_page + 1;
	} else {
		start_page = view->current_page;
		end_page = view->current_page + 1;
	}

	for (i = start_page; i < end_page; i++) {
		GdkRectangle page_area;
		GtkBorder border;
		
		ev_view_get_page_extents (view, i, &page_area, &border);
		if (gdk_rectangle_point_in (&page_area, start) || 
		    gdk_rectangle_point_in (&page_area, stop)) {
			if (first == n_pages)
				first = i;
			last = i;
		}

	}

	/* Now create a list of EvViewSelection's for the affected
	 * pages.  This could be an empty list, a list of just one
	 * page or a number of pages.*/
	for (i = first; i < last + 1; i++) {
		GdkRectangle page_area;
		GtkBorder border;
		GdkPoint *point;

		get_doc_page_size (view, i, &width, &height);

		selection = g_new0 (EvViewSelection, 1);
		selection->page = i;
		selection->style = style;
		selection->rect.x1 = selection->rect.y1 = 0;
		selection->rect.x2 = width;
		selection->rect.y2 = height;

		ev_view_get_page_extents (view, i, &page_area, &border);

		if (gdk_rectangle_point_in (&page_area, start))
			point = start;
		else
			point = stop;

		if (i == first)
			view_point_to_doc_point (view, point, &page_area,
						 &selection->rect.x1,
						 &selection->rect.y1);

		/* If the selection is contained within just one page,
		 * make sure we don't write 'start' into both points
		 * in selection->rect. */
		if (first == last)
			point = stop;

		if (i == last)
			view_point_to_doc_point (view, point, &page_area,
						 &selection->rect.x2,
						 &selection->rect.y2);

		list = g_list_append (list, selection);
	}

	return list;
}

/* This function takes the newly calculated list, and figures out which regions
 * have changed.  It then queues a redraw approporiately.
 */
static void
merge_selection_region (EvView *view,
			GList  *new_list)
{
	GList *old_list;
	GList *new_list_ptr, *old_list_ptr;

	/* Update the selection */
	old_list = ev_pixbuf_cache_get_selection_list (view->pixbuf_cache);
	g_list_foreach (view->selection_info.selections, (GFunc)selection_free, NULL);
	g_list_free (view->selection_info.selections);
	view->selection_info.selections = new_list;
	ev_pixbuf_cache_set_selection_list (view->pixbuf_cache, new_list);
	g_signal_emit (view, signals[SIGNAL_SELECTION_CHANGED], 0, NULL);

	new_list_ptr = new_list;
	old_list_ptr = old_list;

	while (new_list_ptr || old_list_ptr) {
		EvViewSelection *old_sel, *new_sel;
		int cur_page;
		cairo_region_t *region = NULL;

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
		if (cur_page < view->start_page || cur_page > view->end_page)
			continue;

		/* seed the cache with a new page.  We are going to need the new
		 * region too. */
		if (new_sel) {
			cairo_region_t *tmp_region = NULL;

			ev_pixbuf_cache_get_selection_surface (view->pixbuf_cache,
							       cur_page,
							       view->scale,
							       &tmp_region);

			if (tmp_region && !cairo_region_is_empty (tmp_region)) {
				new_sel->covered_region = cairo_region_reference (tmp_region);
			}
		}

		/* Now we figure out what needs redrawing */
		if (old_sel && new_sel) {
			if (old_sel->covered_region && new_sel->covered_region) {
				/* We only want to redraw the areas that have
				 * changed, so we xor the old and new regions
				 * and redraw if it's different */
				region = cairo_region_copy (old_sel->covered_region);
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 9, 12)
				cairo_region_xor (region, new_sel->covered_region);
#else
				cairo_region_t *tbr;
				tbr = cairo_region_copy (new_sel->covered_region);

				/* xor old_sel, new_sel*/
				cairo_region_subtract (tbr, region);
				cairo_region_subtract (region, new_sel->covered_region);
				cairo_region_union (region, tbr);
				cairo_region_destroy (tbr);
#endif

				if (cairo_region_is_empty (region)) {
					cairo_region_destroy (region);
					region = NULL;
				} else {
					gint num_rectangles = cairo_region_num_rectangles (region);
					GdkRectangle r;

					/* We need to make the damage region a little bigger
					 * because the edges of the old selection might change
					 */
					cairo_region_get_rectangle (region, 0, &r);
					r.x -= 5;
					r.width = 5;
					cairo_region_union_rectangle (region, &r);

					cairo_region_get_rectangle (region, num_rectangles - 1, &r);
					r.x += r.width;
					r.width = 5;
					cairo_region_union_rectangle (region, &r);
				}
			} else if (old_sel->covered_region) {
				region = cairo_region_copy (old_sel->covered_region);
			} else if (new_sel->covered_region) {
				region = cairo_region_copy (new_sel->covered_region);
			}
		} else if (old_sel && !new_sel) {
			if (old_sel->covered_region && !cairo_region_is_empty (old_sel->covered_region)) {
				region = cairo_region_copy (old_sel->covered_region);
			}
		} else if (!old_sel && new_sel) {
			if (new_sel->covered_region && !cairo_region_is_empty (new_sel->covered_region)) {
				region = cairo_region_copy (new_sel->covered_region);
			}
		} else {
			g_assert_not_reached ();
		}

		/* Redraw the damaged region! */
		if (region) {
			GdkRectangle page_area;
			GtkBorder    border;

			ev_view_get_page_extents (view, cur_page, &page_area, &border);
			cairo_region_translate (region,
					   page_area.x + border.left - view->scroll_x,
					   page_area.y + border.top - view->scroll_y);
			gdk_window_invalidate_region (gtk_widget_get_window (GTK_WIDGET (view)), region, TRUE);
			cairo_region_destroy (region);
		}
	}

	/* Free the old list, now that we're done with it. */
	g_list_foreach (old_list, (GFunc) selection_free, NULL);
	g_list_free (old_list);
}

static void
compute_selections (EvView          *view,
		    EvSelectionStyle style,
		    GdkPoint        *start,
		    GdkPoint        *stop)
{
	GList *list;

	if (view->selection_mode == EV_VIEW_SELECTION_RECTANGLE)
		list = compute_new_selection_rect (view, start, stop);
	else
		list = compute_new_selection_text (view, style, start, stop);
	merge_selection_region (view, list);
}

/* Free's the selection.  It's up to the caller to queue redraws if needed.
 */
static void
selection_free (EvViewSelection *selection)
{
	if (selection->covered_region)
		cairo_region_destroy (selection->covered_region);
	g_free (selection);
}

static void
clear_selection (EvView *view)
{
	if (view->selection_info.selections) {
		g_list_foreach (view->selection_info.selections, (GFunc)selection_free, NULL);
		g_list_free (view->selection_info.selections);
		view->selection_info.selections = NULL;

		g_signal_emit (view, signals[SIGNAL_SELECTION_CHANGED], 0, NULL);
	}
	view->selection_info.in_selection = FALSE;
	if (view->pixbuf_cache)
		ev_pixbuf_cache_set_selection_list (view->pixbuf_cache, NULL);
}

void
ev_view_select_all (EvView *view)
{
	GList *selections = NULL;
	int n_pages, i;

	/* Disable selection on rotated pages for the 0.4.0 series */
	if (view->rotation != 0)
		return;

	clear_selection (view);
	
	n_pages = ev_document_get_n_pages (view->document);
	for (i = 0; i < n_pages; i++) {
		gdouble width, height;
		EvViewSelection *selection;

		get_doc_page_size (view, i, &width, &height);

		selection = g_new0 (EvViewSelection, 1);
		selection->page = i;
		selection->style = EV_SELECTION_STYLE_GLYPH;
		selection->rect.x1 = selection->rect.y1 = 0;
		selection->rect.x2 = width;
		selection->rect.y2 = height;

		selections = g_list_append (selections, selection);
	}

	merge_selection_region (view, selections);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

gboolean
ev_view_get_has_selection (EvView *view)
{
	return view->selection_info.selections != NULL;
}

static char *
get_selected_text (EvView *view)
{
	GString *text;
	GList *l;
	gchar *normalized_text;

	text = g_string_new (NULL);

	ev_document_doc_mutex_lock ();

	for (l = view->selection_info.selections; l != NULL; l = l->next) {
		EvViewSelection *selection = (EvViewSelection *)l->data;
		EvPage *page;
		gchar *tmp;

		page = ev_document_get_page (view->document, selection->page);
		tmp = ev_selection_get_selected_text (EV_SELECTION (view->document),
						      page, selection->style,
						      &(selection->rect));
		g_object_unref (page);
		g_string_append (text, tmp);
		g_free (tmp);
	}

	ev_document_doc_mutex_unlock ();
	
	normalized_text = g_utf8_normalize (text->str, text->len, G_NORMALIZE_NFKC);
	g_string_free (text, TRUE);
	return normalized_text;
}

static void
ev_view_clipboard_copy (EvView      *view,
			const gchar *text)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view),
					      GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, text, -1);
}

void
ev_view_copy (EvView *ev_view)
{
	char *text;

	if (!EV_IS_SELECTION (ev_view->document))
		return;

	text = get_selected_text (ev_view);
	ev_view_clipboard_copy (ev_view, text);
	g_free (text);
}

static void
ev_view_primary_get_cb (GtkClipboard     *clipboard,
			GtkSelectionData *selection_data,
			guint             info,
			gpointer          data)
{
	EvView *ev_view = EV_VIEW (data);

	if (ev_view->link_selected) {
		gtk_selection_data_set_text (selection_data,
					     ev_link_action_get_uri (ev_view->link_selected),
					     -1);
	} else if (EV_IS_SELECTION (ev_view->document) &&
		   ev_view->selection_info.selections) {
		gchar *text;
		
		text = get_selected_text (ev_view);
		if (text) {
			gtk_selection_data_set_text (selection_data, text, -1);
			g_free (text);
		}
	}
}

static void
ev_view_primary_clear_cb (GtkClipboard *clipboard,
			  gpointer      data)
{
	EvView *view = EV_VIEW (data);

	clear_selection (view);
	clear_link_selected (view);
}

static void
ev_view_update_primary_selection (EvView *ev_view)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (ev_view),
                                              GDK_SELECTION_PRIMARY);

	if (ev_view->selection_info.selections || ev_view->link_selected) {
                GtkTargetList *target_list;
                GtkTargetEntry *targets;
                int n_targets;

                target_list = gtk_target_list_new (NULL, 0);
                gtk_target_list_add_text_targets (target_list, 0);
                targets = gtk_target_table_new_from_list (target_list, &n_targets);
                gtk_target_list_unref (target_list);
                
		if (!gtk_clipboard_set_with_owner (clipboard,
						   targets, n_targets,
						   ev_view_primary_get_cb,
						   ev_view_primary_clear_cb,
						   G_OBJECT (ev_view)))
			ev_view_primary_clear_cb (clipboard, ev_view);

                gtk_target_table_free (targets, n_targets);
	} else {
		if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (ev_view))
			gtk_clipboard_clear (clipboard);
	}
}

static void
clear_link_selected (EvView *view)
{
	if (view->link_selected) {
		g_object_unref (view->link_selected);
		view->link_selected = NULL;
	}
}

void
ev_view_copy_link_address (EvView       *view,
			   EvLinkAction *action)
{
	clear_link_selected (view);
	
	ev_view_clipboard_copy (view, ev_link_action_get_uri (action));
	
	view->link_selected = g_object_ref (action);
	ev_view_update_primary_selection (view);
}

/*** Cursor operations ***/
static void
ev_view_set_cursor (EvView *view, EvViewCursor new_cursor)
{
	GdkCursor *cursor = NULL;
	GtkWidget *widget;
	GdkWindow *window;

	if (view->cursor == new_cursor) {
		return;
	}

	view->cursor = new_cursor;

	window = gtk_widget_get_window (GTK_WIDGET (view));
	widget = gtk_widget_get_toplevel (GTK_WIDGET (view));
	cursor = ev_view_cursor_new (gtk_widget_get_display (widget), new_cursor);
	gdk_window_set_cursor (window, cursor);
	gdk_flush ();
	if (cursor)
		g_object_unref (cursor);
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
	int page, n_pages;

	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);
	
	if (!view->document)
		return FALSE;

	page = ev_document_model_get_page (view->model);
	n_pages = ev_document_get_n_pages (view->document);

	if (view->dual_page)
	        page = page + 2; 
	else 
		page = page + 1;

	if (page < n_pages) {
		ev_document_model_set_page (view->model, page);
		return TRUE;
	} else if (view->dual_page && page == n_pages) {
		ev_document_model_set_page (view->model, page - 1);
		return TRUE;
	} else {
		return FALSE;
	}
}

gboolean
ev_view_previous_page (EvView *view)
{
	int page;

	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

	if (!view->document)
		return FALSE;

	page = ev_document_model_get_page (view->model);

	if (view->dual_page)
	        page = page - 2; 
	else 
		page = page - 1;

	if (page >= 0) {
		ev_document_model_set_page (view->model, page);
		return TRUE;
	} else if (view->dual_page && page == -1) {
		ev_document_model_set_page (view->model, 0);
		return TRUE;
	} else {	
		return FALSE;
	}
}
		
