/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Author:
 *    Marco Pesenti Gritti <marco@gnome.org>
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

#include "ev-tooltip.h"

#include <gtk/gtklabel.h>

#define DEFAULT_DELAY 500
#define STICKY_DELAY 0
#define STICKY_REVERT_DELAY 1000

struct _EvTooltipPrivate {
	GtkWidget *label;
	GTimeVal last_deactivate;
	int timer_tag;
	gboolean active;
};

G_DEFINE_TYPE (EvTooltip, ev_tooltip, GTK_TYPE_WINDOW)

#define EV_TOOLTIP_GET_PRIVATE(object) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_TOOLTIP, EvTooltipPrivate))

static gboolean
ev_tooltip_expose_event (GtkWidget      *widget,
		         GdkEventExpose *event)
{
	gtk_paint_flat_box (widget->style, widget->window,
			    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			    NULL, widget, "tooltip", 0, 0,
			    widget->allocation.width, widget->allocation.height);

	return GTK_WIDGET_CLASS (ev_tooltip_parent_class)->expose_event (widget, event);
}

static void
ev_tooltip_dispose (GObject *object)
{
	EvTooltip *tooltip = EV_TOOLTIP (object);

	if (tooltip->priv->timer_tag) {
		g_source_remove (tooltip->priv->timer_tag);
		tooltip->priv->timer_tag = 0;
	}
}

static void
ev_tooltip_class_init (EvTooltipClass *class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	g_object_class->dispose = ev_tooltip_dispose;
	widget_class->expose_event = ev_tooltip_expose_event;

	g_type_class_add_private (g_object_class, sizeof (EvTooltipPrivate));
}

static void
ev_tooltip_init (EvTooltip *tooltip)
{
	GtkWidget *window = GTK_WIDGET (tooltip);
	GtkWidget *label;

	tooltip->priv = EV_TOOLTIP_GET_PRIVATE (tooltip);

	gtk_widget_set_app_paintable (GTK_WIDGET (tooltip), TRUE);
	gtk_window_set_resizable (GTK_WINDOW (tooltip), FALSE);
	gtk_widget_set_name (window, "gtk-tooltips");

	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_container_add (GTK_CONTAINER (window), label);
	gtk_container_set_border_width (GTK_CONTAINER (window), 3);
	tooltip->priv->label = label;

	gtk_widget_show (label);
}

/* Public functions */

GtkWidget *
ev_tooltip_new (GtkWidget *parent)
{
	GtkWidget *tooltip;
	GtkWidget *toplevel;

	tooltip = g_object_new (EV_TYPE_TOOLTIP, NULL);

	GTK_WINDOW (tooltip)->type = GTK_WINDOW_POPUP;
	EV_TOOLTIP (tooltip)->parent = parent;

	toplevel = gtk_widget_get_toplevel (parent);
	gtk_window_set_transient_for (GTK_WINDOW (tooltip), GTK_WINDOW (toplevel));

	return tooltip;
}

void
ev_tooltip_set_text (EvTooltip *tooltip, const char *text)
{
	gtk_label_set_text (GTK_LABEL (tooltip->priv->label), text);
}

void
ev_tooltip_set_position (EvTooltip *tooltip, int x, int y)
{
	int root_x = 0, root_y = 0;

	if (tooltip->parent != NULL) {
		gdk_window_get_origin (tooltip->parent->window, &root_x, &root_y);
	}

	gtk_window_move (GTK_WINDOW (tooltip), x + root_x, y + root_y);
}

static gboolean
ev_tooltip_recently_shown (EvTooltip *tooltip)
{
	GTimeVal now;
	glong msec;
  
	g_get_current_time (&now);

	msec = (now.tv_sec  - tooltip->priv->last_deactivate.tv_sec) * 1000 +
	       (now.tv_usec - tooltip->priv->last_deactivate.tv_usec) / 1000;

	return (msec < STICKY_REVERT_DELAY);
}

static gint
ev_tooltip_timeout (gpointer data)
{
	GtkWidget *tooltip = GTK_WIDGET (data);

	gtk_widget_show (tooltip);

	return FALSE;
}

void
ev_tooltip_activate (EvTooltip *tooltip)
{
	int delay;

	if (tooltip->priv->active) {
		return;
	} else {
		tooltip->priv->active = TRUE;
	}

	if (ev_tooltip_recently_shown (tooltip)) {
		delay = STICKY_DELAY;
	} else {
		delay = DEFAULT_DELAY;
	}

	tooltip->priv->timer_tag = g_timeout_add (delay, ev_tooltip_timeout,
					          (gpointer)tooltip);
}

void
ev_tooltip_deactivate (EvTooltip *tooltip)
{
	if (!tooltip->priv->active) {
		return;
	} else {
		tooltip->priv->active = FALSE;
	}

	if (tooltip->priv->timer_tag) {
		g_source_remove (tooltip->priv->timer_tag);
		tooltip->priv->timer_tag = 0;
	}

	gtk_widget_hide (GTK_WIDGET (tooltip));

	g_get_current_time (&tooltip->priv->last_deactivate);
}
