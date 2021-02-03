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

#include "ev-stock-icons.h"
#include "ev-zoom-action.h"
#include "ev-application.h"
#include "ev-page-action-widget.h"
#include <math.h>

enum
{
        PROP_0,
        PROP_WINDOW
};

typedef struct {
        EvWindow  *window;

        GtkWidget *action_menu_button;
        GtkWidget *zoom_action;
        GtkWidget *page_selector;
        GtkWidget *navigation_action;
        GtkWidget *find_button;
        GtkWidget *open_button;
        GtkWidget *annots_button;
        GtkWidget *sidebar_button;

        EvToolbarMode toolbar_mode;
} EvToolbarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EvToolbar, ev_toolbar, HDY_TYPE_HEADER_BAR)

# define GET_PRIVATE(o) ev_toolbar_get_instance_private (o)

static void
ev_toolbar_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
        EvToolbar *ev_toolbar = EV_TOOLBAR (object);
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);

        switch (prop_id) {
        case PROP_WINDOW:
                priv->window = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_toolbar_set_button_action (EvToolbar   *ev_toolbar,
                              GtkButton   *button,
                              const gchar *action_name,
                              const gchar *tooltip)
{
        gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action_name);
        gtk_button_set_label (button, NULL);
        gtk_widget_set_focus_on_click (GTK_WIDGET (button), FALSE);
        if (tooltip)
                gtk_widget_set_tooltip_text (GTK_WIDGET (button), tooltip);
}

static GtkWidget *
ev_toolbar_create_button (EvToolbar   *ev_toolbar,
                          const gchar *action_name,
                          const gchar *icon_name,
                          const gchar *label,
                          const gchar *tooltip)
{
        GtkWidget *button = gtk_button_new ();
        GtkWidget *image;

        image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);

        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        if (icon_name)
                gtk_button_set_image (GTK_BUTTON (button), image);
        if (label)
                gtk_button_set_label (GTK_BUTTON (button), label);
        ev_toolbar_set_button_action (ev_toolbar, GTK_BUTTON (button), action_name, tooltip);

        return button;
}

static GtkWidget *
ev_toolbar_create_toggle_button (EvToolbar *ev_toolbar,
                                 const gchar *action_name,
                                 const gchar *icon_name,
                                 const gchar *tooltip)
{
        GtkWidget *button = gtk_toggle_button_new ();
        GtkWidget *image;

        image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);

        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        gtk_button_set_image (GTK_BUTTON (button), image);
        ev_toolbar_set_button_action (ev_toolbar, GTK_BUTTON (button), action_name, tooltip);

        return button;
}

static GtkWidget *
ev_toolbar_create_menu_button (EvToolbar   *ev_toolbar,
                               const gchar *icon_name,
                               GMenuModel  *menu,
                               GtkAlign     menu_align)
{
        GtkWidget  *button;
        GtkPopover *popup;

        button = gtk_menu_button_new ();
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        gtk_widget_set_focus_on_click (button, FALSE);
        gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
        gtk_image_set_from_icon_name (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (button))),
                                      icon_name, GTK_ICON_SIZE_MENU);
        gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), menu);
        popup = gtk_menu_button_get_popover (GTK_MENU_BUTTON (button));
        gtk_popover_set_position (popup, GTK_POS_BOTTOM);
        gtk_widget_set_halign (GTK_WIDGET (popup), menu_align);

        return button;
}

static void
zoom_selector_activated (GtkWidget *zoom_action,
                         EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);

        ev_window_focus_view (priv->window);
}

static void
ev_toolbar_find_button_sensitive_changed (GtkWidget  *find_button,
					  GParamSpec *pspec,
					  EvToolbar *ev_toolbar)
{
        GtkWidget *image;

        if (gtk_widget_is_sensitive (find_button)) {
                gtk_widget_set_tooltip_text (find_button,
                                             _("Find a word or phrase in the document"));
		image = gtk_image_new_from_icon_name ("edit-find-symbolic",
						      GTK_ICON_SIZE_MENU);
		gtk_button_set_image (GTK_BUTTON (find_button), image);
	} else {
                gtk_widget_set_tooltip_text (find_button,
                                             _("Search not available for this document"));
		image = gtk_image_new_from_icon_name (EV_STOCK_FIND_UNSUPPORTED,
						      GTK_ICON_SIZE_MENU);
		gtk_button_set_image (GTK_BUTTON (find_button), image);
	}
}

