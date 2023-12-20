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

#include "ev-toolbar.h"

#include "ev-zoom-action.h"
#include "ev-application.h"
#include "ev-page-action-widget.h"

enum
{
	PROP_0,
	PROP_DOCUMENT_MODEL,
};

typedef struct {
	AdwHeaderBar *header_bar;

	GtkWidget *open_button;
	GtkWidget *sidebar_button;
	GtkWidget *page_selector;
	GtkWidget *annots_button;
	GtkWidget *zoom_action;
	GtkWidget *find_button;
	GtkWidget *action_menu_button;

	EvDocumentModel *model;

	EvToolbarMode toolbar_mode;
} EvToolbarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EvToolbar, ev_toolbar, ADW_TYPE_BIN)

#define GET_PRIVATE(o) ev_toolbar_get_instance_private (o)

static void
ev_toolbar_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
        EvToolbar *ev_toolbar = EV_TOOLBAR (object);
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);

        switch (prop_id) {
	case PROP_DOCUMENT_MODEL:
		priv->model = g_value_get_object (value);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_toolbar_zoom_selector_activated (GtkWidget *zoom_action,
				    EvToolbar *ev_toolbar)
{
	GtkRoot *window = gtk_widget_get_root (zoom_action);

	g_return_if_fail (EV_IS_WINDOW (window));

	ev_window_focus_view (EV_WINDOW (window));
}

static void
ev_toolbar_find_button_sensitive_changed (GtkWidget  *find_button,
					  GParamSpec *pspec,
					  EvToolbar *ev_toolbar)
{
        if (gtk_widget_is_sensitive (find_button)) {
                gtk_widget_set_tooltip_text (find_button,
                                             _("Find a word or phrase in the document"));
		gtk_button_set_icon_name (GTK_BUTTON (find_button), "edit-find-symbolic");
	} else {
                gtk_widget_set_tooltip_text (find_button,
                                             _("Search not available for this document"));
		gtk_button_set_icon_name (GTK_BUTTON (find_button), "find-unsupported-symbolic");
	}
}

static void
ev_toolbar_constructed (GObject *object)
{
        EvToolbar      *ev_toolbar = EV_TOOLBAR (object);
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);

        G_OBJECT_CLASS (ev_toolbar_parent_class)->constructed (object);

	ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (priv->page_selector),
					 priv->model);

	ev_zoom_action_set_model (EV_ZOOM_ACTION (priv->zoom_action), priv->model);
}

static void
ev_toolbar_class_init (EvToolbarClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        g_object_class->set_property = ev_toolbar_set_property;
        g_object_class->constructed = ev_toolbar_constructed;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/evince/ui/ev-toolbar.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, header_bar);
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, open_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, sidebar_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, page_selector);
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, annots_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, action_menu_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, find_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvToolbar, zoom_action);
	gtk_widget_class_bind_template_callback (widget_class, ev_toolbar_find_button_sensitive_changed);
	gtk_widget_class_bind_template_callback (widget_class, ev_toolbar_zoom_selector_activated);

	g_object_class_install_property (g_object_class,
					 PROP_DOCUMENT_MODEL,
					 g_param_spec_object ("document-model",
							      "DocumentModel",
							      "The document model",
							      EV_TYPE_DOCUMENT_MODEL,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));
}

static void
ev_toolbar_init (EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);

	priv->toolbar_mode = EV_TOOLBAR_MODE_NORMAL;

        /* Ensure GTK+ private types used by the template
         * definition before calling gtk_widget_init_template() */
        g_type_ensure (EV_TYPE_PAGE_ACTION_WIDGET);
        g_type_ensure (EV_TYPE_ZOOM_ACTION);

	gtk_widget_init_template (GTK_WIDGET (ev_toolbar));
}

GtkWidget *
ev_toolbar_new (void)
{
        return GTK_WIDGET (g_object_new (EV_TYPE_TOOLBAR, NULL));
}

void
ev_toolbar_action_menu_toggle (EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);
	gboolean is_active;

	g_return_if_fail (EV_IS_TOOLBAR (ev_toolbar));

	is_active = gtk_menu_button_get_active (GTK_MENU_BUTTON (priv->action_menu_button));

	/* FIXME: main menu can't be closed by pressing F10 again */
	gtk_menu_button_set_active (GTK_MENU_BUTTON (priv->action_menu_button), !is_active);
}

GtkWidget *
ev_toolbar_get_page_selector (EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv;

        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), NULL);

        priv = GET_PRIVATE (ev_toolbar);

        return priv->page_selector;
}

AdwHeaderBar *
ev_toolbar_get_header_bar (EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv;

        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), NULL);

        priv = GET_PRIVATE (ev_toolbar);

        return priv->header_bar;
}

void
ev_toolbar_set_mode (EvToolbar     *ev_toolbar,
                     EvToolbarMode  mode)
{
        EvToolbarPrivate *priv;

        g_return_if_fail (EV_IS_TOOLBAR (ev_toolbar));

        priv = GET_PRIVATE (ev_toolbar);
        priv->toolbar_mode = mode;

        switch (mode) {
        case EV_TOOLBAR_MODE_NORMAL:
                gtk_widget_set_visible (priv->sidebar_button, TRUE);
                gtk_widget_set_visible (priv->action_menu_button, TRUE);
                gtk_widget_set_visible (priv->zoom_action, TRUE);
                gtk_widget_set_visible (priv->page_selector, TRUE);
                gtk_widget_set_visible (priv->find_button, TRUE);
                gtk_widget_set_visible (priv->annots_button, TRUE);
                gtk_widget_set_visible (priv->open_button, FALSE);
                break;
	case EV_TOOLBAR_MODE_RECENT_VIEW:
                gtk_widget_set_visible (priv->sidebar_button, FALSE);
                gtk_widget_set_visible (priv->action_menu_button, FALSE);
                gtk_widget_set_visible (priv->zoom_action, FALSE);
                gtk_widget_set_visible (priv->page_selector, FALSE);
                gtk_widget_set_visible (priv->find_button, FALSE);
                gtk_widget_set_visible (priv->annots_button, FALSE);
                gtk_widget_set_visible (priv->open_button, TRUE);
                break;
	case EV_TOOLBAR_MODE_PASSWORD_VIEW:
		gtk_widget_set_visible (priv->sidebar_button, FALSE);
		gtk_widget_set_visible (priv->action_menu_button, FALSE);
		gtk_widget_set_visible (priv->zoom_action, FALSE);
		gtk_widget_set_visible (priv->page_selector, FALSE);
		gtk_widget_set_visible (priv->find_button, FALSE);
		gtk_widget_set_visible (priv->annots_button, FALSE);
		gtk_widget_set_visible (priv->open_button, FALSE);
		break;
        }
}

EvToolbarMode
ev_toolbar_get_mode (EvToolbar *ev_toolbar)
{
        EvToolbarPrivate *priv;

        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), EV_TOOLBAR_MODE_NORMAL);

        priv = GET_PRIVATE (ev_toolbar);

        return priv->toolbar_mode;
}
