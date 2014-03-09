/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *
 *  Author:
 *    Lukas Bezdicka <255993@mail.muni.cz>
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

#include <cairo.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>

#include "ev-presentation-timer.h"

struct _EvPresentationTimerPrivate
{
	gint	   time;
	gint	   remaining;
	guint	   page;
	guint	   pages;
	guint	   timeout;
	gboolean   running;
};

#define EV_PRESENTATION_TIMER_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PRESENTATION_TIMER, EvPresentationTimerPrivate))

G_DEFINE_TYPE (EvPresentationTimer, ev_presentation_timer, GTK_TYPE_DRAWING_AREA);

static gdouble
ev_presentation_timer_progress (gdouble time, gdouble remaining)
{
	return remaining/(time*60);
}

static gboolean
ev_presentation_timer_draw(GtkWidget *timer, cairo_t *cr)
{
	EvPresentationTimer *ev_timer = EV_PRESENTATION_TIMER(timer);
	GtkAllocation allocation;
	gtk_widget_get_allocation (timer, &allocation);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 5);
	guint pos = (allocation.width/ev_timer->priv->pages)*ev_timer->priv->page;
	cairo_move_to (cr,pos,2);
	cairo_line_to (cr,pos,allocation.height);
	cairo_stroke (cr);
	if(ev_timer->priv->running && ev_timer->priv->time > 0 && ev_timer->priv->remaining > 0)
	{
		gdouble progress = ev_presentation_timer_progress (ev_timer->priv->time,
								   ev_timer->priv->remaining)*(allocation.width);
		cairo_rectangle (cr, allocation.width-progress,
				 10,
				 (allocation.width-(allocation.width-progress))-10,
				 allocation.height-5);
		cairo_stroke_preserve (cr);
		cairo_fill(cr);
	}
	return FALSE;
}

static gboolean
timeout_cb (gpointer data)
{
	EvPresentationTimer *ev_timer = EV_PRESENTATION_TIMER(data);
	ev_timer->priv->remaining--;

	if(time >= 0 && ev_timer->priv->remaining >= 0)
	{
		gtk_widget_queue_draw(GTK_WIDGET(ev_timer));
	} else
		ev_timer->priv->running = FALSE;
	return ev_timer->priv->running;
}

void
ev_presentation_timer_set_pages (EvPresentationTimer *ev_timer, guint pages)
{
	if(!EV_IS_PRESENTATION_TIMER (ev_timer))
		return;
	ev_timer->priv->pages = pages;
}

void
ev_presentation_timer_set_page (EvPresentationTimer *ev_timer, guint page)
{
	if(!EV_IS_PRESENTATION_TIMER (ev_timer))
		return;
	if (page >= ev_timer->priv->pages)
	{
		page = ev_timer->priv->pages;
		ev_timer->priv->running=FALSE;
	}
	ev_timer->priv->page = page;
	gtk_widget_queue_draw(GTK_WIDGET(ev_timer));
}

static void
ev_presentation_timer_init (EvPresentationTimer *ev_timer)
{
	ev_timer->priv = EV_PRESENTATION_TIMER_GET_PRIVATE (ev_timer);
	ev_timer->priv->page = 0;
	ev_timer->priv->pages = 0;
	ev_timer->priv->remaining = 0;
	ev_timer->priv->time = 0;
	ev_timer->priv->timeout = 0;
	ev_timer->priv->running = FALSE;
}

void
ev_presentation_timer_start (EvPresentationTimer *ev_timer)
{
	if (!EV_IS_PRESENTATION_TIMER (ev_timer))
		return;
	if (ev_timer->priv->running == FALSE)
	{
		ev_timer->priv->remaining = (ev_timer->priv->time)*60;
		ev_timer->priv->running = TRUE;
		ev_timer->priv->timeout = g_timeout_add_seconds (1, timeout_cb, ev_timer);
	}
}

void
ev_presentation_timer_stop (EvPresentationTimer *ev_timer)
{
	if (!EV_IS_PRESENTATION_TIMER (ev_timer))
		return;
	if (ev_timer->priv->timeout > 0)
		g_source_remove (ev_timer->priv->timeout);
	ev_timer->priv->remaining = 0;
}

void
ev_presentation_timer_set_time (EvPresentationTimer *ev_timer,
				gint time)
{
	if (!EV_IS_PRESENTATION_TIMER (ev_timer))
		return;
	if(ev_timer->priv->running)
		ev_timer->priv->remaining = ((ev_timer->priv->remaining)/(ev_timer->priv->time)*time);
	ev_timer->priv->time = (time < -1)? -1:time;
}

static void
ev_presentation_timer_dispose (GObject *gobject)
{
	EvPresentationTimer *ev_timer = EV_PRESENTATION_TIMER (gobject);
	EvPresentationTimerPrivate *priv = EV_PRESENTATION_TIMER (ev_timer)->priv;
	if (priv->timeout > 0)
		g_source_remove (priv->timeout);
	G_OBJECT_CLASS (ev_presentation_timer_parent_class)->dispose (gobject);
}

static void
ev_presentation_timer_class_init (EvPresentationTimerClass *klass)
{
	GObjectClass    *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass  *widget_class = GTK_WIDGET_CLASS (klass);
	g_type_class_add_private (object_class, sizeof (EvPresentationTimerPrivate));
	widget_class->draw = ev_presentation_timer_draw;
	object_class->dispose =  ev_presentation_timer_dispose;
}

GtkWidget *
ev_presentation_timer_new (void)
{
	EvPresentationTimer     *ev_timer;

	ev_timer = g_object_new (EV_TYPE_PRESENTATION_TIMER, NULL);
        return GTK_WIDGET (ev_timer);
}