static void
ev_toolbar_constructed (GObject *object)
{
        EvToolbar      *ev_toolbar = EV_TOOLBAR (object);
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);
        GtkBuilder     *builder;
        GtkWidget      *tool_item;
        GtkWidget      *vbox;
        GtkWidget      *button;
        GMenuModel     *menu;

        G_OBJECT_CLASS (ev_toolbar_parent_class)->constructed (object);

        builder = gtk_builder_new_from_resource ("/org/gnome/evince/gtk/menus.ui");

        button = ev_toolbar_create_button (ev_toolbar, "win.open",
                                           NULL,
                                           _("Open…"),
                                           _("Open an existing document"));
        priv->open_button = button;
        gtk_container_add (GTK_CONTAINER (ev_toolbar), button);

        /* Sidebar */
        button = ev_toolbar_create_toggle_button (ev_toolbar, "win.show-side-pane",
                                                  EV_STOCK_VIEW_SIDEBAR,
                                                  _("Side pane"));
        priv->sidebar_button = button;
        hdy_header_bar_pack_start (HDY_HEADER_BAR (ev_toolbar), button);

        /* Page selector */
        /* Use EvPageActionWidget for now, since the page selector action is also used by the previewer */
        tool_item = GTK_WIDGET (g_object_new (EV_TYPE_PAGE_ACTION_WIDGET, NULL));
        gtk_widget_set_tooltip_text (tool_item, _("Select page or search in the index"));
        atk_object_set_name (gtk_widget_get_accessible (tool_item), _("Select page"));
        priv->page_selector = tool_item;
        ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (tool_item),
                                         ev_window_get_document_model (priv->window));
        hdy_header_bar_pack_start (HDY_HEADER_BAR (ev_toolbar), tool_item);

        /* Edit Annots */
        button = ev_toolbar_create_toggle_button (ev_toolbar, "win.toggle-edit-annots", "document-edit-symbolic",
                                                  _("Annotate the document"));
        atk_object_set_name (gtk_widget_get_accessible (button), _("Annotate document"));
        priv->annots_button = button;
        hdy_header_bar_pack_start (HDY_HEADER_BAR (ev_toolbar), button);

        /* Action Menu */
        menu = G_MENU_MODEL (gtk_builder_get_object (builder, "action-menu"));
        button = ev_toolbar_create_menu_button (ev_toolbar, "open-menu-symbolic",
                                                menu, GTK_ALIGN_END);
        gtk_widget_set_tooltip_text (button, _("File options"));
        atk_object_set_name (gtk_widget_get_accessible (button), _("File options"));

        priv->action_menu_button = button;
        hdy_header_bar_pack_end (HDY_HEADER_BAR (ev_toolbar), button);

        /* Find */
        button = ev_toolbar_create_toggle_button (ev_toolbar, "win.toggle-find", "edit-find-symbolic",
                                                  NULL);
        priv->find_button = button;
        hdy_header_bar_pack_end (HDY_HEADER_BAR (ev_toolbar), button);
        g_signal_connect (button,
                          "notify::sensitive",
                          G_CALLBACK (ev_toolbar_find_button_sensitive_changed),
                          ev_toolbar);

        /* Zoom selector */
        vbox = ev_zoom_action_new (ev_window_get_document_model (priv->window),
                                   G_MENU (gtk_builder_get_object (builder, "zoom-menu")));
        priv->zoom_action = vbox;
        gtk_widget_set_tooltip_text (vbox, _("Select or set the zoom level of the document"));
        atk_object_set_name (gtk_widget_get_accessible (vbox), _("Set zoom level"));
        g_signal_connect (vbox, "activated",
                          G_CALLBACK (zoom_selector_activated),
                          ev_toolbar);
        hdy_header_bar_pack_end (HDY_HEADER_BAR (ev_toolbar), vbox);

        g_object_unref (builder);
}

static void
ev_toolbar_class_init (EvToolbarClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        g_object_class->set_property = ev_toolbar_set_property;
        g_object_class->constructed = ev_toolbar_constructed;

        g_object_class_install_property (g_object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "The evince window",
                                                              EV_TYPE_WINDOW,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
}

static void
ev_toolbar_init (EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv = GET_PRIVATE (ev_toolbar);

        priv->toolbar_mode = EV_TOOLBAR_MODE_NORMAL;
}

GtkWidget *
ev_toolbar_new (EvWindow *window)
{
        g_return_val_if_fail (EV_IS_WINDOW (window), NULL);

        return GTK_WIDGET (g_object_new (EV_TYPE_TOOLBAR,
                                         "window", window,
                                         NULL));
}

gboolean
ev_toolbar_has_visible_popups (EvToolbar *ev_toolbar)
{
        GtkPopover       *popover;
        EvToolbarPrivate *priv;

        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), FALSE);

        priv = GET_PRIVATE (ev_toolbar);

        popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (priv->action_menu_button));
        if (gtk_widget_get_visible (GTK_WIDGET (popover)))
                return TRUE;

        if (ev_zoom_action_get_popup_shown (EV_ZOOM_ACTION (priv->zoom_action)))
                return TRUE;

        return FALSE;
}

void
ev_toolbar_action_menu_toggle (EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv;
	gboolean is_active;

        g_return_if_fail (EV_IS_TOOLBAR (ev_toolbar));

        priv = GET_PRIVATE (ev_toolbar);
        is_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->action_menu_button));

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->action_menu_button),
                                      !is_active);
}

GtkWidget *
ev_toolbar_get_page_selector (EvToolbar *ev_toolbar)
{
	EvToolbarPrivate *priv;

        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), NULL);

        priv = GET_PRIVATE (ev_toolbar);

        return priv->page_selector;
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
        case EV_TOOLBAR_MODE_FULLSCREEN:
                gtk_widget_show (priv->sidebar_button);
                gtk_widget_show (priv->action_menu_button);
                gtk_widget_show (priv->zoom_action);
                gtk_widget_show (priv->page_selector);
                gtk_widget_show (priv->find_button);
                gtk_widget_show (priv->annots_button);
                gtk_widget_hide (priv->open_button);
                break;
	case EV_TOOLBAR_MODE_RECENT_VIEW:
                gtk_widget_hide (priv->sidebar_button);
                gtk_widget_hide (priv->action_menu_button);
                gtk_widget_hide (priv->zoom_action);
                gtk_widget_hide (priv->page_selector);
                gtk_widget_hide (priv->find_button);
                gtk_widget_hide (priv->annots_button);
                gtk_widget_show (priv->open_button);
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
