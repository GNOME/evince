/*
 *  Copyright (C) 2005 Jonathan Blandford
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "ev-page-view.h"
#include "ev-marshal.h"
#include "ev-document-misc.h"
#include <gtk/gtk.h>

/* We keep a cached array of all the page sizes.  The info is accessed via
 * page_sizes [page - 1], as pages start at 1 */
typedef struct _EvPageViewInfo
{
	gint width;
	gint height;
} EvPageViewInfo;

struct _EvPageViewPrivate
{
	gint width, height;
	gint page_spacing;

	GdkWindow *bin_window;
	EvDocument *document;
	EvPageViewInfo *page_sizes;

	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

	gdouble scale;

	/* Page information*/
	gint n_pages;
	gint max_page_width;

	/* these two are only set if uniform_page_size is set */
	gint uniform_page_width;
	gint uniform_page_height;
	guint uniform_page_size : 1;
};


static void     ev_page_view_init                   (EvPageView      *page_view);
static void     ev_page_view_class_init             (EvPageViewClass *klass);
static void     ev_page_view_set_scroll_adjustments (EvPageView      *page_view,
						     GtkAdjustment   *hadjustment,
						     GtkAdjustment   *vadjustment);
static void     ev_page_view_size_request           (GtkWidget       *widget,
						     GtkRequisition  *requisition);
static void     ev_page_view_size_allocate          (GtkWidget       *widget,
						     GtkAllocation   *allocation);
static gboolean ev_page_view_expose                 (GtkWidget       *widget,
						     GdkEventExpose  *expose);
static void     ev_page_view_realize                (GtkWidget       *widget);
static void     ev_page_view_unrealize              (GtkWidget       *widget);
static void     ev_page_view_map                    (GtkWidget       *widget);
static void     ev_page_view_load                   (EvPageView      *page_view);
static void     ev_page_view_adjustment_changed     (GtkAdjustment   *adjustment,
						     EvPageView      *page_view);
static void     ev_page_view_update_size            (EvPageView      *page_view);


G_DEFINE_TYPE (EvPageView, ev_page_view, GTK_TYPE_WIDGET)

#define EV_PAGE_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PAGE_VIEW, EvPageViewPrivate))

static void
ev_page_view_init (EvPageView *page_view)
{
	page_view->priv = EV_PAGE_VIEW_GET_PRIVATE (page_view);

	page_view->priv->width = 1;
	page_view->priv->height = 1;
	page_view->priv->page_spacing = 10;
	page_view->priv->scale = 1.0;

	/* Make some stuff up */
	page_view->priv->n_pages = 0;
	page_view->priv->uniform_page_width = -1;
	page_view->priv->uniform_page_height = -1;
	page_view->priv->uniform_page_size = FALSE;
}

static void
ev_page_view_class_init (EvPageViewClass *klass)
{
	GObjectClass *o_class;
	GtkWidgetClass *widget_class;

	o_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;
	klass->set_scroll_adjustments = ev_page_view_set_scroll_adjustments;

	g_type_class_add_private (klass, sizeof (EvPageViewPrivate));
	widget_class->size_request = ev_page_view_size_request;
	widget_class->size_allocate = ev_page_view_size_allocate;
	widget_class->expose_event = ev_page_view_expose;
	widget_class->realize = ev_page_view_realize;
	widget_class->unrealize = ev_page_view_unrealize;
	widget_class->map = ev_page_view_map;

	widget_class->set_scroll_adjustments_signal =
		g_signal_new ("set_scroll_adjustments",
			      G_TYPE_FROM_CLASS (o_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvPageViewClass, set_scroll_adjustments),
			      NULL, NULL,
			      ev_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE, 2,
			      GTK_TYPE_ADJUSTMENT,
			      GTK_TYPE_ADJUSTMENT);


}


