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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <math.h>
#include <gtk/gtkalignment.h>
#include <glib/gi18n.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkselection.h>
#include <gtk/gtkclipboard.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "ev-marshal.h"
#include "ev-view.h"
#include "ev-utils.h"
#include "ev-selection.h"
#include "ev-document-find.h"
#include "ev-document-misc.h"
#include "ev-debug.h"
#include "ev-job-queue.h"
#include "ev-page-cache.h"
#include "ev-pixbuf-cache.h"
#include "ev-tooltip.h"

#define EV_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_VIEW, EvViewClass))
#define EV_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_VIEW))
#define EV_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_VIEW, EvViewClass))


enum {
	PROP_0,
	PROP_STATUS,
	PROP_FIND_STATUS,
	PROP_CONTINUOUS,
	PROP_DUAL_PAGE,
	PROP_FULLSCREEN,
	PROP_PRESENTATION,
	PROP_SIZING_MODE,
	PROP_ZOOM,
	PROP_ROTATION,
};

enum {
	SIGNAL_BINDING_ACTIVATED,
	SIGNAL_ZOOM_INVALID,
	N_SIGNALS,
};

enum {
	TARGET_STRING,
	TARGET_TEXT,
	TARGET_COMPOUND_TEXT,
	TARGET_UTF8_STRING,
	TARGET_TEXT_BUFFER_CONTENTS
};

static const GtkTargetEntry targets[] = {
	{ "STRING", 0, TARGET_STRING },
	{ "TEXT",   0, TARGET_TEXT },
	{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
	{ "UTF8_STRING", 0, TARGET_UTF8_STRING },
};

static guint signals[N_SIGNALS];

typedef enum {
	EV_VIEW_CURSOR_NORMAL,
	EV_VIEW_CURSOR_IBEAM,
	EV_VIEW_CURSOR_LINK,
	EV_VIEW_CURSOR_WAIT,
	EV_VIEW_CURSOR_HIDDEN,
	EV_VIEW_CURSOR_DRAG
} EvViewCursor;

typedef enum {
	EV_VIEW_FIND_NEXT,
	EV_VIEW_FIND_PREV
} EvViewFindDirection;

#define ZOOM_IN_FACTOR  1.2
#define ZOOM_OUT_FACTOR (1.0/ZOOM_IN_FACTOR)

#define MIN_SCALE 0.05409
#define MAX_SCALE 4.0

/* Information for middle clicking and moving around the doc */
typedef struct {
        gboolean in_drag;
	GdkPoint start;
	gdouble hadj;
	gdouble vadj;
} DragInfo;

/* Information for handling selection */
typedef struct {
	gboolean in_selection;
	GdkPoint start;
	GList *selections;
} SelectionInfo;

typedef enum {
	SCROLL_TO_KEEP_POSITION,
	SCROLL_TO_CURRENT_PAGE,
	SCROLL_TO_CENTER
} PendingScroll;

struct _EvView {
	GtkWidget parent_instance;

	EvDocument *document;

	char *status;
	char *find_status;

	/* Scrolling */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

	gint scroll_x;
	gint scroll_y;

	/* Information for middle clicking and dragging around. */
	DragInfo drag_info;

	/* Selection */
	gint motion_x;
	gint motion_y;
	guint selection_update_id;

	EvViewSelectionMode selection_mode;
	SelectionInfo selection_info;

	int pressed_button;
	EvViewCursor cursor;
	GtkWidget *link_tooltip;

	EvPageCache *page_cache;
	EvPixbufCache *pixbuf_cache;

	gint start_page;
	gint end_page;
	gint current_page;

	EvJobRender *current_job;

	int find_page;
	int find_result;
	int spacing;

	int rotation;
	double scale;

	gboolean continuous;
	gboolean dual_page;
	gboolean fullscreen;
	gboolean presentation;
	EvSizingMode sizing_mode;

	PendingScroll pending_scroll;
	gboolean pending_resize;
};

struct _EvViewClass {
	GtkWidgetClass parent_class;

	void	(*set_scroll_adjustments) (EvView         *view,
					   GtkAdjustment  *hadjustment,
					   GtkAdjustment  *vadjustment);
	void    (*binding_activated)	  (EvView         *view,
					   GtkScrollType   scroll,
					   gboolean        horizontal);
	void    (*zoom_invalid)		  (EvView         *view);
};

/*** Scrolling ***/
static void       scroll_to_current_page 		     (EvView *view,
							      GtkOrientation orientation);
static void       ev_view_set_scroll_adjustments             (EvView             *view,
							      GtkAdjustment      *hadjustment,
							      GtkAdjustment      *vadjustment);
static void       view_update_range_and_current_page         (EvView             *view);
static void       set_scroll_adjustment                      (EvView             *view,
							      GtkOrientation      orientation,
							      GtkAdjustment      *adjustment);
static void       ev_view_set_scroll_adjustments             (EvView             *view,
							      GtkAdjustment      *hadjustment,
							      GtkAdjustment      *vadjustment);
static void       add_scroll_binding_keypad                  (GtkBindingSet      *binding_set,
							      guint               keyval,
							      GtkScrollType       scroll,
							      gboolean            horizontal);
static void       ev_view_binding_activated                  (EvView             *view,
							      GtkScrollType       scroll,
							      gboolean            horizontal);
static void       ensure_rectangle_is_visible                (EvView             *view,
							      GdkRectangle       *rect);

/*** Geometry computations ***/
static void       compute_border                             (EvView             *view,
							      int                 width,
							      int                 height,
							      GtkBorder          *border);
static void       get_page_y_offset                          (EvView *view,
							      int page,
							      double zoom,
							      int *y_offset);
static gboolean   get_page_extents                           (EvView             *view,
							      gint                page,
							      GdkRectangle       *page_area,
							      GtkBorder          *border);
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

/*** Hyperrefs ***/
static EvLink*    get_link_at_location                       (EvView             *view,
							      gdouble             x,
							      gdouble             y);
static void       go_to_link                                 (EvView             *view,
							      EvLink             *link);
static char*      tip_from_link                              (EvView             *view,
							      EvLink             *link);

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
static void       ev_view_realize                            (GtkWidget          *widget);
static void       ev_view_unrealize                          (GtkWidget          *widget);
static gboolean   ev_view_scroll_event                       (GtkWidget          *widget,
							      GdkEventScroll     *event);
static gboolean   ev_view_expose_event                       (GtkWidget          *widget,
							      GdkEventExpose     *event);
static gboolean   ev_view_button_press_event                 (GtkWidget          *widget,
							      GdkEventButton     *event);
static gboolean   ev_view_motion_notify_event                (GtkWidget          *widget,
							      GdkEventMotion     *event);
static gboolean   ev_view_button_release_event               (GtkWidget          *widget,
							      GdkEventButton     *event);
static gboolean   ev_view_leave_notify_event                 (GtkWidget          *widget,
							      GdkEventCrossing   *event);
static void       ev_view_style_set                          (GtkWidget          *widget,
							      GtkStyle           *old_style);

/*** Drawing ***/
static guint32    ev_gdk_color_to_rgb                        (const GdkColor     *color);
static void       draw_rubberband                            (GtkWidget          *widget,
							      GdkWindow          *window,
							      const GdkRectangle *rect,
							      guchar              alpha);
static void       highlight_find_results                     (EvView             *view,
							      int                 page);
static void       draw_one_page                              (EvView             *view,
							      gint                page,
							      GdkRectangle       *page_area,
							      GtkBorder          *border,
							      GdkRectangle       *expose_area);

/*** Callbacks ***/
static void       find_changed_cb                            (EvDocument         *document,
							      int                 page,
							      EvView             *view);
static void       job_finished_cb                            (EvPixbufCache      *pixbuf_cache,
							      EvView             *view);
static void       page_changed_cb                            (EvPageCache        *page_cache,
							      int                 new_page,
							      EvView             *view);
static void       on_adjustment_value_changed                (GtkAdjustment      *adjustment,
							      EvView             *view);

/*** GObject ***/
static void       ev_view_finalize                           (GObject            *object);
static void       ev_view_destroy                            (GtkObject          *object);
static void       ev_view_set_property                       (GObject            *object,
							      guint               prop_id,
							      const GValue       *value,
							      GParamSpec         *pspec);
static void       ev_view_get_property                       (GObject            *object,
							      guint               prop_id,
							      GValue             *value,
							      GParamSpec         *pspec);
static void       ev_view_class_init                         (EvViewClass        *class);
static void       ev_view_init                               (EvView             *view);

/*** Zoom and sizing ***/
static double   zoom_for_size_fit_width	 		     (int doc_width,
							      int doc_height,
	    						      int target_width,
							      int target_height,
							      int vsb_width);
static double	zoom_for_size_best_fit 			     (int doc_width,
							      int doc_height,
							      int target_width,
							      int target_height,
							      int vsb_width,
							      int hsb_width);
static void	ev_view_zoom_for_size_presentation	     (EvView *view,
							      int     width,
					    		      int     height);
static void	ev_view_zoom_for_size_continuous_and_dual_page (EvView *view,
							        int     width,
						     	        int     height,
							        int     vsb_width,
							        int     hsb_height);
static void	ev_view_zoom_for_size_continuous	       (EvView *view,
					    		        int     width,
								int     height,
				    				int     vsb_width,
								int     hsb_height);
static void 	ev_view_zoom_for_size_dual_page 	       (EvView *view,
						    		int     width,
								int     height,
								int     vsb_width,
								int     hsb_height);
static void	ev_view_zoom_for_size_single_page 	       (EvView *view,
				    			        int     width,
					    			int     height,
								int     vsb_width,
								int     hsb_height);
/*** Cursors ***/
static GdkCursor* ev_view_create_invisible_cursor            (void);
static void       ev_view_set_cursor                         (EvView             *view,
							      EvViewCursor        new_cursor);

/*** Status messages ***/
static void       ev_view_set_status                         (EvView             *view,
							      const char         *message);
static void       update_find_status_message                 (EvView             *view);
static void       ev_view_set_find_status                    (EvView             *view,
							      const char         *message);
/*** Find ***/
static void       jump_to_find_result                        (EvView             *view);
static void       jump_to_find_page                          (EvView             *view, 
							      EvViewFindDirection direction);

/*** Selection ***/
static void       compute_selections                         (EvView             *view,
							      GdkPoint           *start,
							      GdkPoint           *stop);
static void       clear_selection                            (EvView             *view);
static void       selection_free                             (EvViewSelection    *selection);
static char*      get_selected_text                          (EvView             *ev_view);
static void       ev_view_primary_get_cb                     (GtkClipboard       *clipboard,
							      GtkSelectionData   *selection_data,
							      guint               info,
							      gpointer            data);
static void       ev_view_primary_clear_cb                   (GtkClipboard       *clipboard,
							      gpointer            data);
static void       ev_view_update_primary_selection           (EvView             *ev_view);


G_DEFINE_TYPE (EvView, ev_view, GTK_TYPE_WIDGET)

static void
scroll_to_current_page (EvView *view, GtkOrientation orientation)
{
	GdkRectangle page_area;
	GtkBorder border;

	if (view->document == NULL) {
		return;
	}

	get_page_extents (view, view->current_page, &page_area, &border);

	if (orientation == GTK_ORIENTATION_VERTICAL) {
		if (view->continuous) {
    			gtk_adjustment_clamp_page (view->vadjustment,
						   page_area.y - view->spacing,
						   page_area.y + view->vadjustment->page_size);
		} else {
			gtk_adjustment_set_value (view->vadjustment,
		    				  view->vadjustment->lower);
		}
	} else {
		if (view->dual_page) {
			gtk_adjustment_clamp_page (view->hadjustment,
						   page_area.x,
						   page_area.x + view->hadjustment->page_size);
		} else {
			gtk_adjustment_set_value (view->hadjustment,
						  CLAMP (view->hadjustment->value,
						  view->hadjustment->lower,
						  view->hadjustment->upper -
						  view->hadjustment->page_size));
		}
	}
}

static void
view_set_adjustment_values (EvView         *view,
			    GtkOrientation  orientation)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkAdjustment *adjustment;
	int requisition;
	int allocation;

	double factor;
	gint new_value;

	if (orientation == GTK_ORIENTATION_HORIZONTAL)  {
		requisition = widget->requisition.width;
		allocation = widget->allocation.width;
		adjustment = view->hadjustment;
	} else {
		requisition = widget->requisition.height;
		allocation = widget->allocation.height;
		adjustment = view->vadjustment;
	}

	if (!adjustment)
		return;

	factor = 1.0;
	switch (view->pending_scroll) {
    	        case SCROLL_TO_KEEP_POSITION:
			factor = (adjustment->value) / adjustment->upper;
			break;
    	        case SCROLL_TO_CURRENT_PAGE:
			break;
    	        case SCROLL_TO_CENTER:
			factor = (adjustment->value + adjustment->page_size * 0.5) / adjustment->upper;
			break;
	}

	adjustment->page_size = allocation;
	adjustment->step_increment = allocation * 0.1;
	adjustment->page_increment = allocation * 0.9;
	adjustment->lower = 0;
	adjustment->upper = MAX (allocation, requisition);

	/*
	 * We add 0.5 to the values before to average out our rounding errors.
	 */
	switch (view->pending_scroll) {
    	        case SCROLL_TO_KEEP_POSITION:
			new_value = CLAMP (adjustment->upper * factor + 0.5, 0, adjustment->upper - adjustment->page_size);
			gtk_adjustment_set_value (adjustment, (int)new_value);
			break;
    	        case SCROLL_TO_CURRENT_PAGE:
			scroll_to_current_page (view, orientation);
			break;
    	        case SCROLL_TO_CENTER:
			new_value = CLAMP (adjustment->upper * factor - adjustment->page_size * 0.5 + 0.5,
					   0, adjustment->upper - adjustment->page_size);
			gtk_adjustment_set_value (adjustment, (int)new_value);
			break;
	}

	gtk_adjustment_changed (adjustment);
}

