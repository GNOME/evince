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

#include <gtk/gtkalignment.h>
#include <glib/gi18n.h>

#include "ev-marshal.h"
#include "ev-view.h"
#include "ev-document-find.h"

#define EV_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_VIEW, EvViewClass))
#define EV_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_VIEW))
#define EV_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_VIEW, EvViewClass))

struct _EvView {
	GtkWidget parent_instance;

	EvDocument *document;
	
	GdkWindow *bin_window;
	
	int scroll_x;
	int scroll_y;

	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

        GArray *find_results;
	int results_on_this_page;
	int next_page_with_result;
	double find_percent_complete;

	double scale;
};

struct _EvViewClass {
	GtkWidgetClass parent_class;

	void	(*set_scroll_adjustments) (EvView         *view,
					   GtkAdjustment  *hadjustment,
					   GtkAdjustment  *vadjustment);
	
	/* Should this be notify::page? */
	void	(*page_changed)           (EvView         *view);
};

static guint page_changed_signal = 0;

static void ev_view_set_scroll_adjustments (EvView         *view,
					    GtkAdjustment  *hadjustment,
					    GtkAdjustment  *vadjustment);
     
G_DEFINE_TYPE (EvView, ev_view, GTK_TYPE_WIDGET)

/*** Helper functions ***/       
     
static void
view_update_adjustments (EvView *view)
{
	int old_x = view->scroll_x;
	int old_y = view->scroll_y;
  
	if (view->hadjustment)
		view->scroll_x = view->hadjustment->value;
	else
		view->scroll_x = 0;

	if (view->vadjustment)
		view->scroll_y = view->vadjustment->value;
	else
		view->scroll_y = 0;
  
	if (GTK_WIDGET_REALIZED (view) &&
	    (view->scroll_x != old_x || view->scroll_y != old_y)) {
		gdk_window_move (view->bin_window, - view->scroll_x, - view->scroll_y);
		gdk_window_process_updates (view->bin_window, TRUE);
	}
}

static void
view_set_adjustment_values (EvView         *view,
			    GtkOrientation  orientation)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkAdjustment *adjustment;
	gboolean value_changed = FALSE;
	int requisition;
	int allocation;

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
  
	adjustment->page_size = allocation;
	adjustment->step_increment = allocation * 0.1;
	adjustment->page_increment = allocation * 0.9;
	adjustment->lower = 0;
	adjustment->upper = MAX (allocation, requisition);

	if (adjustment->value > adjustment->upper - adjustment->page_size) {
		adjustment->value = adjustment->upper - adjustment->page_size;
		value_changed = TRUE;
	}

	gtk_adjustment_changed (adjustment);
	if (value_changed)
		gtk_adjustment_value_changed (adjustment);
}

/*** Virtual function implementations ***/       
     
static void
ev_view_finalize (GObject *object)
{
	EvView *view = EV_VIEW (object);

	if (view->document)
		g_object_unref (view->document);

	ev_view_set_scroll_adjustments (view, NULL, NULL);

        g_array_free (view->find_results, TRUE);
        view->find_results = NULL;
        
	G_OBJECT_CLASS (ev_view_parent_class)->finalize (object);
}

static void
ev_view_destroy (GtkObject *object)
{
	EvView *view = EV_VIEW (object);

	ev_view_set_scroll_adjustments (view, NULL, NULL);
  
	GTK_OBJECT_CLASS (ev_view_parent_class)->destroy (object);
}

static void
ev_view_size_request (GtkWidget      *widget,
		      GtkRequisition *requisition)
{
	EvView *view = EV_VIEW (widget);

	if (GTK_WIDGET_REALIZED (widget)) {
		if (view->document) {
			ev_document_get_page_size (view->document,
						   &requisition->width,
						   &requisition->height);
		} else {
			requisition->width = 10;
			requisition->height = 10;
		}
	}
  
}

static void
ev_view_size_allocate (GtkWidget      *widget,
		       GtkAllocation  *allocation)
{
	EvView *view = EV_VIEW (widget);

	GTK_WIDGET_CLASS (ev_view_parent_class)->size_allocate (widget, allocation);

	view_set_adjustment_values (view, GTK_ORIENTATION_HORIZONTAL);
	view_set_adjustment_values (view, GTK_ORIENTATION_VERTICAL);

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_resize (view->bin_window,
				   MAX (widget->allocation.width, widget->requisition.width),
				   MAX (widget->allocation.height, widget->requisition.height));
	}
}

