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

typedef struct {
        EvPreviewerWindow  *window;

        GtkWidget *page_selector;
} EvPreviewerToolbarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EvPreviewerToolbar, ev_previewer_toolbar,
                            GTK_TYPE_HEADER_BAR)

static void
ev_previewer_toolbar_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
        EvPreviewerToolbar *ev_previewer_toolbar = EV_PREVIEWER_TOOLBAR (object);
        EvPreviewerToolbarPrivate *priv;

        priv = ev_previewer_toolbar_get_instance_private (ev_previewer_toolbar);

        switch (prop_id) {
        case PROP_WINDOW:
                priv->window = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_previewer_toolbar_constructed (GObject *object)
{
        EvPreviewerToolbar *ev_previewer_toolbar = EV_PREVIEWER_TOOLBAR (object);
        EvPreviewerToolbarPrivate *priv;

        G_OBJECT_CLASS (ev_previewer_toolbar_parent_class)->constructed (object);

        priv = ev_previewer_toolbar_get_instance_private (ev_previewer_toolbar);

        /* Page selector */
        ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (priv->page_selector),
                                         ev_previewer_window_get_document_model (priv->window));
}

static void
ev_previewer_toolbar_class_init (EvPreviewerToolbarClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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

	g_type_ensure (EV_TYPE_PAGE_ACTION_WIDGET);
	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/evince/previewer/ui/previewer-toolbar.ui");

	gtk_widget_class_bind_template_child_private (widget_class, EvPreviewerToolbar, page_selector);
}

static void
ev_previewer_toolbar_init (EvPreviewerToolbar *ev_previewer_toolbar)
{
	gtk_widget_init_template (GTK_WIDGET (ev_previewer_toolbar));
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
        EvPreviewerToolbarPrivate *priv;

        g_return_val_if_fail (EV_IS_PREVIEWER_TOOLBAR (ev_previewer_toolbar), NULL);

        priv = ev_previewer_toolbar_get_instance_private (ev_previewer_toolbar);

        return priv->page_selector;
}