static void
view_update_range_and_current_page (EvView *view)
{
	if (view->pending_scroll != SCROLL_TO_KEEP_POSITION)
		return;

	/* Presentation trumps all other modes */
	if (view->presentation) {
		view->start_page = view->current_page;
		view->end_page = view->current_page;
	} else if (view->continuous) {
		GdkRectangle current_area, unused, page_area;
		GtkBorder border;
		gint current_page;
		gboolean found = FALSE;
		int i;

		if (!(view->vadjustment && view->hadjustment))
			return;

		current_area.x = view->hadjustment->value;
		current_area.width = view->hadjustment->upper;
		current_area.y = view->vadjustment->value;
		current_area.height = view->vadjustment->page_size;

		for (i = 0; i < ev_page_cache_get_n_pages (view->page_cache); i++) {

			get_page_extents (view, i, &page_area, &border);

			if (gdk_rectangle_intersect (&current_area, &page_area, &unused)) {
				if (! found) {
					view->start_page = i;
					found = TRUE;

				}
				view->end_page = i;
			} else if (found) {
				break;
			}
		}

		current_page = ev_page_cache_get_current_page (view->page_cache);

		if (current_page < view->start_page || current_page > view->end_page) {
			view->current_page = view->start_page;
			ev_page_cache_set_current_page (view->page_cache, view->start_page);
		}
	} else {
		if (view->dual_page) {
			if (view->current_page % 2 == 0) {
				view->start_page = view->current_page;
				if (view->current_page + 1 < ev_page_cache_get_n_pages (view->page_cache))
					view->end_page = view->start_page + 1;
			} else {
				view->start_page = view->current_page - 1;
				view->end_page = view->current_page;
			}
		} else {
			view->start_page = view->current_page;
			view->end_page = view->current_page;
		}
	}

	ev_pixbuf_cache_set_page_range (view->pixbuf_cache,
					view->start_page,
					view->end_page,
					view->rotation,
					view->scale,
					view->selection_info.selections);
}

static void
set_scroll_adjustment (EvView *view,
		       GtkOrientation  orientation,
		       GtkAdjustment  *adjustment)
{
	GtkAdjustment **to_set;

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		to_set = &view->hadjustment;
	else
		to_set = &view->vadjustment;

	if (*to_set != adjustment) {
		if (*to_set) {
			g_signal_handlers_disconnect_by_func (*to_set,
							      (gpointer) on_adjustment_value_changed,
							      view);
			g_object_unref (*to_set);
		}

		*to_set = adjustment;
		view_set_adjustment_values (view, orientation);

		if (*to_set) {
			g_object_ref (*to_set);
			g_signal_connect (*to_set, "value_changed",
					  G_CALLBACK (on_adjustment_value_changed), view);
		}
	}
}

static void
ev_view_set_scroll_adjustments (EvView *view,
				GtkAdjustment  *hadjustment,
				GtkAdjustment  *vadjustment)
{
	set_scroll_adjustment (view, GTK_ORIENTATION_HORIZONTAL, hadjustment);
	set_scroll_adjustment (view, GTK_ORIENTATION_VERTICAL, vadjustment);

	on_adjustment_value_changed (NULL, view);
}