static void
update_window_backgrounds (EvView *view)
{
	GtkWidget *widget = GTK_WIDGET (view);
  
	if (GTK_WIDGET_REALIZED (view)) {
		gdk_window_set_background (view->bin_window,
					   &widget->style->base[GTK_WIDGET_STATE (widget)]);
	}
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
	attributes.event_mask = 0;
  
	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes,
					 GDK_WA_X | GDK_WA_Y |
					 GDK_WA_COLORMAP |
					 GDK_WA_VISUAL);
	gdk_window_set_user_data (widget->window, widget);
	widget->style = gtk_style_attach (widget->style, widget->window);
  
	attributes.x = 0;
	attributes.y = 0;
	attributes.width = MAX (widget->allocation.width, widget->requisition.width);
	attributes.height = MAX (widget->allocation.height, widget->requisition.height);
	attributes.event_mask = GDK_EXPOSURE_MASK;
  
	view->bin_window = gdk_window_new (widget->window,
					   &attributes,
					   GDK_WA_X | GDK_WA_Y |
					   GDK_WA_COLORMAP |
					   GDK_WA_VISUAL);
	gdk_window_set_user_data (view->bin_window, widget);
	gdk_window_show (view->bin_window);

	if (view->document) {
		ev_document_set_target (view->document, view->bin_window);

		/* We can't get page size without a target, so we have to
		 * queue a size request at realization. Could be fixed
		 * with EvDocument changes to allow setting a GdkScreen
		 * without setting a target.
		 */
		gtk_widget_queue_resize (widget);
	}

	update_window_backgrounds (view);
}

static void
ev_view_unrealize (GtkWidget *widget)
{
	EvView *view = EV_VIEW (widget);

	if (view->document)
		ev_document_set_target (view->document, NULL);

	gdk_window_set_user_data (view->bin_window, NULL);
	gdk_window_destroy (view->bin_window);
	view->bin_window = NULL;

	GTK_WIDGET_CLASS (ev_view_parent_class)->unrealize (widget);
}

static void
ev_view_style_set (GtkWidget      *widget,
		   GtkStyle       *previous_style)
{
	update_window_backgrounds (EV_VIEW (widget));
}

static void
ev_view_state_changed (GtkWidget    *widget,
		       GtkStateType  previous_state)
{
	update_window_backgrounds (EV_VIEW (widget));
}

static void
expose_bin_window (GtkWidget      *widget,
		   GdkEventExpose *event)
{
	EvView *view = EV_VIEW (widget);
        int i;
	int current_page;
        const EvFindResult *results;

	if (view->document == NULL)
		return;
	
	ev_document_render (view->document,
			    event->area.x, event->area.y,
			    event->area.width, event->area.height);

        results = (EvFindResult*) view->find_results->data;
	current_page = ev_document_get_page (view->document);
        i = 0;
        while (i < view->find_results->len) {
#if 0
                g_printerr ("highlighting result %d page %d at %d,%d %dx%d\n",
                            i, results[i].page_num,
                            results[i].highlight_area.x,
                            results[i].highlight_area.y,
                            results[i].highlight_area.width,
                            results[i].highlight_area.height);
#endif
		if (results[i].page_num == current_page)
			gdk_draw_rectangle (view->bin_window,
					    widget->style->base_gc[GTK_STATE_SELECTED],
					    FALSE,
					    results[i].highlight_area.x,
					    results[i].highlight_area.y,
					    results[i].highlight_area.width,
					    results[i].highlight_area.height);
                ++i;
        }
}

static gboolean
ev_view_expose_event (GtkWidget      *widget,
		      GdkEventExpose *event)
{
	EvView *view = EV_VIEW (widget);

	if (event->window == view->bin_window)
		expose_bin_window (widget, event);
	else
		return GTK_WIDGET_CLASS (ev_view_parent_class)->expose_event (widget, event);

	return FALSE;
}

static gboolean
ev_view_button_press_event (GtkWidget      *widget,
			    GdkEventButton *event)
{
	/* EvView *view = EV_VIEW (widget); */

	return FALSE;
}

static gboolean
ev_view_motion_notify_event (GtkWidget      *widget,
			     GdkEventMotion *event)
{
	/* EvView *view = EV_VIEW (widget); */
  
	return FALSE;
}

static gboolean
ev_view_button_release_event (GtkWidget      *widget,
			      GdkEventButton *event)
{
	/* EvView *view = EV_VIEW (widget); */

	return FALSE;
}

static void
on_adjustment_value_changed (GtkAdjustment  *adjustment,
			     EvView *view)
{
	view_update_adjustments (view);
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

	view_update_adjustments (view);
}