static void
ev_page_view_set_scroll_adjustments (EvPageView    *page_view,
				     GtkAdjustment *hadjustment,
				     GtkAdjustment *vadjustment)
{
  gboolean need_adjust = FALSE;

  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else
    hadjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else
    vadjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (page_view->priv->hadjustment && (page_view->priv->hadjustment != hadjustment))
    {
      g_signal_handlers_disconnect_matched (page_view->priv->hadjustment, G_SIGNAL_MATCH_DATA,
					   0, 0, NULL, NULL, page_view);
      g_object_unref (page_view->priv->hadjustment);
    }

  if (page_view->priv->vadjustment && (page_view->priv->vadjustment != vadjustment))
    {
      g_signal_handlers_disconnect_matched (page_view->priv->vadjustment, G_SIGNAL_MATCH_DATA,
					    0, 0, NULL, NULL, page_view);
      g_object_unref (page_view->priv->vadjustment);
    }

  if (page_view->priv->hadjustment != hadjustment)
    {
      page_view->priv->hadjustment = hadjustment;
      g_object_ref (page_view->priv->hadjustment);
      gtk_object_sink (GTK_OBJECT (page_view->priv->hadjustment));

      g_signal_connect (page_view->priv->hadjustment, "value_changed",
			G_CALLBACK (ev_page_view_adjustment_changed),
			page_view);
      need_adjust = TRUE;
    }

  if (page_view->priv->vadjustment != vadjustment)
    {
      page_view->priv->vadjustment = vadjustment;
      g_object_ref (page_view->priv->vadjustment);
      gtk_object_sink (GTK_OBJECT (page_view->priv->vadjustment));

      g_signal_connect (page_view->priv->vadjustment, "value_changed",
			G_CALLBACK (ev_page_view_adjustment_changed),
			page_view);
      need_adjust = TRUE;
    }

  if (need_adjust)
	  ev_page_view_adjustment_changed (NULL, page_view);
}

static void
ev_page_view_update_size (EvPageView *page_view)
{
	gint left_border;
	gint right_border;
	gint top_border;
	gint bottom_border;
	gint width, height;

	g_assert (page_view->priv->scale > 0.0);

	if (page_view->priv->uniform_page_size) {
		width = (int) (page_view->priv->uniform_page_width *
			       page_view->priv->scale);
		height = (int) (page_view->priv->uniform_page_height *
				page_view->priv->scale);

		ev_document_misc_get_page_border_size (width, height,
						       & left_border, & right_border,
						       & top_border, & bottom_border);

		page_view->priv->width = width
			+ page_view->priv->page_spacing * 2
			+ left_border
			+ right_border;
		page_view->priv->height =
			((height
			  + page_view->priv->page_spacing
			  + top_border
			  + bottom_border)
			 * page_view->priv->n_pages) +
			page_view->priv->page_spacing;
	} else {
		int i;

		page_view->priv->width = 0;
		page_view->priv->height = page_view->priv->page_spacing;

		for (i = 0; i < page_view->priv->n_pages; i++) {
			width = page_view->priv->page_sizes[i].width *
				page_view->priv->scale;
			height = page_view->priv->page_sizes[i].height *
				page_view->priv->scale;

			ev_document_misc_get_page_border_size (width, height,
							       & left_border, & right_border,
							       & top_border, & bottom_border);

			width = width
				+ page_view->priv->page_spacing * 2
				+ left_border
				+ right_border;
			height = height
				+ page_view->priv->page_spacing
				+ top_border
				+ bottom_border;

			page_view->priv->width = MAX (width, page_view->priv->width);
			page_view->priv->height += height;
		}
	}

}

static void
ev_page_view_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
	EvPageView *page_view;

	page_view = EV_PAGE_VIEW (widget);

	ev_page_view_update_size (page_view);

	requisition->width = page_view->priv->width;
	requisition->height = page_view->priv->height;
}

static void
ev_page_view_paint_one_page (EvPageView   *page_view,
			     GdkRectangle *area,
			     gint          left_border,
			     gint          right_border,
			     gint          top_border,
			     gint          bottom_border)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (page_view);

		g_print ("paint one page (%d,%d) %dx%d\n",
		 area->x, area->y,
		 area->width,
		 area->height);
	gdk_draw_rectangle (page_view->priv->bin_window,
			    widget->style->black_gc,
			    TRUE,
			    area->x,
			    area->y,
			    area->width,
			    area->height);
	gdk_draw_rectangle (page_view->priv->bin_window,
			    widget->style->white_gc,
			    TRUE,
			    area->x + left_border,
			    area->y + top_border,
			    area->width - (left_border + right_border),
			    area->height - (top_border + bottom_border));
	gdk_draw_rectangle (page_view->priv->bin_window,
			    widget->style->mid_gc[widget->state],
			    TRUE,
			    area->x,
			    area->y + area->height - (bottom_border - top_border),
			    bottom_border - top_border,
			    bottom_border - top_border);
	gdk_draw_rectangle (page_view->priv->bin_window,
			    widget->style->mid_gc[widget->state],
			    TRUE,
			    area->x + area->width - (right_border - left_border),
			    area->y,
			    right_border - left_border,
			    right_border - left_border);
}