static void
add_scroll_binding_keypad (GtkBindingSet  *binding_set,
    			   guint           keyval,
    			   GtkScrollType   scroll,
			   gboolean        horizontal)
{
  guint keypad_keyval = keyval - GDK_Left + GDK_KP_Left;

  gtk_binding_entry_add_signal (binding_set, keyval, 0,
                                "binding_activated", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
  gtk_binding_entry_add_signal (binding_set, keypad_keyval, 0,
                                "binding_activated", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
}

void
ev_view_scroll (EvView        *view,
	        EvScrollType   scroll)
{
	GtkAdjustment *adjustment;
	double value, increment;
	gboolean first_page = FALSE;
	gboolean last_page = FALSE;

	/* Assign values for increment and vertical adjustment */
	adjustment = view->vadjustment;
	increment = adjustment->page_size * 0.75;
	value = adjustment->value;

	/* Assign boolean for first and last page */
	if (view->current_page == 0)
		first_page = TRUE;
	if (view->current_page == ev_page_cache_get_n_pages (view->page_cache) - 1)
		last_page = TRUE;

	switch (scroll) {
		case EV_SCROLL_PAGE_BACKWARD:
			/* Do not jump backwards if at the first page */
			if (value == (adjustment->lower) && first_page) {
				/* Do nothing */
				/* At the top of a page, assign the upper bound limit of previous page */
			} else if (value == (adjustment->lower)) {
				value = adjustment->upper - adjustment->page_size;
				ev_page_cache_set_current_page (view->page_cache, view->current_page - 1);
				/* Jump to the top */
			} else {
				value = MAX (value - increment, adjustment->lower);
			}
			break;
		case EV_SCROLL_PAGE_FORWARD:
			/* Do not jump forward if at the last page */
			if (value == (adjustment->upper - adjustment->page_size) && last_page) {
				/* Do nothing */
			/* At the bottom of a page, assign the lower bound limit of next page */
			} else if (value == (adjustment->upper - adjustment->page_size)) {
				value = 0;
				ev_page_cache_set_current_page (view->page_cache, view->current_page + 1);
			/* Jump to the bottom */
			} else {
				value = MIN (value + increment, adjustment->upper - adjustment->page_size);
			}
			break;
		default:
			break;
	}

	gtk_adjustment_set_value (adjustment, value);
}

static void
ev_view_binding_activated (EvView *view,
		    	   GtkScrollType scroll,
		    	   gboolean horizontal)
{
	GtkAdjustment *adjustment;
	double value;

	if (view->presentation) {
		switch (scroll) {
			case GTK_SCROLL_STEP_BACKWARD:
				ev_page_cache_prev_page (view->page_cache);
				break;
			case GTK_SCROLL_STEP_FORWARD:
				ev_page_cache_next_page (view->page_cache);
				break;
			default:
				break;
		}
		return;
	}

	if (horizontal) {
		adjustment = view->hadjustment;
	} else {
		adjustment = view->vadjustment;
	}

	value = adjustment->value;

	switch (scroll) {
		case GTK_SCROLL_STEP_BACKWARD:
			value -= adjustment->step_increment;
			break;
		case GTK_SCROLL_STEP_FORWARD:
			value += adjustment->step_increment;
			break;
		default:
			break;
	}

	value = CLAMP (value, adjustment->lower,
		       adjustment->upper - adjustment->page_size);

	gtk_adjustment_set_value (adjustment, value);
}

#define MARGIN 5

static void
ensure_rectangle_is_visible (EvView *view, GdkRectangle *rect)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkAdjustment *adjustment;
	int value;

	view->pending_scroll = SCROLL_TO_KEEP_POSITION;

	adjustment = view->vadjustment;

	if (rect->y < adjustment->value) {
		value = MAX (adjustment->lower, rect->y - MARGIN);
		gtk_adjustment_set_value (view->vadjustment, value);
	} else if (rect->y + rect->height >
		   adjustment->value + widget->allocation.height) {
		value = MIN (adjustment->upper, rect->y + rect->height -
			     widget->allocation.height + MARGIN);
		gtk_adjustment_set_value (view->vadjustment, value);
	}

	adjustment = view->hadjustment;

	if (rect->x < adjustment->value) {
		value = MAX (adjustment->lower, rect->x - MARGIN);
		gtk_adjustment_set_value (view->hadjustment, value);
	} else if (rect->x + rect->height >
		   adjustment->value + widget->allocation.width) {
		value = MIN (adjustment->upper, rect->x + rect->width -
			     widget->allocation.width + MARGIN);
		gtk_adjustment_set_value (view->hadjustment, value);
	}
}

/*** Geometry computations ***/

static void
compute_border (EvView *view, int width, int height, GtkBorder *border)
{
	if (view->presentation) {
		border->left = 0;
		border->right = 0;
		border->top = 0;
		border->bottom = 0;
	} else {
		ev_document_misc_get_page_border_size (width, height, border);
	}
}

static void
get_page_y_offset (EvView *view, int page, double zoom, int *y_offset)
{
	int max_width, offset;
	GtkBorder border;

	g_return_if_fail (y_offset != NULL);

	ev_page_cache_get_max_width (view->page_cache, view->rotation, zoom, &max_width);

	compute_border (view, max_width, max_width, &border);

	if (view->dual_page) {
		ev_page_cache_get_height_to_page (view->page_cache, page,
						  view->rotation, zoom, NULL, &offset);
		offset += (page / 2 + 1) * view->spacing + (page / 2) * (border.top + border.bottom);
	} else {
		ev_page_cache_get_height_to_page (view->page_cache, page,
						  view->rotation, zoom, &offset, NULL);
		offset += (page + 1) * view->spacing + page * (border.top + border.bottom);
	}

	*y_offset = offset;
	return;
}

static gboolean
get_page_extents (EvView       *view,
		  gint          page,
		  GdkRectangle *page_area,
		  GtkBorder    *border)
{
	GtkWidget *widget;
	int width, height;

	widget = GTK_WIDGET (view);

	/* Get the size of the page */
	ev_page_cache_get_size (view->page_cache, page,
				view->rotation,
				view->scale,
				&width, &height);
	compute_border (view, width, height, border);
	page_area->width = width + border->left + border->right;
	page_area->height = height + border->top + border->bottom;

	if (view->presentation) {
		page_area->x = (MAX (0, widget->allocation.width - width))/2;
		page_area->y = (MAX (0, widget->allocation.height - height))/2;
	} else if (view->continuous) {
		gint max_width;
		gint x, y;

		ev_page_cache_get_max_width (view->page_cache, view->rotation,
					     view->scale, &max_width);
		max_width = max_width + border->left + border->right;
		/* Get the location of the bounding box */
		if (view->dual_page) {
			x = view->spacing + (page % 2) * (max_width + view->spacing);
			x = x + MAX (0, widget->allocation.width - (max_width * 2 + view->spacing * 3)) / 2;
			if (page % 2 == 0)
				x = x + (max_width - width - border->left - border->right);
		} else {
			x = view->spacing;
			x = x + MAX (0, widget->allocation.width - (width + view->spacing * 2)) / 2;
		}

		get_page_y_offset (view, page, view->scale, &y);

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

			other_page = page ^ 1;

			/* First, we get the bounding box of the two pages */
			if (other_page < ev_page_cache_get_n_pages (view->page_cache)) {
				ev_page_cache_get_size (view->page_cache,
							other_page,
							view->rotation,
							view->scale,
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
			if (page % 2 == 0)
				x = x + max_width - width;
			else
				x = x + (max_width + overall_border.left + overall_border.right) + view->spacing;

			y = y + (max_height - height)/2;

			/* Adjust for extra allocation */
			x = x + MAX (0, widget->allocation.width -
				     ((max_width + overall_border.left + overall_border.right) * 2 + view->spacing * 3))/2;
			y = y + MAX (0, widget->allocation.height - (height + view->spacing * 2))/2;
		} else {
			x = view->spacing;
			y = view->spacing;

			/* Adjust for extra allocation */
			x = x + MAX (0, widget->allocation.width - (width + border->left + border->right + view->spacing * 2))/2;
			y = y + MAX (0, widget->allocation.height - (height + border->top + border->bottom +  view->spacing * 2))/2;
		}

		page_area->x = x;
		page_area->y = y;
	}

	return TRUE;
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
doc_rect_to_view_rect (EvView       *view,
                       int           page,
		       EvRectangle  *doc_rect,
		       GdkRectangle *view_rect)
{
	GdkRectangle page_area;
	GtkBorder border;
	double x, y, w, h;
	int width, height;

	ev_page_cache_get_size (view->page_cache, page,
				view->rotation,
				1.0,
				&width, &height);

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

	get_page_extents (view, page, &page_area, &border);

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

	for (i = view->start_page; i <= view->end_page; i++) {
		GdkRectangle page_area;
		GtkBorder border;

		if (! get_page_extents (view, i, &page_area, &border))
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
	GdkRegion *region;
	gint page = -1;
	gint x_offset = 0, y_offset = 0;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	if (page == -1)
		return FALSE;
	
	region = ev_pixbuf_cache_get_text_mapping (view->pixbuf_cache, page);

	if (region)
		return gdk_region_point_in (region, x_offset / view->scale, y_offset / view->scale);
	else
		return FALSE;
}

/*** Hyperref ***/
static EvLink *
get_link_at_location (EvView  *view,
		      gdouble  x,
		      gdouble  y)
{
	gint page = -1;
	gint x_offset = 0, y_offset = 0;
	GList *link_mapping;

	find_page_at_location (view, x, y, &page, &x_offset, &y_offset);

	if (page == -1)
		return NULL;

	link_mapping = ev_pixbuf_cache_get_link_mapping (view->pixbuf_cache, page);

	if (link_mapping)
		return ev_link_mapping_find (link_mapping, x_offset / view->scale, y_offset / view->scale);
	else
		return NULL;
}

/* FIXME: standardize this sometime */
static void
go_to_link (EvView *view, EvLink *link)
{
	EvLinkType type;
	const char *uri;
	int page;

	type = ev_link_get_link_type (link);

	switch (type) {
		case EV_LINK_TYPE_TITLE:
			break;
		case EV_LINK_TYPE_PAGE:
			page = ev_link_get_page (link);
			ev_page_cache_set_current_page (view->page_cache, page);
			break;
		case EV_LINK_TYPE_EXTERNAL_URI:
			uri = ev_link_get_uri (link);
			gnome_vfs_url_show (uri);
			break;
	}
}

static char *
tip_from_link (EvView *view, EvLink *link)
{
	EvLinkType type;
	char *msg = NULL;
	char *page_label;

	type = ev_link_get_link_type (link);

	switch (type) {
		case EV_LINK_TYPE_TITLE:
			if (ev_link_get_title (link))
				msg = g_strdup (ev_link_get_title (link));
			break;
		case EV_LINK_TYPE_PAGE:
			page_label = ev_page_cache_get_page_label (view->page_cache, ev_link_get_page (link));
			msg = g_strdup_printf (_("Go to page %s"), page_label);
			g_free (page_label);
			break;
		case EV_LINK_TYPE_EXTERNAL_URI:
			msg = g_strdup (ev_link_get_uri (link));
			break;
		default:
			break;
	}

	return msg;
}


/*** GtkWidget implementation ***/

static void
ev_view_size_request_continuous_dual_page (EvView         *view,
			     	           GtkRequisition *requisition)
{
	int max_width;
	gint n_pages;
	GtkBorder border;

	ev_page_cache_get_max_width (view->page_cache, view->rotation,
				     view->scale, &max_width);
	compute_border (view, max_width, max_width, &border);

	n_pages = ev_page_cache_get_n_pages (view->page_cache) + 1;

	requisition->width = (max_width + border.left + border.right) * 2 + (view->spacing * 3);
	get_page_y_offset (view, n_pages, view->scale, &requisition->height);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		requisition->width = 1;
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT) {
		requisition->width = 1;
		/* FIXME: This could actually be set on one page docs or docs
		 * with a strange aspect ratio. */
		/* requisition->height = 1;*/
	}
}

static void
ev_view_size_request_continuous (EvView         *view,
				 GtkRequisition *requisition)
{
	int max_width;
	int n_pages;
	GtkBorder border;


	ev_page_cache_get_max_width (view->page_cache, view->rotation,
				     view->scale, &max_width);
	n_pages = ev_page_cache_get_n_pages (view->page_cache);
	compute_border (view, max_width, max_width, &border);

	requisition->width = max_width + (view->spacing * 2) + border.left + border.right;
	get_page_y_offset (view, n_pages, view->scale, &requisition->height);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		requisition->width = 1;
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT) {
		requisition->width = 1;
		/* FIXME: This could actually be set on one page docs or docs
		 * with a strange aspect ratio. */
		/* requisition->height = 1;*/
	}
}

static void
ev_view_size_request_dual_page (EvView         *view,
				GtkRequisition *requisition)
{
	GtkBorder border;
	gint width, height;

	/* Find the largest of the two. */
	ev_page_cache_get_size (view->page_cache,
				view->current_page,
				view->rotation,
				view->scale,
				&width, &height);
	if (view->current_page + 1 < ev_page_cache_get_n_pages (view->page_cache)) {
		gint width_2, height_2;
		ev_page_cache_get_size (view->page_cache,
					view->current_page + 1,
					view->rotation,
					view->scale,
					&width_2, &height_2);
		if (width_2 > width) {
			width = width_2;
			height = height_2;
		}
	}
	compute_border (view, width, height, &border);

	requisition->width = ((width + border.left + border.right) * 2) +
		(view->spacing * 3);
	requisition->height = (height + border.top + border.bottom) +
		(view->spacing * 2);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		requisition->width = 1;
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT) {
		requisition->width = 1;
		requisition->height = 1;
	}
}

static void
ev_view_size_request_single_page (EvView         *view,
				  GtkRequisition *requisition)
{
	GtkBorder border;
	gint width, height;

	ev_page_cache_get_size (view->page_cache,
				view->current_page,
				view->rotation,
				view->scale,
				&width, &height);
	compute_border (view, width, height, &border);

	requisition->width = width + border.left + border.right + (2 * view->spacing);
	requisition->height = height + border.top + border.bottom + (2 * view->spacing);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH) {
		requisition->width = 1;
		requisition->height = height + border.top + border.bottom + (2 * view->spacing);
	} else if (view->sizing_mode == EV_SIZING_BEST_FIT) {
		requisition->width = 1;
		requisition->height = 1;
	}
}

static void
ev_view_size_request (GtkWidget      *widget,
		      GtkRequisition *requisition)
{
	EvView *view = EV_VIEW (widget);

	if (view->document == NULL) {
		requisition->width = 1;
		requisition->height = 1;
		return;
	}

	if (view->presentation) {
		requisition->width = 1;
		requisition->height = 1;
		return;
	}

	if (view->continuous && view->dual_page)
		ev_view_size_request_continuous_dual_page (view, requisition);
	else if (view->continuous)
		ev_view_size_request_continuous (view, requisition);
	else if (view->dual_page)
		ev_view_size_request_dual_page (view, requisition);
	else
		ev_view_size_request_single_page (view, requisition);
}

