/* ev-loading-message.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010, 2012 Carlos Garcia Campos <carlosgc@gnome.org>
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
#include "ev-loading-message.h"

#include <string.h>
#include <glib/gi18n.h>

struct _EvLoadingMessage {
        GtkBox     base_instance;

        GtkWidget *spinner;
};

struct _EvLoadingMessageClass {
        GtkBoxClass base_class;
};

G_DEFINE_TYPE (EvLoadingMessage, ev_loading_message, GTK_TYPE_BOX)

static void
ev_loading_message_init (EvLoadingMessage *message)
{
        GtkWidget *label;

        gtk_container_set_border_width (GTK_CONTAINER (message), 10);

        message->spinner = gtk_spinner_new ();
        gtk_box_pack_start (GTK_BOX (message), message->spinner, FALSE, FALSE, 0);
        gtk_widget_show (message->spinner);

        label = gtk_label_new (_("Loading…"));
        gtk_box_pack_start (GTK_BOX (message), label, FALSE, FALSE, 0);
        gtk_widget_show (label);
}

static void
get_widget_padding (GtkWidget *widget,
                    GtkBorder *padding)
{
        GtkStyleContext *context;
        GtkStateFlags state;

        context = gtk_widget_get_style_context (widget);
        state = gtk_style_context_get_state (context);
        gtk_style_context_get_padding (context, state, padding);
}

static void
ev_loading_message_size_allocate (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
        GtkAllocation child_allocation;
        GtkBorder padding;

        get_widget_padding (widget, &padding);
        child_allocation.y = allocation->x + padding.left;
        child_allocation.x = allocation->y + padding.top;
        child_allocation.width = MAX (1, allocation->width - (padding.left + padding.right));
        child_allocation.height = MAX (1, allocation->height - (padding.top + padding.bottom));

        GTK_WIDGET_CLASS (ev_loading_message_parent_class)->size_allocate (widget, &child_allocation);
        gtk_widget_set_allocation (widget, allocation);
}

static void
ev_loading_message_get_preferred_width (GtkWidget *widget,
                                        gint      *minimum_size,
                                        gint      *natural_size)
{
        GtkBorder padding;

        GTK_WIDGET_CLASS (ev_loading_message_parent_class)->get_preferred_width (widget, minimum_size, natural_size);

        get_widget_padding (widget, &padding);
        *minimum_size += padding.left + padding.right;
        *natural_size += padding.left + padding.right;
}

static void
ev_loading_message_get_preferred_height (GtkWidget *widget,
                                         gint      *minimum_size,
                                         gint      *natural_size)
{
        GtkBorder padding;

        GTK_WIDGET_CLASS (ev_loading_message_parent_class)->get_preferred_height (widget, minimum_size, natural_size);

        get_widget_padding (widget, &padding);
        *minimum_size += padding.top + padding.bottom;
        *natural_size += padding.top + padding.bottom;
}

static gboolean
ev_loading_message_draw (GtkWidget *widget,
                         cairo_t   *cr)
{
        GtkStyleContext *context;
        gint             width, height;

        context = gtk_widget_get_style_context (widget);
        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);

        gtk_render_background (context, cr, 0, 0, width, height);
        gtk_render_frame (context, cr, 0, 0, width, height);

        GTK_WIDGET_CLASS (ev_loading_message_parent_class)->draw (widget, cr);

        return TRUE;
}

static void
ev_loading_message_hide (GtkWidget *widget)
{
        EvLoadingMessage *message = EV_LOADING_MESSAGE (widget);

        gtk_spinner_stop (GTK_SPINNER (message->spinner));

        GTK_WIDGET_CLASS (ev_loading_message_parent_class)->hide (widget);
}

static void
ev_loading_message_show (GtkWidget *widget)
{
        EvLoadingMessage *message = EV_LOADING_MESSAGE (widget);

        gtk_spinner_start (GTK_SPINNER (message->spinner));

        GTK_WIDGET_CLASS (ev_loading_message_parent_class)->show (widget);
}

static void
ev_loading_message_class_init (EvLoadingMessageClass *klass)
{
        GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class->size_allocate = ev_loading_message_size_allocate;
        gtk_widget_class->get_preferred_width = ev_loading_message_get_preferred_width;
        gtk_widget_class->get_preferred_height = ev_loading_message_get_preferred_height;
        gtk_widget_class->draw = ev_loading_message_draw;
        gtk_widget_class->show = ev_loading_message_show;
        gtk_widget_class->hide = ev_loading_message_hide;
}

/* Public methods */
GtkWidget *
ev_loading_message_new (void)
{
        GtkWidget *message;

        message = g_object_new (EV_TYPE_LOADING_MESSAGE,
                                "orientation", GTK_ORIENTATION_HORIZONTAL,
                                "spacing", 12,
                                NULL);
        return message;
}

