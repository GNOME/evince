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

#include "ev-marshal.h"
#include "ev-view.h"

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
};

struct _EvViewClass {
	GtkWidgetClass parent_class;

	void	(*set_scroll_adjustments) (EvView         *view,
					   GtkAdjustment  *hadjustment,
					   GtkAdjustment  *vadjustment);
};

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
	/* EvView *view = EV_VIEW (widget); */
  
	requisition->width = 500;
	requisition->height = 500;
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

	attributes.event_mask = GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK | GDK_EXPOSURE_MASK;
  
	update_window_backgrounds (view);
}

static void
ev_view_unrealize (GtkWidget *widget)
{
	EvView *view = EV_VIEW (widget);

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
	/* EvView *view = EV_VIEW (widget); */
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
}

static void
ev_view_init (EvView *view)
{
	static const GdkColor white = { 0, 0xffff, 0xffff, 0xffff };
	
	gtk_widget_modify_bg (GTK_WIDGET (view), GTK_STATE_NORMAL, &white);
}

/*** Public API ***/       
     
GtkWidget*
ev_view_new (void)
{
	return g_object_new (EV_TYPE_VIEW, NULL);
}

void
ev_view_set_document (EvView     *view,
		      EvDocument *document)
{
	g_return_if_fail (EV_IS_VIEW (view));

	if (document != view->document) {
		if (view->document)
			g_object_unref (view->document);

		view->document = document;

		if (view->document)
			g_object_ref (view->document);

		gtk_widget_queue_resize (GTK_WIDGET (view));
	}
}
