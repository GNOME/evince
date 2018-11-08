/* ev-toolbar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012-2014 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2014-2018 Germán Poo-Caamaño <gpoo@gnome.org>
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

#include <glib/gi18n.h>

#include "ev-previewer-toolbar.h"

/*
#include "ev-stock-icons.h"
#include "ev-zoom-action.h"
#include "ev-application.h"
 */
#include "ev-page-action-widget.h"
#include <math.h>

enum
{
        PROP_0,
        PROP_WINDOW
};

struct _EvPreviewerToolbarPrivate {
        EvPreviewerWindow  *window;

        GtkWidget *page_selector;
        GtkWidget *print_button;
        GtkWidget *previous_button;
        GtkWidget *next_button;
        GtkWidget *zoom_in_button;
        GtkWidget *zoom_out_button;
        GtkWidget *zoom_default_button;
};

G_DEFINE_TYPE (EvPreviewerToolbar, ev_previewer_toolbar, GTK_TYPE_HEADER_BAR)

static void
ev_previewer_toolbar_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
        EvPreviewerToolbar *ev_previewer_toolbar = EV_PREVIEWER_TOOLBAR (object);

        switch (prop_id) {
        case PROP_WINDOW:
                ev_previewer_toolbar->priv->window = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_previewer_toolbar_constructed (GObject *object)
{
        EvPreviewerToolbar *ev_previewer_toolbar = EV_PREVIEWER_TOOLBAR (object);
        GtkWidget     *tool_item;
        GtkWidget     *button;
        GtkWidget     *hbox;
        GtkBuilder    *builder;

        G_OBJECT_CLASS (ev_previewer_toolbar_parent_class)->constructed (object);

        builder = gtk_builder_new_from_resource ("/org/gnome/evince/previewer/ui/previewer.ui");

        hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_EXPAND);
	gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);

        button = GTK_WIDGET (gtk_builder_get_object (builder, "go-previous-page"));
        ev_previewer_toolbar->priv->previous_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        button = GTK_WIDGET (gtk_builder_get_object (builder, "go-next-page"));
        ev_previewer_toolbar->priv->next_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        gtk_header_bar_pack_start (GTK_HEADER_BAR (ev_previewer_toolbar), hbox);
        gtk_widget_show (hbox);

        /* Page selector */
        tool_item = GTK_WIDGET (g_object_new (EV_TYPE_PAGE_ACTION_WIDGET, NULL));
        gtk_widget_set_tooltip_text (tool_item, _("Select page or search in the index"));
        atk_object_set_name (gtk_widget_get_accessible (tool_item), _("Select page"));
        ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (tool_item),
                                         ev_previewer_window_get_document_model (ev_previewer_toolbar->priv->window));
        ev_previewer_toolbar->priv->page_selector = tool_item;
        gtk_header_bar_pack_start (GTK_HEADER_BAR (ev_previewer_toolbar), tool_item);
        gtk_widget_show (tool_item);

        /* Print */
        button = GTK_WIDGET (gtk_builder_get_object (builder, "print"));
        ev_previewer_toolbar->priv->print_button = button;
        gtk_header_bar_pack_end (GTK_HEADER_BAR (ev_previewer_toolbar), button);
        gtk_widget_show (button);

        /* Zoom */
        hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_EXPAND);
	gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);

        button = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-in"));
        ev_previewer_toolbar->priv->zoom_in_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        button = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-default"));
        ev_previewer_toolbar->priv->zoom_default_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        button = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-out"));
        ev_previewer_toolbar->priv->zoom_out_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        gtk_header_bar_pack_end (GTK_HEADER_BAR (ev_previewer_toolbar), hbox);

        gtk_widget_show (hbox);

        g_object_unref (builder);
}

static void
ev_previewer_toolbar_class_init (EvPreviewerToolbarClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        g_object_class->set_property = ev_previewer_toolbar_set_property;
        g_object_class->constructed = ev_previewer_toolbar_constructed;

        g_object_class_install_property (g_object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "The evince previewer window",
                                                              EV_TYPE_PREVIEWER_WINDOW,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));

        g_type_class_add_private (g_object_class, sizeof (EvPreviewerToolbarPrivate));
}

static void
ev_previewer_toolbar_init (EvPreviewerToolbar *ev_previewer_toolbar)
{
        ev_previewer_toolbar->priv = G_TYPE_INSTANCE_GET_PRIVATE (ev_previewer_toolbar, EV_TYPE_PREVIEWER_TOOLBAR, EvPreviewerToolbarPrivate);
}

GtkWidget *
ev_previewer_toolbar_new (EvPreviewerWindow *window)
{
        g_return_val_if_fail (EV_IS_PREVIEWER_WINDOW (window), NULL);

        return GTK_WIDGET (g_object_new (EV_TYPE_PREVIEWER_TOOLBAR,
                                         "window", window,
                                         NULL));
}

GtkWidget *
ev_previewer_toolbar_get_page_selector (EvPreviewerToolbar *ev_previewer_toolbar)
{
        g_return_val_if_fail (EV_IS_PREVIEWER_TOOLBAR (ev_previewer_toolbar), NULL);

        return ev_previewer_toolbar->priv->page_selector;
}