static void
ev_view_size_allocate (GtkWidget      *widget,
		       GtkAllocation  *allocation)
{
	EvView *view = EV_VIEW (widget);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH ||
		  view->sizing_mode == EV_SIZING_BEST_FIT) {

		g_signal_emit (view, signals[SIGNAL_ZOOM_INVALID], 0);

		ev_view_size_request (widget, &widget->requisition);
	}

	view_set_adjustment_values (view, GTK_ORIENTATION_HORIZONTAL);
	view_set_adjustment_values (view, GTK_ORIENTATION_VERTICAL);

	view->pending_scroll = SCROLL_TO_KEEP_POSITION;
	view->pending_resize = FALSE;

	if (view->document)
		view_update_range_and_current_page (view);

	GTK_WIDGET_CLASS (ev_view_parent_class)->size_allocate (widget, allocation);
}

static void
ev_view_realize (GtkWidget *widget)
{
	EvView *view = EV_VIEW (widget);
	GdkWindowAttr attributes;

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);


	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);

	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.event_mask = GDK_EXPOSURE_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_SCROLL_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_POINTER_MOTION_MASK |
		                GDK_LEAVE_NOTIFY_MASK;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes,
					 GDK_WA_X | GDK_WA_Y |
					 GDK_WA_COLORMAP |
					 GDK_WA_VISUAL);
	gdk_window_set_user_data (widget->window, widget);
	widget->style = gtk_style_attach (widget->style, widget->window);

	if (view->presentation)
		gdk_window_set_background (widget->window, &widget->style->black);
	else
		gdk_window_set_background (widget->window, &widget->style->mid [GTK_STATE_NORMAL]);
}

static void
ev_view_unrealize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (ev_view_parent_class)->unrealize (widget);
}

static gboolean
ev_view_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
 	EvView *view = EV_VIEW (widget);

	if ((event->state & GDK_CONTROL_MASK) != 0) {

		 ev_view_set_sizing_mode (view, EV_SIZING_FREE);

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

	if ((event->state & GDK_SHIFT_MASK) != 0) {
		if (event->direction == GDK_SCROLL_UP)
			event->direction = GDK_SCROLL_LEFT;
		if (event->direction == GDK_SCROLL_DOWN)
			event->direction = GDK_SCROLL_RIGHT;
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

static gboolean
ev_view_expose_event (GtkWidget      *widget,
		      GdkEventExpose *event)
{
	EvView *view = EV_VIEW (widget);
	int i;

	if (view->document == NULL)
		return FALSE;

	for (i = view->start_page; i <= view->end_page; i++) {
		GdkRectangle page_area;
		GtkBorder border;

		if (!get_page_extents (view, i, &page_area, &border))
			continue;

		page_area.x -= view->scroll_x;
		page_area.y -= view->scroll_y;

		draw_one_page (view, i, &page_area, &border, &(event->area));

		if (EV_IS_DOCUMENT_FIND (view->document))
			highlight_find_results (view, i);
	}

	return FALSE;
}

static gboolean
ev_view_button_press_event (GtkWidget      *widget,
			    GdkEventButton *event)
{
	EvView *view = EV_VIEW (widget);

	if (!GTK_WIDGET_HAS_FOCUS (widget)) {
		gtk_widget_grab_focus (widget);
	}

	view->pressed_button = event->button;

	switch (event->button) {
		case 1:
			if (view->selection_info.selections) {
				clear_selection (view);
				gtk_widget_queue_draw (widget);
			}

			view->selection_info.start.x = event->x + view->scroll_x;
			view->selection_info.start.y = event->y + view->scroll_y;
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
	}

	return FALSE;
}


static gboolean
selection_update_idle_cb (EvView *view)
{
	GdkPoint point;
	point.x = view->motion_x;
	point.y = view->motion_y;
	compute_selections (view, &view->selection_info.start, &point);

	view->selection_update_id = 0;
	return FALSE;
}

static gboolean
ev_view_motion_notify_event (GtkWidget      *widget,
			     GdkEventMotion *event)
{
	EvView *view = EV_VIEW (widget);

	if (!view->document)
		return FALSE;

	/* For the Evince 0.4.x release, we limit selection to un-rotated
	 * documents only.
	 */
	if (view->pressed_button == 1 &&
	    view->rotation == 0) {
		view->selection_info.in_selection = TRUE;
		view->motion_x = event->x + view->scroll_x;
		view->motion_y = event->y + view->scroll_y;

		/* Queue an idle to handle the motion.  We do this because
		 * handling any selection events in the motion could be slower
		 * than new motion events reach us.  We always put it in the
		 * idle to make sure we catch up and don't visibly lag the
		 * mouse. */
		if (! view->selection_update_id)
			view->selection_update_id = g_idle_add ((GSourceFunc)selection_update_idle_cb, view);

		return TRUE;
	} else if (view->pressed_button == 2) {
		if (!view->drag_info.in_drag) {
			gboolean start;

			start = gtk_drag_check_threshold (widget,
							  view->drag_info.start.x,
							  view->drag_info.start.y,
							  event->x_root,
							  event->y_root);
			view->drag_info.in_drag = start;
		}

		if (view->drag_info.in_drag) {
			int dx, dy;
			gdouble dhadj_value, dvadj_value;

			dx = event->x_root - view->drag_info.start.x;
			dy = event->y_root - view->drag_info.start.y;

			dhadj_value = view->hadjustment->page_size *
				      (gdouble)dx / widget->allocation.width;
			dvadj_value = view->vadjustment->page_size *
				      (gdouble)dy / widget->allocation.height;

			/* clamp scrolling to visible area */
			gtk_adjustment_set_value (view->hadjustment,
						  MIN(view->drag_info.hadj - dhadj_value,
						      view->hadjustment->upper -
						      view->hadjustment->page_size));
			gtk_adjustment_set_value (view->vadjustment,
						  MIN(view->drag_info.vadj - dvadj_value,
						      view->vadjustment->upper -
						      view->vadjustment->page_size));

			return TRUE;
		}
	/* For the Evince 0.4.x release, we limit links to un-rotated documents
	 * only.
	 */
	} else if (view->pressed_button <= 0 &&
		   view->rotation == 0) {
		EvLink *link;

		link = get_link_at_location (view, event->x + view->scroll_x, event->y + view->scroll_y);

		if (!link && view->link_tooltip) {
			ev_tooltip_deactivate (EV_TOOLTIP (view->link_tooltip));
		}

                if (link) {
			char *msg = tip_from_link (view, link);

			if (view->link_tooltip == NULL) {
				view->link_tooltip = ev_tooltip_new (GTK_WIDGET (view));
			}
			ev_tooltip_set_position (EV_TOOLTIP (view->link_tooltip), event->x, event->y);
			ev_tooltip_set_text (EV_TOOLTIP (view->link_tooltip), msg);
			ev_tooltip_activate (EV_TOOLTIP (view->link_tooltip));
			g_free (msg);

			ev_view_set_cursor (view, EV_VIEW_CURSOR_LINK);
		} else if (location_in_text (view, event->x + view->scroll_x, event->y + view->scroll_y)) {
			ev_view_set_cursor (view, EV_VIEW_CURSOR_IBEAM);
		} else {
			ev_view_set_status (view, NULL);
			if (view->cursor == EV_VIEW_CURSOR_LINK ||
			    view->cursor == EV_VIEW_CURSOR_IBEAM)
				ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
		}
		return TRUE;
	}

	return FALSE;
}

static gboolean
ev_view_button_release_event (GtkWidget      *widget,
			      GdkEventButton *event)
{
	EvView *view = EV_VIEW (widget);

	if (view->pressed_button == 2) {
		ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);
	}

	view->pressed_button = -1;
	view->drag_info.in_drag = FALSE;

	if (view->selection_info.selections) {
		ev_view_update_primary_selection (view);
	} else if (view->document) {
		EvLink *link;

		link = get_link_at_location (view, event->x + view->scroll_x, event->y + view->scroll_y);
		if (link) {
			go_to_link (view, link);
		}
	}

	return FALSE;
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
    
	ev_view_set_status (view, NULL);

	if (view->cursor == EV_VIEW_CURSOR_LINK ||
	    view->cursor == EV_VIEW_CURSOR_IBEAM)
		ev_view_set_cursor (view, EV_VIEW_CURSOR_NORMAL);

	return FALSE;
}

static void
ev_view_style_set (GtkWidget *widget,
		   GtkStyle  *old_style)
{
	if (EV_VIEW (widget)->pixbuf_cache)
		ev_pixbuf_cache_style_changed (EV_VIEW (widget)->pixbuf_cache);

	GTK_WIDGET_CLASS (ev_view_parent_class)->style_set (widget, old_style);
}


/*** Drawing ***/

static guint32
ev_gdk_color_to_rgb (const GdkColor *color)
{
  guint32 result;
  result = (0xff0000 | (color->red & 0xff00));
  result <<= 8;
  result |= ((color->green & 0xff00) | (color->blue >> 8));
  return result;
}

static void
draw_rubberband (GtkWidget *widget, GdkWindow *window,
		 const GdkRectangle *rect, guchar alpha)
{
	GdkGC *gc;
	GdkPixbuf *pixbuf;
	GdkColor *fill_color_gdk;
	guint fill_color;

	fill_color_gdk = gdk_color_copy (&GTK_WIDGET (widget)->style->base[GTK_STATE_SELECTED]);
	fill_color = ev_gdk_color_to_rgb (fill_color_gdk) << 8 | alpha;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
				 rect->width, rect->height);
	gdk_pixbuf_fill (pixbuf, fill_color);

	gdk_draw_pixbuf (window, NULL, pixbuf,
			 0, 0,
			 rect->x - EV_VIEW (widget)->scroll_x, rect->y - EV_VIEW (widget)->scroll_y,
			 rect->width, rect->height,
			 GDK_RGB_DITHER_NONE,
			 0, 0);

	g_object_unref (pixbuf);

	gc = gdk_gc_new (window);
	gdk_gc_set_rgb_fg_color (gc, fill_color_gdk);
	gdk_draw_rectangle (window, gc, FALSE,
			    rect->x - EV_VIEW (widget)->scroll_x, rect->y - EV_VIEW (widget)->scroll_y,
			    rect->width - 1,
			    rect->height - 1);
	g_object_unref (gc);

	gdk_color_free (fill_color_gdk);
}