static void
ev_page_view_expose_uniform (GtkWidget      *widget,
			     GdkEventExpose *expose)
{
	EvPageView *page_view;
	gint left_border;
	gint right_border;
	gint top_border;
	gint bottom_border;
	int x_offset = 0;
	GdkRectangle rectangle;
	gint width, height;
	int i;

	page_view = EV_PAGE_VIEW (widget);

	width = (int) (page_view->priv->uniform_page_width *
		       page_view->priv->scale);
	height = (int) (page_view->priv->uniform_page_height *
			page_view->priv->scale);

	if (widget->allocation.width > page_view->priv->width)
		x_offset = (widget->allocation.width - page_view->priv->width)/2;

	ev_document_misc_get_page_border_size (width, height,
					       & left_border,
					       & right_border,
					       & top_border,
					       & bottom_border);

	rectangle.x = page_view->priv->page_spacing + x_offset;
	rectangle.y = page_view->priv->page_spacing;
	rectangle.width = width
		+ left_border
		+ right_border;
	rectangle.height = height
		+ top_border
		+ bottom_border;
	for (i = 0; i < page_view->priv->n_pages; i++) {
		GdkRectangle unused;

		if (gdk_rectangle_intersect (&rectangle,
					     &expose->area,
					     &unused))
			ev_page_view_paint_one_page (page_view,
						     & rectangle,
						     left_border, right_border,
						     top_border, bottom_border);
		rectangle.y += rectangle.height
			+ page_view->priv->page_spacing;

	}
}

static void
ev_page_view_expose_pages (GtkWidget      *widget,
			   GdkEventExpose *expose)
{
	EvPageView *page_view;
	gint left_border;
	gint right_border;
	gint top_border;
	gint bottom_border;
	int x_offset = 0;
	GdkRectangle rectangle;
	gint width, height;
	int i;

	page_view = EV_PAGE_VIEW (widget);

	width = (int) (page_view->priv->uniform_page_width *
		       page_view->priv->scale);
	height = (int) (page_view->priv->uniform_page_height *
			page_view->priv->scale);

	if (widget->allocation.width > page_view->priv->width)
		x_offset = (widget->allocation.width - page_view->priv->width)/2;

	ev_document_misc_get_page_border_size (width, height,
					       & left_border,
					       & right_border,
					       & top_border,
					       & bottom_border);

	rectangle.x = page_view->priv->page_spacing + x_offset;
	rectangle.y = page_view->priv->page_spacing;
	rectangle.width = width
		+ left_border
		+ right_border;
	rectangle.height = height
		+ top_border
		+ bottom_border;
	for (i = 0; i < page_view->priv->n_pages; i++) {
		GdkRectangle unused;

		if (gdk_rectangle_intersect (&rectangle,
					     &expose->area,
					     &unused))
			ev_page_view_paint_one_page (page_view,
						     & rectangle,
						     left_border, right_border,
						     top_border, bottom_border);
		rectangle.y += rectangle.height
			+ page_view->priv->page_spacing;

	}
}

static gboolean
ev_page_view_expose (GtkWidget      *widget,
		     GdkEventExpose *expose)
{
	EvPageView *page_view;

	page_view = EV_PAGE_VIEW (widget);

	if (expose->window != page_view->priv->bin_window)
		return FALSE;

	if (page_view->priv->uniform_page_size) {
		ev_page_view_expose_uniform (widget, expose);
	} else {
		ev_page_view_expose_pages (widget, expose);
	}

	return TRUE;
}

static void
ev_page_view_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  EvPageView *page_view;

  widget->allocation = *allocation;

  page_view = EV_PAGE_VIEW (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_resize (page_view->priv->bin_window,
			 MAX (page_view->priv->width, allocation->width),
			 MAX (page_view->priv->height, allocation->height));
    }

  page_view->priv->hadjustment->page_size = allocation->width;
  page_view->priv->hadjustment->page_increment = allocation->width * 0.9;
  page_view->priv->hadjustment->step_increment = allocation->width * 0.1;
  page_view->priv->hadjustment->lower = 0;
  page_view->priv->hadjustment->upper = MAX (allocation->width, page_view->priv->width);
  gtk_adjustment_changed (page_view->priv->hadjustment);

  page_view->priv->vadjustment->page_size = allocation->height;
  page_view->priv->vadjustment->page_increment = allocation->height * 0.9;
  page_view->priv->vadjustment->step_increment = allocation->width * 0.1;
  page_view->priv->vadjustment->lower = 0;
  page_view->priv->vadjustment->upper = MAX (allocation->height, page_view->priv->height);
  gtk_adjustment_changed (page_view->priv->vadjustment);
}

static void
ev_page_view_adjustment_changed (GtkAdjustment *adjustment,
				 EvPageView    *page_view)
{
	if (GTK_WIDGET_REALIZED (page_view)) {
		gdk_window_move (page_view->priv->bin_window,
				 - page_view->priv->hadjustment->value,
				 - page_view->priv->vadjustment->value);

		gdk_window_process_updates (page_view->priv->bin_window, TRUE);
	}
}