static void
ev_view_class_init (EvViewClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	object_class->finalize = ev_view_finalize;

	widget_class->expose_event = ev_view_expose_event;
	widget_class->button_press_event = ev_view_button_press_event;
	widget_class->motion_notify_event = ev_view_motion_notify_event;
	widget_class->button_release_event = ev_view_button_release_event;
	widget_class->size_request = ev_view_size_request;
	widget_class->size_allocate = ev_view_size_allocate;
	widget_class->realize = ev_view_realize;
	widget_class->unrealize = ev_view_unrealize;
	widget_class->style_set = ev_view_style_set;
	widget_class->state_changed = ev_view_state_changed;
	gtk_object_class->destroy = ev_view_destroy;
  
	class->set_scroll_adjustments = ev_view_set_scroll_adjustments;

	widget_class->set_scroll_adjustments_signal =  g_signal_new ("set-scroll-adjustments",
								     G_OBJECT_CLASS_TYPE (object_class),
								     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
								     G_STRUCT_OFFSET (EvViewClass, set_scroll_adjustments),
								     NULL, NULL,
								     ev_marshal_VOID__OBJECT_OBJECT,
								     G_TYPE_NONE, 2,
								     GTK_TYPE_ADJUSTMENT,
								     GTK_TYPE_ADJUSTMENT);
	page_changed_signal = g_signal_new ("page-changed",
					    G_OBJECT_CLASS_TYPE (object_class),
					    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					    G_STRUCT_OFFSET (EvViewClass, page_changed),
					    NULL, NULL,
					    ev_marshal_VOID__NONE,
					    G_TYPE_NONE, 0);

	g_signal_new ("find-status-changed",
		      G_OBJECT_CLASS_TYPE (object_class),
		      G_SIGNAL_RUN_LAST,
		      0,
		      NULL, NULL,
		      ev_marshal_VOID__NONE,
		      G_TYPE_NONE, 0);
}

static void
ev_view_init (EvView *view)
{
	static const GdkColor white = { 0, 0xffff, 0xffff, 0xffff };

	view->scale = 1.0;
	
	gtk_widget_modify_bg (GTK_WIDGET (view), GTK_STATE_NORMAL, &white);

        view->find_results = g_array_new (FALSE,
                                          FALSE,
                                          sizeof (EvFindResult));
	view->results_on_this_page = 0;
	view->next_page_with_result = 0;
}

static void
update_find_results (EvView *view)
{
	const EvFindResult *results;
	int i;
	int on_this_page;
	int next_page_with_result;
	int earliest_page_with_result;
	int current_page;
	gboolean counts_changed;
	
	results = (EvFindResult*) view->find_results->data;
	current_page = ev_document_get_page (view->document);
	next_page_with_result = 0;
	on_this_page = 0;
	earliest_page_with_result = 0;
	
	i = 0;
	while (i < view->find_results->len) {
		if (results[i].page_num == current_page) {
			++on_this_page;
		} else {
			int delta = results[i].page_num - current_page;
			
			if (delta > 0 && /* result on later page */
			    (next_page_with_result == 0 ||
			     results[i].page_num < next_page_with_result))
				next_page_with_result = results[i].page_num;

			if (delta < 0 && /* result on a previous page */
			    (earliest_page_with_result == 0 ||
			     results[i].page_num < earliest_page_with_result))
				earliest_page_with_result = results[i].page_num;
		}
		++i;
	}

	/* If earliest page is just the current page, there is no earliest page */
	if (earliest_page_with_result == current_page)
		earliest_page_with_result = 0;
	
	/* If no next page, then wrap and the wrapped page is the next page */
	if (next_page_with_result == 0)
		next_page_with_result = earliest_page_with_result;

	counts_changed = FALSE;
	if (on_this_page != view->results_on_this_page ||
	    next_page_with_result != view->next_page_with_result) {
		view->results_on_this_page = on_this_page;
		view->next_page_with_result = next_page_with_result;
		counts_changed = TRUE;
	}

	/* If there are no results at all, then the
	 * results of ev_view_get_find_status_message() will change
	 * to reflect the percent_complete so we have to emit the signal
	 */
	if (counts_changed ||
	    view->find_results->len == 0) {
		g_signal_emit_by_name (view,
				       "find-status-changed");
	}
}

static void
found_results_callback (EvDocument         *document,
                        const EvFindResult *results,
                        int                 n_results,
                        double              percent_complete,
                        void               *data)
{
  EvView *view = EV_VIEW (data);
  
  g_array_set_size (view->find_results, 0);

  if (n_results > 0)
          g_array_append_vals (view->find_results,
                               results, n_results);

#if 0
  {
	  int i;

	  g_printerr ("%d results %d%%: ", n_results,
		      (int) (percent_complete * 100));
	  i = 0;
	  while (i < n_results) {
		  g_printerr ("%d ", results[i].page_num);
		  ++i;
	  }
	  g_printerr ("\n");
  }
#endif

  view->find_percent_complete = percent_complete;
  update_find_results (view);
  
  gtk_widget_queue_draw (GTK_WIDGET (view));
}