static void
highlight_find_results (EvView *view, int page)
{
	EvDocumentFind *find;
	int i, results = 0;

	g_return_if_fail (EV_IS_DOCUMENT_FIND (view->document));

	find = EV_DOCUMENT_FIND (view->document);

	results = ev_document_find_get_n_results (find, page);

	for (i = 0; i < results; i++) {
		EvRectangle rectangle;
		GdkRectangle view_rectangle;
		guchar alpha;

		if (i == view->find_result && page == view->find_page) {
			alpha = 0x90;
		} else {
			alpha = 0x20;
		}

		ev_document_find_get_result (find, page, i, &rectangle);
		doc_rect_to_view_rect (view, page, &rectangle, &view_rectangle);
		draw_rubberband (GTK_WIDGET (view), GTK_WIDGET(view)->window,
				 &view_rectangle, alpha);
        }
}

static void
draw_loading_text (EvView       *view,
		   GdkRectangle *page_area,
		   GdkRectangle *expose_area)
{
	PangoLayout *layout;
	PangoFontDescription *font_desc;
	PangoRectangle logical_rect;
	double real_scale;
	int target_width;

	const char *loading_text = _("Loading...");	

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), loading_text);

	font_desc = pango_font_description_new ();


	/* We set the font to be 10 points, get the size, and scale appropriately */
	pango_font_description_set_size (font_desc, 10 * PANGO_SCALE);
	pango_layout_set_font_description (layout, font_desc);
	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

	/* Make sure we fit the middle of the page */
	target_width = MAX (page_area->width / 2, 1);
	real_scale = ((double)target_width / (double) logical_rect.width) * (PANGO_SCALE * 10);
	pango_font_description_set_size (font_desc, (int)real_scale);
	pango_layout_set_font_description (layout, font_desc);
	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

	gtk_paint_layout (GTK_WIDGET (view)->style,
			  GTK_WIDGET (view)->window,
			  GTK_WIDGET_STATE (view),
			  FALSE,
			  page_area,
			  GTK_WIDGET (view),
			  NULL,
			  page_area->x + (target_width/2),
			  page_area->y + (page_area->height - logical_rect.height) / 2,
			  layout);

	pango_font_description_free (font_desc);
	g_object_unref (layout);
}

