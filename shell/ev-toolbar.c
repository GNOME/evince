/* ev-toolbar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012-2014 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2014 Germán Poo-Caamaño <gpoo@gnome.org>
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
#include "ev-history-action.h"
#include "ev-application.h"
#include "ev-page-action-widget.h"
#include <math.h>

enum
{
        PROP_0,
        PROP_WINDOW
};

struct _EvToolbarPrivate {
        EvWindow  *window;

        GtkWidget *view_menu_button;
        GtkWidget *action_menu_button;
        GtkWidget *history_action;
        GtkWidget *zoom_action;
        GtkWidget *page_selector;
        GtkWidget *navigation_action;
        GtkWidget *find_button;
        GtkWidget *open_button;
        GMenu *bookmarks_section;

        EvToolbarMode toolbar_mode;
};

G_DEFINE_TYPE (EvToolbar, ev_toolbar, GTK_TYPE_HEADER_BAR)

static void
ev_toolbar_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
        EvToolbar *ev_toolbar = EV_TOOLBAR (object);

        switch (prop_id) {
        case PROP_WINDOW:
                ev_toolbar->priv->window = g_value_get_object (value);
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
        gtk_button_set_focus_on_click (button, FALSE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (button), tooltip);
}

static GtkWidget *
ev_toolbar_create_button (EvToolbar   *ev_toolbar,
                          const gchar *action_name,
                          const gchar *icon_name,
                          const gchar *tooltip)
{
        GtkWidget *button = gtk_button_new ();
        GtkWidget *image;

        image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);

        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        gtk_button_set_image (GTK_BUTTON (button), image);
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
        GtkWidget *button;
        GtkMenu *popup;

        button = gtk_menu_button_new ();
        gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), FALSE);
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
        gtk_image_set_from_icon_name (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (button))),
                                      icon_name, GTK_ICON_SIZE_MENU);
        gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), menu);
        popup = gtk_menu_button_get_popup (GTK_MENU_BUTTON (button));
        gtk_widget_set_halign (GTK_WIDGET (popup), menu_align);

        return button;
}

static void
ev_toolbar_setup_bookmarks_menu (EvToolbar  *toolbar,
                                 GMenuModel *bookmarks_submenu_model)
{
        GMenu *bookmarks_section = toolbar->priv->bookmarks_section;

        /* The bookmarks section has one or two items: "Add Bookmark"
         * and the "Bookmarks" submenu item. Hide the latter when there
         * are no bookmarks.
         */
        if (g_menu_model_get_n_items (bookmarks_submenu_model) > 0) {
                if (g_menu_model_get_n_items (G_MENU_MODEL (bookmarks_section)) == 1)
                        g_menu_append_submenu (bookmarks_section, _("Bookmarks"), bookmarks_submenu_model);
        } else {
                if (g_menu_model_get_n_items (G_MENU_MODEL (bookmarks_section)) == 2)
                        g_menu_remove (bookmarks_section, 1);
        }
}

static void
ev_toolbar_bookmarks_menu_model_changed (GMenuModel *model,
                                         gint        position,
                                         gint        removed,
                                         gint        added,
                                         EvToolbar  *toolbar)
{
        ev_toolbar_setup_bookmarks_menu (toolbar, model);
}

static void
zoom_selector_activated (GtkWidget *zoom_action,
                         EvToolbar *toolbar)
{
        ev_window_focus_view (toolbar->priv->window);
}

static void
ev_toolbar_constructed (GObject *object)
{
        EvToolbar      *ev_toolbar = EV_TOOLBAR (object);
        GtkBuilder     *builder;
        GtkWidget      *tool_item;
        GtkWidget      *hbox, *vbox;
        GtkWidget      *button;
        GMenuModel     *menu;
        GMenuModel     *bookmarks_submenu_model;

        G_OBJECT_CLASS (ev_toolbar_parent_class)->constructed (object);

        builder = gtk_builder_new_from_resource ("/org/gnome/evince/shell/ui/menus.ui");

        button = ev_toolbar_create_button (ev_toolbar, "win.open",
                                           "document-open-symbolic",
                                           _("Open an existing document"));
        ev_toolbar->priv->open_button = button;
        gtk_container_add (GTK_CONTAINER (ev_toolbar), button);
        gtk_widget_set_margin_end (button, 6);

        /* Page selector */
        /* Use EvPageActionWidget for now, since the page selector action is also used by the previewer */
        tool_item = GTK_WIDGET (g_object_new (EV_TYPE_PAGE_ACTION_WIDGET, NULL));
        ev_toolbar->priv->page_selector = tool_item;
        ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (tool_item),
                                         ev_window_get_document_model (ev_toolbar->priv->window));
        gtk_widget_set_margin_end (tool_item, 6);
        gtk_header_bar_pack_start (GTK_HEADER_BAR (ev_toolbar), tool_item);

        /* History */
        hbox = ev_history_action_new (ev_window_get_history (ev_toolbar->priv->window));
        ev_toolbar->priv->history_action = hbox;
        gtk_widget_set_margin_end (hbox, 6);
        gtk_header_bar_pack_start (GTK_HEADER_BAR (ev_toolbar), hbox);

        /* Find */
        button = ev_toolbar_create_toggle_button (ev_toolbar, "win.toggle-find", "edit-find-symbolic",
                                                  _("Find a word or phrase in the document"));
        ev_toolbar->priv->find_button = button;
        gtk_widget_set_margin_end (button, 6);
        gtk_header_bar_pack_start (GTK_HEADER_BAR (ev_toolbar), button);

        /* Action Menu */
        menu = G_MENU_MODEL (gtk_builder_get_object (builder, "action-menu"));
        button = ev_toolbar_create_menu_button (ev_toolbar, "open-menu-symbolic",
                                                menu, GTK_ALIGN_END);
        gtk_widget_set_tooltip_text (button, _("File options"));
        atk_object_set_name (gtk_widget_get_accessible (button), _("File options"));

        ev_toolbar->priv->action_menu_button = button;
        gtk_header_bar_pack_end (GTK_HEADER_BAR (ev_toolbar), button);

        /* View Menu */
        menu = G_MENU_MODEL (gtk_builder_get_object (builder, "view-menu"));
        button = ev_toolbar_create_menu_button (ev_toolbar, "document-properties-symbolic",
                                                menu, GTK_ALIGN_END);
        gtk_widget_set_tooltip_text (button, _("View options"));
        atk_object_set_name (gtk_widget_get_accessible (button), _("View options"));
        ev_toolbar->priv->view_menu_button = button;
        gtk_header_bar_pack_end (GTK_HEADER_BAR (ev_toolbar), button);

        /* Zoom selector */
        vbox = ev_zoom_action_new (ev_window_get_document_model (ev_toolbar->priv->window),
                                   G_MENU (gtk_builder_get_object (builder, "zoom-menu")));
        ev_toolbar->priv->zoom_action = vbox;
        g_signal_connect (vbox, "activated",
                          G_CALLBACK (zoom_selector_activated),
                          ev_toolbar);
        gtk_widget_set_margin_end (vbox, 6);
        gtk_header_bar_pack_end (GTK_HEADER_BAR (ev_toolbar), vbox);

        ev_toolbar->priv->bookmarks_section = G_MENU (gtk_builder_get_object (builder, "bookmarks"));
        bookmarks_submenu_model = ev_window_get_bookmarks_menu (ev_toolbar->priv->window);
        g_signal_connect (bookmarks_submenu_model, "items-changed",
                          G_CALLBACK (ev_toolbar_bookmarks_menu_model_changed),
                          ev_toolbar);
        ev_toolbar_setup_bookmarks_menu (ev_toolbar, bookmarks_submenu_model);

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

        g_type_class_add_private (g_object_class, sizeof (EvToolbarPrivate));
}