/*** Public API ***/       
     
GtkWidget*
ev_view_new (void)
{
	return g_object_new (EV_TYPE_VIEW, NULL);
}

static void
document_changed_callback (EvDocument *document,
			   EvView     *view)
{
	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_set_document (EvView     *view,
		      EvDocument *document)
{
	g_return_if_fail (EV_IS_VIEW (view));

	if (document != view->document) {
		int old_page = ev_view_get_page (view);
		
		if (view->document) {
                        g_signal_handlers_disconnect_by_func (view->document,
                                                              found_results_callback,
                                                              view);
                        g_array_set_size (view->find_results, 0);
			view->results_on_this_page = 0;
			view->next_page_with_result = 0;

			g_object_unref (view->document);
                }

		view->document = document;

		if (view->document) {
			g_object_ref (view->document);
                        g_signal_connect (view->document,
                                          "found",
                                          G_CALLBACK (found_results_callback),
                                          view);
			g_signal_connect (view->document,
					  "changed",
					  G_CALLBACK (document_changed_callback),
					  view);
                }

		if (GTK_WIDGET_REALIZED (view))
			ev_document_set_target (view->document, view->bin_window);
		
		gtk_widget_queue_resize (GTK_WIDGET (view));
		
		if (old_page != ev_view_get_page (view))
			g_signal_emit (view, page_changed_signal, 0);
	}
}

void
ev_view_set_page (EvView *view,
		  int     page)
{
	if (view->document) {
		int old_page = ev_document_get_page (view->document);
		if (old_page != page)
			ev_document_set_page (view->document, page);
		if (old_page != ev_document_get_page (view->document)) {
			g_signal_emit (view, page_changed_signal, 0);

			view->find_percent_complete = 0.0;
			update_find_results (view);	
		}
	}
}

int
ev_view_get_page (EvView *view)
{
	if (view->document)
		return ev_document_get_page (view->document);
	else
		return 1;
}

#define ZOOM_IN_FACTOR  1.2
#define ZOOM_OUT_FACTOR (1.0/ZOOM_IN_FACTOR)

#define MIN_SCALE 0.05409
#define MAX_SCALE 18.4884

static void
ev_view_zoom (EvView   *view,
	      double    factor,
	      gboolean  relative)
{
	double scale;

	if (relative)
		scale = view->scale * factor;
	else
		scale = factor;

	view->scale = CLAMP (scale, MIN_SCALE, MAX_SCALE);

	ev_document_set_scale (view->document, view->scale);

	gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
ev_view_zoom_in (EvView *view)
{
	ev_view_zoom (view, ZOOM_IN_FACTOR, TRUE);
}

void
ev_view_zoom_out (EvView *view)
{
	ev_view_zoom (view, ZOOM_OUT_FACTOR, TRUE);
}

void
ev_view_normal_size (EvView *view)
{
	ev_view_zoom (view, 1.0, FALSE);
}

void
ev_view_best_fit (EvView *view)
{
	double scale;
	int width, height;

	width = height = 0;
	ev_document_get_page_size (view->document, &width, &height);

	scale = 1.0;
	if (width != 0 && height != 0) {
		double scale_w, scale_h;

		scale_w = (double)GTK_WIDGET (view)->allocation.width * view->scale / width;
		scale_h = (double)GTK_WIDGET (view)->allocation.height * view->scale / height;

		scale = (scale_w < scale_h) ? scale_w : scale_h;
	}

	ev_view_zoom (view, scale, FALSE);
}

void
ev_view_fit_width (EvView *view)
{
	double scale = 1.0;
	int width;

	width = 0;
	ev_document_get_page_size (view->document, &width, NULL);

	scale = 1.0;
	if (width != 0)
		scale = (double)GTK_WIDGET (view)->allocation.width * view->scale / width;

	ev_view_zoom (view, scale, FALSE);
}

char*
ev_view_get_find_status_message (EvView *view)
{
	if (view->find_results->len == 0) {
		if (view->find_percent_complete >= (1.0 - 1e-10)) {
			return g_strdup (_("Not found"));
		} else {
			return g_strdup_printf (_("%3d%% remaining to search"),
						(int) ((1.0 - view->find_percent_complete) * 100));
		}
	} else if (view->results_on_this_page == 0) {
		g_assert (view->next_page_with_result != 0);
		return g_strdup_printf (_("Found on page %d"),
					view->next_page_with_result);
	} else {
		return g_strdup_printf (_("%d found on this page"),
					view->results_on_this_page);
	}
}