static void
draw_one_page (EvView          *view,
	       gint             page,
	       GdkRectangle    *page_area,
	       GtkBorder       *border,
	       GdkRectangle    *expose_area)
{
	gint width, height;
	GdkPixbuf *current_pixbuf;
	GdkRectangle overlap;
	GdkRectangle real_page_area;
	EvViewSelection *selection;

	g_assert (view->document);
	if (! gdk_rectangle_intersect (page_area, expose_area, &overlap))
		return;

	selection = find_selection_for_page (view, page);
	ev_page_cache_get_size (view->page_cache,
				page, view->rotation,
				view->scale,
				&width, &height);
	/* Render the document itself */
	real_page_area = *page_area;

	real_page_area.x += border->left;
	real_page_area.y += border->top;
	real_page_area.width -= (border->left + border->right);
	real_page_area.height -= (border->top + border->bottom);

	ev_document_misc_paint_one_page (GTK_WIDGET(view)->window,
					 GTK_WIDGET (view),
					 page_area, border);

	if (gdk_rectangle_intersect (&real_page_area, expose_area, &overlap)) {
		GdkPixbuf *selection_pixbuf = NULL;
		GdkPixbuf *scaled_image;
		GdkPixbuf *scaled_selection;

		current_pixbuf = ev_pixbuf_cache_get_pixbuf (view->pixbuf_cache, page);

		/* Get the selection pixbuf iff we have something to draw */
		if (current_pixbuf && view->selection_mode == EV_VIEW_SELECTION_TEXT && selection)
			selection_pixbuf = ev_pixbuf_cache_get_selection_pixbuf (view->pixbuf_cache,
										 page,
										 view->scale,
										 NULL);

		if (current_pixbuf == NULL)
			scaled_image = NULL;
		else if (width == gdk_pixbuf_get_width (current_pixbuf) &&
			 height == gdk_pixbuf_get_height (current_pixbuf))
			scaled_image = g_object_ref (current_pixbuf);
		else
			/* FIXME: We don't want to scale the whole area, just the right
			 * area of it */
			scaled_image = gdk_pixbuf_scale_simple (current_pixbuf,
								width, height,
								GDK_INTERP_NEAREST);

		if (selection_pixbuf == NULL)
			scaled_selection = NULL;
		else if (width == gdk_pixbuf_get_width (selection_pixbuf) &&
			 height == gdk_pixbuf_get_height (selection_pixbuf))
			scaled_selection = g_object_ref (selection_pixbuf);
		else
			/* FIXME: We don't want to scale the whole area, just the right
			 * area of it */
			scaled_selection = gdk_pixbuf_scale_simple (selection_pixbuf,
								    width, height,
								    GDK_INTERP_NEAREST);

		if (scaled_image) {
			gdk_draw_pixbuf (GTK_WIDGET(view)->window,
					 GTK_WIDGET (view)->style->fg_gc[GTK_STATE_NORMAL],
					 scaled_image,
					 overlap.x - real_page_area.x,
					 overlap.y - real_page_area.y,
					 overlap.x, overlap.y,
					 overlap.width, overlap.height,
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
			g_object_unref (scaled_image);
		} else {
			draw_loading_text (view,
					   &real_page_area,
					   expose_area);
		}

		if (scaled_selection) {
			gdk_draw_pixbuf (GTK_WIDGET(view)->window,
					 GTK_WIDGET (view)->style->fg_gc[GTK_STATE_NORMAL],
					 scaled_selection,
					 overlap.x - real_page_area.x,
					 overlap.y - real_page_area.y,
					 overlap.x, overlap.y,
					 overlap.width, overlap.height,
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
			g_object_unref (scaled_selection);
		}
	}
}

/*** GObject functions ***/

static void
ev_view_finalize (GObject *object)
{
	EvView *view = EV_VIEW (object);

	LOG ("Finalize");

	g_free (view->status);
	g_free (view->find_status);

	clear_selection (view);

	G_OBJECT_CLASS (ev_view_parent_class)->finalize (object);
}

static void
ev_view_destroy (GtkObject *object)
{
	EvView *view = EV_VIEW (object);

	if (view->document) {
		g_object_unref (view->document);
		view->document = NULL;
	}
	if (view->pixbuf_cache) {
		g_object_unref (view->pixbuf_cache);
		view->pixbuf_cache = NULL;
	}
	if (view->link_tooltip) {
		gtk_widget_destroy (view->link_tooltip);
		view->link_tooltip = NULL;
	}

	ev_view_set_scroll_adjustments (view, NULL, NULL);

	GTK_OBJECT_CLASS (ev_view_parent_class)->destroy (object);
}

static void
ev_view_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	EvView *view = EV_VIEW (object);

	switch (prop_id)
	{
	case PROP_CONTINUOUS:
		ev_view_set_continuous (view, g_value_get_boolean (value));
		break;
	case PROP_DUAL_PAGE:
		ev_view_set_dual_page (view, g_value_get_boolean (value));
		break;
	case PROP_FULLSCREEN:
		ev_view_set_fullscreen (view, g_value_get_boolean (value));
		break;
	case PROP_PRESENTATION:
		ev_view_set_presentation (view, g_value_get_boolean (value));
		break;
	case PROP_SIZING_MODE:
		ev_view_set_sizing_mode (view, g_value_get_enum (value));
		break;
	case PROP_ZOOM:
		ev_view_set_zoom (view, g_value_get_double (value), FALSE);
		break;
	case PROP_ROTATION:
		ev_view_set_rotation (view, g_value_get_int (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_view_get_property (GObject *object,
		      guint prop_id,
		      GValue *value,
		      GParamSpec *pspec)
{
	EvView *view = EV_VIEW (object);

	switch (prop_id)
	{
	case PROP_STATUS:
		g_value_set_string (value, view->status);
		break;
	case PROP_FIND_STATUS:
		g_value_set_string (value, view->status);
		break;
	case PROP_CONTINUOUS:
		g_value_set_boolean (value, view->continuous);
		break;
	case PROP_DUAL_PAGE:
		g_value_set_boolean (value, view->dual_page);
		break;
	case PROP_FULLSCREEN:
		g_value_set_boolean (value, view->fullscreen);
		break;
	case PROP_PRESENTATION:
		g_value_set_boolean (value, view->presentation);
		break;
	case PROP_SIZING_MODE:
		g_value_set_enum (value, view->sizing_mode);
		break;
	case PROP_ZOOM:
		g_value_set_double (value, view->scale);
		break;
	case PROP_ROTATION:
		g_value_set_int (value, view->rotation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_view_class_init (EvViewClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GtkBindingSet *binding_set;

	object_class->finalize = ev_view_finalize;
	object_class->set_property = ev_view_set_property;
	object_class->get_property = ev_view_get_property;

	widget_class->expose_event = ev_view_expose_event;
	widget_class->button_press_event = ev_view_button_press_event;
	widget_class->motion_notify_event = ev_view_motion_notify_event;
	widget_class->button_release_event = ev_view_button_release_event;
	widget_class->focus_in_event = ev_view_focus_in;
	widget_class->focus_out_event = ev_view_focus_out;
	widget_class->size_request = ev_view_size_request;
	widget_class->size_allocate = ev_view_size_allocate;
	widget_class->realize = ev_view_realize;
	widget_class->unrealize = ev_view_unrealize;
	widget_class->scroll_event = ev_view_scroll_event;
	widget_class->leave_notify_event = ev_view_leave_notify_event;
	widget_class->style_set = ev_view_style_set;
	gtk_object_class->destroy = ev_view_destroy;

	class->set_scroll_adjustments = ev_view_set_scroll_adjustments;
	class->binding_activated = ev_view_binding_activated;

	widget_class->set_scroll_adjustments_signal =
	    g_signal_new ("set-scroll-adjustments",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (EvViewClass, set_scroll_adjustments),
			  NULL, NULL,
			  ev_marshal_VOID__OBJECT_OBJECT,
			  G_TYPE_NONE, 2,
			  GTK_TYPE_ADJUSTMENT,
			  GTK_TYPE_ADJUSTMENT);

	signals[SIGNAL_BINDING_ACTIVATED] = g_signal_new ("binding_activated",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, binding_activated),
		         NULL, NULL,
		         ev_marshal_VOID__ENUM_BOOLEAN,
		         G_TYPE_NONE, 2,
		         GTK_TYPE_SCROLL_TYPE,
		         G_TYPE_BOOLEAN);

	signals[SIGNAL_ZOOM_INVALID] = g_signal_new ("zoom-invalid",
	  	         G_TYPE_FROM_CLASS (object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvViewClass, zoom_invalid),
		         NULL, NULL,
		         ev_marshal_VOID__VOID,
		         G_TYPE_NONE, 0, G_TYPE_NONE);

	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "Status Message",
							      "The status message",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_FIND_STATUS,
					 g_param_spec_string ("find-status",
							      "Find Status Message",
							      "The find status message",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CONTINUOUS,
					 g_param_spec_boolean ("continuous",
							       "Continuous",
							       "Continuous scrolling mode",
							       TRUE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_DUAL_PAGE,
					 g_param_spec_boolean ("dual-page",
							       "Dual Page",
							       "Two pages visible at once",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FULLSCREEN,
					 g_param_spec_boolean ("fullscreen",
							       "Full Screen",
							       "Draw page in a fullscreen fashion",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PRESENTATION,
					 g_param_spec_boolean ("presentation",
							       "Presentation",
							       "Draw page in presentation mode",
							       TRUE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SIZING_MODE,
					 g_param_spec_enum ("sizing-mode",
							    "Sizing Mode",
							    "Sizing Mode",
							    EV_TYPE_SIZING_MODE,
							    EV_SIZING_FIT_WIDTH,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ZOOM,
					 g_param_spec_double ("zoom",
							      "Zoom factor",
							       "Zoom factor",
							       MIN_SCALE,
							       MAX_SCALE,
							       1.0,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_ROTATION,
					 g_param_spec_double ("rotation",
							      "Rotation",
							       "Rotation",
							       0,
							       360,
							       0,
							       G_PARAM_READWRITE));

	binding_set = gtk_binding_set_by_class (class);

	add_scroll_binding_keypad (binding_set, GDK_Left,  GTK_SCROLL_STEP_BACKWARD, TRUE);
	add_scroll_binding_keypad (binding_set, GDK_Right, GTK_SCROLL_STEP_FORWARD,  TRUE);
	add_scroll_binding_keypad (binding_set, GDK_Up,    GTK_SCROLL_STEP_BACKWARD, FALSE);
	add_scroll_binding_keypad (binding_set, GDK_Down,  GTK_SCROLL_STEP_FORWARD,  FALSE);
}

static void
ev_view_init (EvView *view)
{
	GTK_WIDGET_SET_FLAGS (view, GTK_CAN_FOCUS);

	view->spacing = 5;
	view->scale = 1.0;
	view->current_page = 0;
	view->pressed_button = -1;
	view->cursor = EV_VIEW_CURSOR_NORMAL;
	view->drag_info.in_drag = FALSE;
	view->selection_info.in_selection = FALSE;

	view->selection_mode = EV_VIEW_SELECTION_TEXT;
	view->continuous = TRUE;
	view->dual_page = FALSE;
	view->presentation = FALSE;
	view->fullscreen = FALSE;
	view->sizing_mode = EV_SIZING_FIT_WIDTH;
	view->pending_scroll = SCROLL_TO_KEEP_POSITION;
}

/*** Callbacks ***/

static void
find_changed_cb (EvDocument *document, int page, EvView *view)
{
	jump_to_find_page (view, EV_VIEW_FIND_NEXT);
	jump_to_find_result (view);
	update_find_status_message (view);

	if (view->current_page == page)
		gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
job_finished_cb (EvPixbufCache *pixbuf_cache,
		 EvView        *view)
{
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
page_changed_cb (EvPageCache *page_cache,
		 int          new_page,
		 EvView      *view)
{
	if (view->current_page != new_page) {

		view->current_page = new_page;
		view->pending_scroll = SCROLL_TO_CURRENT_PAGE;
		gtk_widget_queue_resize (GTK_WIDGET (view));

		if (EV_IS_DOCUMENT_FIND (view->document)) {
			view->find_page = new_page;
			view->find_result = 0;
			update_find_status_message (view);
		}
	}
}

static void on_adjustment_value_changed (GtkAdjustment  *adjustment,
				         EvView *view)
{
	int dx = 0, dy = 0;

	if (! GTK_WIDGET_REALIZED (view))
		return;

	if (view->hadjustment) {
		dx = view->scroll_x - (int) view->hadjustment->value;
		view->scroll_x = (int) view->hadjustment->value;
	} else {
		view->scroll_x = 0;
	}

	if (view->vadjustment) {
		dy = view->scroll_y - (int) view->vadjustment->value;
		view->scroll_y = (int) view->vadjustment->value;
	} else {
		view->scroll_y = 0;
	}


	if (view->pending_resize)
		gtk_widget_queue_draw (GTK_WIDGET (view));
	else
		gdk_window_scroll (GTK_WIDGET (view)->window, dx, dy);


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
	view->page_cache = ev_page_cache_get (view->document);
	g_signal_connect (view->page_cache, "page-changed", G_CALLBACK (page_changed_cb), view);
	view->pixbuf_cache = ev_pixbuf_cache_new (GTK_WIDGET (view), view->document);
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
		view->page_cache = NULL;
	}
}

void
ev_view_set_document (EvView     *view,
		      EvDocument *document)
{
	g_return_if_fail (EV_IS_VIEW (view));

	if (document != view->document) {
		clear_caches (view);

		if (view->document) {
                        g_signal_handlers_disconnect_by_func (view->document,
                                                              find_changed_cb,
                                                              view);
			g_object_unref (view->document);
			view->page_cache = NULL;

                }

		view->document = document;
		view->find_page = 0;
		view->find_result = 0;

		if (view->document) {
			g_object_ref (view->document);
			if (EV_IS_DOCUMENT_FIND (view->document)) {
				g_signal_connect (view->document,
						  "find_changed",
						  G_CALLBACK (find_changed_cb),
						  view);
			}

			setup_caches (view);
                }

		gtk_widget_queue_resize (GTK_WIDGET (view));
	}
}

/*** Zoom and sizing mode ***/

#define EPSILON 0.0000001
void
ev_view_set_zoom (EvView   *view,
		  double    factor,
		  gboolean  relative)
{
	double scale;

	if (relative)
		scale = view->scale * factor;
	else
		scale = factor;

	scale = CLAMP (scale, MIN_SCALE, MAX_SCALE);

	if (ABS (view->scale - scale) < EPSILON)
		return;

	view->scale = scale;
	view->pending_resize = TRUE;

	gtk_widget_queue_resize (GTK_WIDGET (view));

	g_object_notify (G_OBJECT (view), "zoom");
}

double
ev_view_get_zoom (EvView *view)
{
	return view->scale;
}

gboolean
ev_view_get_continuous (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

	return view->continuous;
}

void
ev_view_set_continuous (EvView   *view,
			gboolean  continuous)
{
	g_return_if_fail (EV_IS_VIEW (view));

	continuous = continuous != FALSE;

	if (view->continuous != continuous) {
		view->continuous = continuous;
		view->pending_scroll = SCROLL_TO_CURRENT_PAGE;
		gtk_widget_queue_resize (GTK_WIDGET (view));
	}

	g_object_notify (G_OBJECT (view), "continuous");
}

gboolean
ev_view_get_dual_page (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

	return view->dual_page;
}

void
ev_view_set_dual_page (EvView   *view,
		       gboolean  dual_page)
{
	g_return_if_fail (EV_IS_VIEW (view));

	dual_page = dual_page != FALSE;

	if (view->dual_page == dual_page)
		return;

	view->pending_scroll = SCROLL_TO_CURRENT_PAGE;
	view->dual_page = dual_page;
	/* FIXME: if we're keeping the pixbuf cache around, we should extend the
	 * preload_cache_size to be 2 if dual_page is set.
	 */
	gtk_widget_queue_resize (GTK_WIDGET (view));

	g_object_notify (G_OBJECT (view), "dual-page");
}

void
ev_view_set_fullscreen (EvView   *view,
			 gboolean  fullscreen)
{
	g_return_if_fail (EV_IS_VIEW (view));

	fullscreen = fullscreen != FALSE;

	if (view->fullscreen == fullscreen) 
		return;
		
	view->fullscreen = fullscreen;
	gtk_widget_queue_resize (GTK_WIDGET (view));
	
	g_object_notify (G_OBJECT (view), "fullscreen");
}

gboolean
ev_view_get_fullscreen (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

	return view->fullscreen;
}

void
ev_view_set_presentation (EvView   *view,
			  gboolean  presentation)
{
	g_return_if_fail (EV_IS_VIEW (view));

	presentation = presentation != FALSE;

	if (view->presentation == presentation)
		return;

	view->presentation = presentation;
	view->pending_scroll = SCROLL_TO_CURRENT_PAGE;
	gtk_widget_queue_resize (GTK_WIDGET (view));

	if (GTK_WIDGET_REALIZED (view)) {
		if (view->presentation)
			gdk_window_set_background (GTK_WIDGET(view)->window,
						   &GTK_WIDGET (view)->style->black);
		else
			gdk_window_set_background (GTK_WIDGET(view)->window,
						   &GTK_WIDGET (view)->style->mid [GTK_STATE_NORMAL]);
	}


	g_object_notify (G_OBJECT (view), "presentation");
}

gboolean
ev_view_get_presentation (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), FALSE);

	return view->presentation;
}

void
ev_view_set_sizing_mode (EvView       *view,
			 EvSizingMode  sizing_mode)
{
	g_return_if_fail (EV_IS_VIEW (view));

	if (view->sizing_mode == sizing_mode)
		return;

	view->sizing_mode = sizing_mode;
	gtk_widget_queue_resize (GTK_WIDGET (view));

	g_object_notify (G_OBJECT (view), "sizing-mode");
}

EvSizingMode
ev_view_get_sizing_mode (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), EV_SIZING_FREE);

	return view->sizing_mode;
}

gboolean
ev_view_can_zoom_in (EvView *view)
{
	return view->scale * ZOOM_IN_FACTOR <= MAX_SCALE;
}

gboolean
ev_view_can_zoom_out (EvView *view)
{
	return view->scale * ZOOM_OUT_FACTOR >= MIN_SCALE;
}

void
ev_view_zoom_in (EvView *view)
{
	g_return_if_fail (view->sizing_mode == EV_SIZING_FREE);

	view->pending_scroll = SCROLL_TO_CENTER;
	ev_view_set_zoom (view, ZOOM_IN_FACTOR, TRUE);
}

void
ev_view_zoom_out (EvView *view)
{
	g_return_if_fail (view->sizing_mode == EV_SIZING_FREE);

	view->pending_scroll = SCROLL_TO_CENTER;
	ev_view_set_zoom (view, ZOOM_OUT_FACTOR, TRUE);
}

void
ev_view_rotate_right (EvView *view)
{
	int rotation = view->rotation + 90;

	if (rotation >= 360) {
		rotation -= 360;
	}

	ev_view_set_rotation (view, rotation);
}

void
ev_view_rotate_left (EvView *view)
{
	int rotation = view->rotation - 90;

	if (rotation < 0) {
		rotation += 360;
	}

	ev_view_set_rotation (view, rotation);
}

void
ev_view_set_rotation (EvView *view, int rotation)
{
	view->rotation = rotation;

	if (view->pixbuf_cache) {
		ev_pixbuf_cache_clear (view->pixbuf_cache);
		gtk_widget_queue_resize (GTK_WIDGET (view));
	}

	if (rotation != 0)
		clear_selection (view);

	g_object_notify (G_OBJECT (view), "rotation");
}

int
ev_view_get_rotation (EvView *view)
{
	return view->rotation;
}

static double
zoom_for_size_fit_width (int doc_width,
			 int doc_height,
			 int target_width,
			 int target_height,
			 int vsb_width)
{
	double scale;

	scale = (double)target_width / doc_width;

	if (doc_height * scale > target_height)
		scale = (double) (target_width - vsb_width) / doc_width;

	return scale;
}

static double
zoom_for_size_best_fit (int doc_width,
			int doc_height,
			int target_width,
			int target_height,
			int vsb_width,
			int hsb_width)
{
	double w_scale;
	double h_scale;

	w_scale = (double)target_width / doc_width;
	h_scale = (double)target_height / doc_height;

	if (doc_height * w_scale > target_height)
		w_scale = (double) (target_width - vsb_width) / doc_width;
	if (doc_width * h_scale > target_width)
		h_scale = (double) (target_height - hsb_width) / doc_height;

	return MIN (w_scale, h_scale);
}


static void
ev_view_zoom_for_size_presentation (EvView *view,
				    int     width,
				    int     height)
{
	int doc_width, doc_height;
	gdouble scale;

	ev_page_cache_get_size (view->page_cache,
				view->current_page,
				view->rotation,
				1.0,
				&doc_width,
				&doc_height);
	scale = zoom_for_size_best_fit (doc_width, doc_height, width, height, 0, 0);
	ev_view_set_zoom (view, scale, FALSE);
}

static void
ev_view_zoom_for_size_continuous_and_dual_page (EvView *view,
			   int     width,
			   int     height,
			   int     vsb_width,
			   int     hsb_height)
{
	int doc_width, doc_height;
	GtkBorder border;
	gdouble scale;

	ev_page_cache_get_max_width (view->page_cache,
				     view->rotation,
				     1.0,
				     &doc_width);
	ev_page_cache_get_max_height (view->page_cache,
				      view->rotation,
				      1.0,
				      &doc_height);
	compute_border (view, doc_width, doc_height, &border);

	doc_width = doc_width * 2;
	width -= (2 * (border.left + border.right) + 3 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing - 1);

	/* FIXME: We really need to calculate the overall height here, not the
	 * page height.  We assume there's always a vertical scrollbar for
	 * now.  We need to fix this. */
	if (view->sizing_mode == EV_SIZING_FIT_WIDTH)
		scale = zoom_for_size_fit_width (doc_width, doc_height, width - vsb_width, height, 0);
	else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width - vsb_width, height, 0, hsb_height);
	else
		g_assert_not_reached ();

	ev_view_set_zoom (view, scale, FALSE);
}

static void
ev_view_zoom_for_size_continuous (EvView *view,
				  int     width,
				  int     height,
				  int     vsb_width,
				  int     hsb_height)
{
	int doc_width, doc_height;
	GtkBorder border;
	gdouble scale;

	ev_page_cache_get_max_width (view->page_cache,
				     view->rotation,
				     1.0,
				     &doc_width);
	ev_page_cache_get_max_height (view->page_cache,
				      view->rotation,
				      1.0,
				      &doc_height);
	compute_border (view, doc_width, doc_height, &border);

	width -= (border.left + border.right + 2 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing - 1);

	/* FIXME: We really need to calculate the overall height here, not the
	 * page height.  We assume there's always a vertical scrollbar for
	 * now.  We need to fix this. */
	if (view->sizing_mode == EV_SIZING_FIT_WIDTH)
		scale = zoom_for_size_fit_width (doc_width, doc_height, width - vsb_width, height, 0);
	else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width - vsb_width, height, 0, hsb_height);
	else
		g_assert_not_reached ();

	ev_view_set_zoom (view, scale, FALSE);
}

static void
ev_view_zoom_for_size_dual_page (EvView *view,
				 int     width,
				 int     height,
				 int     vsb_width,
				 int     hsb_height)
{
	GtkBorder border;
	gint doc_width, doc_height;
	gdouble scale;
	gint other_page;

	other_page = view->current_page ^ 1;

	/* Find the largest of the two. */
	ev_page_cache_get_size (view->page_cache,
				view->current_page,
				view->rotation,
				1.0,
				&doc_width, &doc_height);

	if (other_page < ev_page_cache_get_n_pages (view->page_cache)) {
		gint width_2, height_2;
		ev_page_cache_get_size (view->page_cache,
					other_page,
					view->rotation,
					1.0,
					&width_2, &height_2);
		if (width_2 > doc_width)
			doc_width = width_2;
		if (height_2 > doc_height)
			doc_height = height_2;
	}
	compute_border (view, doc_width, doc_height, &border);

	doc_width = doc_width * 2;
	width -= ((border.left + border.right)* 2 + 3 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH)
		scale = zoom_for_size_fit_width (doc_width, doc_height, width, height, vsb_width);
	else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width, height, vsb_width, hsb_height);
	else
		g_assert_not_reached ();

	ev_view_set_zoom (view, scale, FALSE);
}

static void
ev_view_zoom_for_size_single_page (EvView *view,
				   int     width,
				   int     height,
				   int     vsb_width,
				   int     hsb_height)
{
	int doc_width, doc_height;
	GtkBorder border;
	gdouble scale;

	ev_page_cache_get_size (view->page_cache,
				view->current_page,
				view->rotation,
				1.0,
				&doc_width,
				&doc_height);
	/* Get an approximate border */
	compute_border (view, width, height, &border);

	width -= (border.left + border.right + 2 * view->spacing);
	height -= (border.top + border.bottom + 2 * view->spacing);

	if (view->sizing_mode == EV_SIZING_FIT_WIDTH)
		scale = zoom_for_size_fit_width (doc_width, doc_height, width, height, vsb_width);
	else if (view->sizing_mode == EV_SIZING_BEST_FIT)
		scale = zoom_for_size_best_fit (doc_width, doc_height, width, height, vsb_width, hsb_height);
	else
		g_assert_not_reached ();

	ev_view_set_zoom (view, scale, FALSE);
}

void
ev_view_set_zoom_for_size (EvView *view,
			   int     width,
			   int     height,
			   int     vsb_width,
			   int     hsb_height)
{
	g_return_if_fail (view->sizing_mode == EV_SIZING_FIT_WIDTH ||
			  view->sizing_mode == EV_SIZING_BEST_FIT);
	g_return_if_fail (width >= 0);
	g_return_if_fail (height >= 0);

	if (view->document == NULL)
		return;

	if (view->presentation)
		ev_view_zoom_for_size_presentation (view, width, height);
	else if (view->continuous && view->dual_page)
		ev_view_zoom_for_size_continuous_and_dual_page (view, width, height, vsb_width, hsb_height);
	else if (view->continuous)
		ev_view_zoom_for_size_continuous (view, width, height, vsb_width, hsb_height);
	else if (view->dual_page)
		ev_view_zoom_for_size_dual_page (view, width, height, vsb_width, hsb_height);
	else
		ev_view_zoom_for_size_single_page (view, width, height, vsb_width, hsb_height);
}

/*** Status text messages ***/

const char *
ev_view_get_status (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), NULL);

	return view->status;
}

static void
ev_view_set_status (EvView *view, const char *message)
{
	g_return_if_fail (EV_IS_VIEW (view));

	if (message != view->status) {
		g_free (view->status);
		view->status = g_strdup (message);
		g_object_notify (G_OBJECT (view), "status");
	}
}

static void
update_find_status_message (EvView *view)
{
	char *message;

	if (view->current_page == view->find_page) {
		int results;

		results = ev_document_find_get_n_results
				(EV_DOCUMENT_FIND (view->document),
				 view->current_page);
		/* TRANS: Sometimes this could be better translated as
		   "%d hit(s) on this page".  Therefore this string
		   contains plural cases. */
		message = g_strdup_printf (ngettext ("%d found on this page",
						     "%d found on this page",
						     results),
					   results);
	} else {
		double percent;

		percent = ev_document_find_get_progress
				(EV_DOCUMENT_FIND (view->document));
		if (percent >= (1.0 - 1e-10)) {
			message = g_strdup (_("Not found"));
		} else {
			message = g_strdup_printf (_("%3d%% remaining to search"),
						   (int) ((1.0 - percent) * 100));
		}

	}
	ev_view_set_find_status (view, message);
	g_free (message);
}

const char *
ev_view_get_find_status (EvView *view)
{
	g_return_val_if_fail (EV_IS_VIEW (view), NULL);

	return view->find_status;
}

static void
ev_view_set_find_status (EvView *view, const char *message)
{
	g_return_if_fail (EV_IS_VIEW (view));

	g_free (view->find_status);
	view->find_status = g_strdup (message);
	g_object_notify (G_OBJECT (view), "find-status");
}

/*** Find ***/

static void
jump_to_find_result (EvView *view)
{
	EvDocumentFind *find = EV_DOCUMENT_FIND (view->document);
	EvRectangle rect;
	GdkRectangle view_rect;
	int n_results;
	int page = view->find_page;

	n_results = ev_document_find_get_n_results (find, page);

	if (n_results > 0  && view->find_result < n_results) {
		ev_document_find_get_result
			(find, page, view->find_result, &rect);

		doc_rect_to_view_rect (view, page, &rect, &view_rect);
		ensure_rectangle_is_visible (view, &view_rect);
	}
}

static void
jump_to_find_page (EvView *view, EvViewFindDirection direction)
{
	int n_pages, i;

	n_pages = ev_page_cache_get_n_pages (view->page_cache);

	for (i = 0; i < n_pages; i++) {
		int has_results;
		int page;
		
		if (direction == EV_VIEW_FIND_NEXT)
			page = view->find_page + i;
		else
			page = view->find_page - i;


		if (page >= n_pages) {
			page = page - n_pages;
		}
		if (page < 0) 
			page = page + n_pages;
		
		has_results = ev_document_find_page_has_results
				(EV_DOCUMENT_FIND (view->document), page);
		if (has_results == -1) {
			view->find_page = page;
			break;
		} else if (has_results == 1) {
			ev_page_cache_set_current_page (view->page_cache, page);
			break;
		}
	}
}

gboolean
ev_view_can_find_next (EvView *view)
{
	int n_results = 0;

	if (EV_IS_DOCUMENT_FIND (view->document)) {
		EvDocumentFind *find = EV_DOCUMENT_FIND (view->document);

		n_results = ev_document_find_get_n_results (find, view->current_page);
	}

	return n_results > 0;
}

void
ev_view_find_next (EvView *view)
{
	int n_results, n_pages;
	EvDocumentFind *find = EV_DOCUMENT_FIND (view->document);

	n_results = ev_document_find_get_n_results (find, view->current_page);

	n_pages = ev_page_cache_get_n_pages (view->page_cache);

	view->find_result++;

	if (view->find_result >= n_results) {

		view->find_result = 0;
		view->find_page++;
		if (view->find_page >= n_pages) {
			view->find_page = 0;
		}

		jump_to_find_page (view, EV_VIEW_FIND_NEXT);
		jump_to_find_result (view);
	} else {
		jump_to_find_result (view);
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}
}

void
ev_view_find_previous (EvView *view)
{
	int n_results, n_pages;
	EvDocumentFind *find = EV_DOCUMENT_FIND (view->document);
	EvPageCache *page_cache;

	page_cache = ev_page_cache_get (view->document);

	n_results = ev_document_find_get_n_results (find, view->current_page);

	n_pages = ev_page_cache_get_n_pages (page_cache);

	view->find_result--;

	if (view->find_result < 0) {

		view->find_page--;
		if (view->find_page < 0) {
			view->find_page = n_pages - 1;
		}

		jump_to_find_page (view, EV_VIEW_FIND_PREV);
		view->find_result = ev_document_find_get_n_results (find, view->current_page) - 1;
		jump_to_find_result (view);
	} else {
		jump_to_find_result (view);
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}
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

	n_pages = ev_page_cache_get_n_pages (view->page_cache);

	for (i = 0; i < n_pages; i++) {
		GdkRectangle page_area;
		GtkBorder border;
		
		if (get_page_extents (view, i, &page_area, &border)) {
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
compute_new_selection_text (EvView   *view,
			    GdkPoint *start,
			    GdkPoint *stop)
{
	int n_pages, i, first, last;
	GList *list = NULL;
	EvViewSelection *selection;
	gint width, height;
	int start_page, end_page;

	g_assert (view->selection_mode == EV_VIEW_SELECTION_TEXT);

	n_pages = ev_page_cache_get_n_pages (view->page_cache);

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
		
		get_page_extents (view, i, &page_area, &border);
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

		ev_page_cache_get_size (view->page_cache, i,
					view->rotation,
					1.0, &width, &height);

		selection = g_new0 (EvViewSelection, 1);
		selection->page = i;
		selection->rect.x1 = selection->rect.y1 = 0;
		selection->rect.x2 = width;
		selection->rect.y2 = height;

		get_page_extents (view, i, &page_area, &border);

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
	view->selection_info.selections = new_list;
	ev_pixbuf_cache_set_selection_list (view->pixbuf_cache, new_list);

	new_list_ptr = new_list;
	old_list_ptr = old_list;

	while (new_list_ptr || old_list_ptr) {
		EvViewSelection *old_sel, *new_sel;
		int cur_page;
		GdkRegion *region = NULL;

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
			GdkRegion *tmp_region = NULL;
			ev_pixbuf_cache_get_selection_pixbuf (view->pixbuf_cache,
							      cur_page,
							      view->scale,
							      &tmp_region);
			if (tmp_region) {
				new_sel->covered_region = gdk_region_copy (tmp_region);
			}
		}

		/* Now we figure out what needs redrawing */
		if (old_sel && new_sel) {
			if (old_sel->covered_region &&
			    new_sel->covered_region) {
				/* We only want to redraw the areas that have
				 * changed, so we xor the old and new regions
				 * and redraw if it's different */
				region = gdk_region_copy (old_sel->covered_region);
				gdk_region_xor (region, new_sel->covered_region);
				if (gdk_region_empty (region)) {
					gdk_region_destroy (region);
					region = NULL;
				}
			} else if (old_sel->covered_region) {
				region = gdk_region_copy (old_sel->covered_region);
			} else if (new_sel->covered_region) {
				region = gdk_region_copy (new_sel->covered_region);
			}
		} else if (old_sel && !new_sel) {
			if (old_sel->covered_region && !gdk_region_empty (old_sel->covered_region)) {
				region = gdk_region_copy (old_sel->covered_region);
			}
		} else if (!old_sel && new_sel) {
			if (new_sel->covered_region && !gdk_region_empty (new_sel->covered_region)) {
				region = gdk_region_copy (new_sel->covered_region);
			}
		} else {
			g_assert_not_reached ();
		}

		/* Redraw the damaged region! */
		if (region) {
			GdkRectangle page_area;
			GtkBorder border;

			get_page_extents (view, cur_page, &page_area, &border);
			gdk_region_offset (region,
					   page_area.x + border.left - view->scroll_x,
					   page_area.y + border.top - view->scroll_y);
			gdk_window_invalidate_region (GTK_WIDGET (view)->window, region, TRUE);
			gdk_region_destroy (region);
		}
	}

	/* Free the old list, now that we're done with it. */
	g_list_foreach (old_list, (GFunc) selection_free, NULL);
}

static void
compute_selections (EvView   *view,
		    GdkPoint *start,
		    GdkPoint *stop)
{
	GList *list;

	if (view->selection_mode == EV_VIEW_SELECTION_RECTANGLE)
		list = compute_new_selection_rect (view, start, stop);
	else
		list = compute_new_selection_text (view, start, stop);
	merge_selection_region (view, list);
}

/* Free's the selection.  It's up to the caller to queue redraws if needed.
 */
static void
selection_free (EvViewSelection *selection)
{
	if (selection->covered_region)
		gdk_region_destroy (selection->covered_region);
	g_free (selection);
}

static void
clear_selection (EvView *view)
{
	g_list_foreach (view->selection_info.selections, (GFunc)selection_free, NULL);
	view->selection_info.selections = NULL;
	view->selection_info.in_selection = FALSE;
}


void
ev_view_select_all (EvView *view)
{
	int n_pages, i;

	/* Disable selection on rotated pages for the 0.4.0 series */
	if (view->rotation != 0)
		return;

	clear_selection (view);

	n_pages = ev_page_cache_get_n_pages (view->page_cache);
	for (i = 0; i < n_pages; i++) {
		int width, height;
		EvViewSelection *selection;

		ev_page_cache_get_size (view->page_cache,
					i,
					view->rotation,
					1.0, &width, &height);

		selection = g_new0 (EvViewSelection, 1);
		selection->page = i;
		selection->rect.x1 = selection->rect.y1 = 0;
		selection->rect.x2 = width;
		selection->rect.y2 = height;

		view->selection_info.selections = g_list_append (view->selection_info.selections, selection);
	}

	ev_pixbuf_cache_set_selection_list (view->pixbuf_cache, view->selection_info.selections);
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

static char *
get_selected_text (EvView *ev_view)
{
	GString *text;
	GList *l;

	text = g_string_new (NULL);

	ev_document_doc_mutex_lock ();

	for (l = ev_view->selection_info.selections; l != NULL; l = l->next) {
		EvViewSelection *selection = (EvViewSelection *)l->data;
		char *tmp;

		tmp = ev_document_get_text (ev_view->document,
					    selection->page,
					    &selection->rect);
		g_string_append (text, tmp);
		g_free (tmp);
	}

	ev_document_doc_mutex_unlock ();

	return g_string_free (text, FALSE);
}

void
ev_view_copy (EvView *ev_view)
{
	GtkClipboard *clipboard;
	char *text;

	if (!ev_document_can_get_text (ev_view->document)) {
		return;
	}

	text = get_selected_text (ev_view);
	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (ev_view),
					      GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, text, -1);
	g_free (text);
}

static void
ev_view_primary_get_cb (GtkClipboard     *clipboard,
			GtkSelectionData *selection_data,
			guint             info,
			gpointer          data)
{
	EvView *ev_view = EV_VIEW (data);
	char *text;

	if (!ev_document_can_get_text (ev_view->document)) {
		return;
	}

	text = get_selected_text (ev_view);
	gtk_selection_data_set_text (selection_data, text, -1);
	g_free (text);
}

static void
ev_view_primary_clear_cb (GtkClipboard *clipboard,
			  gpointer      data)
{
	EvView *view = EV_VIEW (data);

	clear_selection (view);
}

static void
ev_view_update_primary_selection (EvView *ev_view)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (ev_view),
                                              GDK_SELECTION_PRIMARY);

	if (ev_view->selection_info.selections) {
		if (!gtk_clipboard_set_with_owner (clipboard,
						   targets,
						   G_N_ELEMENTS (targets),
						   ev_view_primary_get_cb,
						   ev_view_primary_clear_cb,
						   G_OBJECT (ev_view)))
			ev_view_primary_clear_cb (clipboard, ev_view);
	} else {
		if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (ev_view))
			gtk_clipboard_clear (clipboard);
	}
}

/*** Cursor operations ***/

static GdkCursor *
ev_view_create_invisible_cursor(void)
{
       GdkBitmap *empty;
       GdkColor black = { 0, 0, 0, 0 };
       static char bits[] = { 0x00 };

       empty = gdk_bitmap_create_from_data (NULL, bits, 1, 1);

       return gdk_cursor_new_from_pixmap (empty, empty, &black, &black, 0, 0);
}

static void
ev_view_set_cursor (EvView *view, EvViewCursor new_cursor)
{
	GdkCursor *cursor = NULL;
	GdkDisplay *display;
	GtkWidget *widget;

	if (view->cursor == new_cursor) {
		return;
	}

	widget = gtk_widget_get_toplevel (GTK_WIDGET (view));
	display = gtk_widget_get_display (widget);
	view->cursor = new_cursor;

	switch (new_cursor) {
		case EV_VIEW_CURSOR_NORMAL:
			gdk_window_set_cursor (widget->window, NULL);
			break;
		case EV_VIEW_CURSOR_IBEAM:
			cursor = gdk_cursor_new_for_display (display, GDK_XTERM);
			break;
		case EV_VIEW_CURSOR_LINK:
			cursor = gdk_cursor_new_for_display (display, GDK_HAND2);
			break;
		case EV_VIEW_CURSOR_WAIT:
			cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
			break;
                case EV_VIEW_CURSOR_HIDDEN:
                        cursor = ev_view_create_invisible_cursor ();
                        break;
		case EV_VIEW_CURSOR_DRAG:
			cursor = gdk_cursor_new_for_display (display, GDK_FLEUR);
			break;
	}

	if (cursor) {
		gdk_window_set_cursor (widget->window, cursor);
		gdk_cursor_unref (cursor);
		gdk_flush();
	}
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

/*** Enum description for usage in signal ***/

GType
ev_sizing_mode_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { EV_SIZING_FIT_WIDTH, "EV_SIZING_FIT_WIDTH", "fit-width" },
      { EV_SIZING_BEST_FIT, "EV_SIZING_BEST_FIT", "best-fit" },
      { EV_SIZING_FREE, "EV_SIZING_FREE", "free" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("EvSizingMode", values);
  }
  return etype;
}