static void
ev_page_view_realize_document (EvPageView *page_view)
{
	if (page_view->priv->document == NULL)
		return;

	ev_document_set_target (page_view->priv->document,
				page_view->priv->bin_window);
	ev_page_view_load (page_view);
	gtk_widget_queue_resize (GTK_WIDGET (page_view));
}


static void
ev_page_view_realize (GtkWidget *widget)
{
	EvPageView *page_view;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (EV_IS_PAGE_VIEW (widget));

	page_view = EV_PAGE_VIEW (widget);
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	/* Make the main, clipping window */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	/* Make the window for the page view */
	attributes.x = 0;
	attributes.y = 0;
	attributes.width = MAX (page_view->priv->width, widget->allocation.width);
	attributes.height = MAX (page_view->priv->height, widget->allocation.height);
	attributes.event_mask = (GDK_EXPOSURE_MASK |
				 GDK_SCROLL_MASK |
				 GDK_POINTER_MOTION_MASK |
				 GDK_BUTTON_PRESS_MASK |
				 GDK_BUTTON_RELEASE_MASK |
				 GDK_KEY_PRESS_MASK |
				 GDK_KEY_RELEASE_MASK) |
		gtk_widget_get_events (widget);

	page_view->priv->bin_window = gdk_window_new (widget->window,
						      &attributes, attributes_mask);
	gdk_window_set_user_data (page_view->priv->bin_window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gdk_window_set_background (page_view->priv->bin_window, &widget->style->mid[widget->state]);
	gdk_window_set_background (widget->window, &widget->style->mid[widget->state]);

	ev_page_view_realize_document (page_view);
}


static void
ev_page_view_unrealize (GtkWidget *widget)
{
  EvPageView *page_view;

  page_view = EV_PAGE_VIEW (widget);

  gdk_window_set_user_data (page_view->priv->bin_window, NULL);
  gdk_window_destroy (page_view->priv->bin_window);
  page_view->priv->bin_window = NULL;

  /* GtkWidget::unrealize destroys children and widget->window */
  if (GTK_WIDGET_CLASS (ev_page_view_parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (ev_page_view_parent_class)->unrealize) (widget);
}

static void
ev_page_view_map (GtkWidget *widget)
{
  EvPageView *page_view;

  page_view = EV_PAGE_VIEW (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  gdk_window_show (page_view->priv->bin_window);
  gdk_window_show (widget->window);
}

static void
ev_page_view_load (EvPageView *page_view)
{
	int i;
	gboolean uniform_page_size = TRUE;
	int width = 0, height = 0;

	page_view->priv->n_pages =
		ev_document_get_n_pages (page_view->priv->document);

	for (i = 1; i <= page_view->priv->n_pages; i++) {
		EvPageViewInfo *info;
		gint page_width = 0;
		gint page_height = 0;

		ev_document_set_scale (page_view->priv->document, page_view->priv->scale);
		ev_document_get_page_size (page_view->priv->document,
					   i,
					   &page_width, &page_height);

		if (i == 1) {
			width = page_width;
			height = page_height;
		} else if (width != page_width || height != page_height) {
			/* It's a different page size.  Backfill the array. */
			int j;

			uniform_page_size = FALSE;

			page_view->priv->page_sizes =
				g_new0 (EvPageViewInfo, page_view->priv->n_pages);

			for (j = 1; j < i; j++) {

				info = &(page_view->priv->page_sizes[j - 1]);
				info->width = width;
				info->height = height;
			}
		}

		if (! uniform_page_size) {
			info = &(page_view->priv->page_sizes[i - 1]);

			info->width = page_width;
			info->height = page_height;
		}
	}

	page_view->priv->uniform_page_size = uniform_page_size;

	if (uniform_page_size) {
		page_view->priv->uniform_page_width = width;
		page_view->priv->uniform_page_height = height;
	}

	ev_page_view_update_size (page_view);

	gtk_widget_queue_resize (GTK_WIDGET (page_view));
}

/* Public functions */
GtkWidget *
ev_page_view_new (void)
{
	return g_object_new (EV_TYPE_PAGE_VIEW, NULL);
}

void
ev_page_view_set_document (EvPageView *page_view,
			   EvDocument *document)
{
	g_return_if_fail (EV_IS_PAGE_VIEW (page_view));

	if (document != page_view->priv->document) {
		if (page_view->priv->document) {
			g_object_unref (page_view->priv->document);
                }

		page_view->priv->document = document;

		if (page_view->priv->document) {
			g_object_ref (page_view->priv->document);
                }

		if (GTK_WIDGET_REALIZED (page_view)) {
			ev_page_view_realize_document (page_view);
		}
	}

}