static void
ev_toolbar_init (EvToolbar *ev_toolbar)
{
        ev_toolbar->priv = G_TYPE_INSTANCE_GET_PRIVATE (ev_toolbar, EV_TYPE_TOOLBAR, EvToolbarPrivate);
        ev_toolbar->priv->toolbar_mode = EV_TOOLBAR_MODE_NORMAL;
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
        GtkMenu          *popup_menu;
        EvToolbarPrivate *priv;

        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), FALSE);

        priv = ev_toolbar->priv;

        popup_menu = gtk_menu_button_get_popup (GTK_MENU_BUTTON (priv->view_menu_button));
        if (gtk_widget_get_visible (GTK_WIDGET (popup_menu)))
                return TRUE;

        popup_menu = gtk_menu_button_get_popup (GTK_MENU_BUTTON (priv->action_menu_button));
        if (gtk_widget_get_visible (GTK_WIDGET (popup_menu)))
                return TRUE;

        if (ev_zoom_action_get_popup_shown (EV_ZOOM_ACTION (ev_toolbar->priv->zoom_action)))
                return TRUE;

        if (ev_history_action_get_popup_shown (EV_HISTORY_ACTION (ev_toolbar->priv->history_action)))
                return TRUE;

        return FALSE;
}

void
ev_toolbar_action_menu_popup (EvToolbar *ev_toolbar)
{
        g_return_if_fail (EV_IS_TOOLBAR (ev_toolbar));

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ev_toolbar->priv->action_menu_button),
                                      TRUE);
}

GtkWidget *
ev_toolbar_get_page_selector (EvToolbar *ev_toolbar)
{
        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), NULL);

        return ev_toolbar->priv->page_selector;
}

void
ev_toolbar_set_mode (EvToolbar     *ev_toolbar,
                     EvToolbarMode  mode)
{
        EvToolbarPrivate *priv;

        g_return_if_fail (EV_IS_TOOLBAR (ev_toolbar));

        priv = ev_toolbar->priv;
        priv->toolbar_mode = mode;

        switch (mode) {
        case EV_TOOLBAR_MODE_NORMAL:
                gtk_widget_show (priv->view_menu_button);
                gtk_widget_show (priv->action_menu_button);
                gtk_widget_show (priv->history_action);
                gtk_widget_show (priv->zoom_action);
                gtk_widget_show (priv->page_selector);
                gtk_widget_show (priv->find_button);
                gtk_widget_hide (priv->open_button);
                break;
        case EV_TOOLBAR_MODE_FULLSCREEN:
                gtk_widget_show (priv->view_menu_button);
                gtk_widget_show (priv->action_menu_button);
                gtk_widget_show (priv->history_action);
                gtk_widget_show (priv->zoom_action);
                gtk_widget_show (priv->page_selector);
                gtk_widget_show (priv->find_button);
                gtk_widget_hide (priv->open_button);
                break;
	case EV_TOOLBAR_MODE_RECENT_VIEW:
                gtk_widget_hide (priv->view_menu_button);
                gtk_widget_hide (priv->action_menu_button);
                gtk_widget_hide (priv->history_action);
                gtk_widget_hide (priv->zoom_action);
                gtk_widget_hide (priv->page_selector);
                gtk_widget_hide (priv->find_button);
                gtk_widget_show (priv->open_button);
                break;
        }
}

EvToolbarMode
ev_toolbar_get_mode (EvToolbar *ev_toolbar)
{
        g_return_val_if_fail (EV_IS_TOOLBAR (ev_toolbar), EV_TOOLBAR_MODE_NORMAL);

        return ev_toolbar->priv->toolbar_mode;
}
