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
ev_previewer_toolbar_set_button_action (EvPreviewerToolbar   *ev_previewer_toolbar,
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
ev_previewer_toolbar_create_button (EvPreviewerToolbar   *ev_previewer_toolbar,
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
        ev_previewer_toolbar_set_button_action (ev_previewer_toolbar, GTK_BUTTON (button), action_name, tooltip);

        return button;
}

static void
ev_previewer_toolbar_constructed (GObject *object)
{
        EvPreviewerToolbar *ev_previewer_toolbar = EV_PREVIEWER_TOOLBAR (object);
        GtkWidget     *tool_item;
        GtkWidget     *button;
        GtkWidget     *hbox;
        GtkAccelGroup *accel_group;

        /* Shortcuts, just for reference.
	const gchar *action_accels[] = {
		"preview.select-page",      "<Ctrl>L", NULL,
		"preview.go-previous-page", "p", "<Ctrl>Page_Up", NULL,
		"preview.go-next-page",     "n", "<Ctrl>Page_Down", NULL,
		"preview.print",            "<Ctrl>P", NULL,
		"preview.zoom-in",          "plus", "<Ctrl>plus", "KP_Add", "<Ctrl>KP_Add", "equal", "<Ctrl>equal", NULL,
		"preview.zoom-out",         "minus", "<Ctrl>minus", "KP_Subtract", "<Ctrl>KP_Subtract", NULL,
		"preview.zoom-default",     "a", NULL,
		NULL,
	};
         */

        G_OBJECT_CLASS (ev_previewer_toolbar_parent_class)->constructed (object);

        accel_group = gtk_accel_group_new ();
        gtk_window_add_accel_group (GTK_WINDOW (ev_previewer_toolbar->priv->window),
                                    accel_group);

        hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_EXPAND);
	gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);

        button = ev_previewer_toolbar_create_button (ev_previewer_toolbar,
                                                     "preview.go-previous-page",
                                                     "go-previous-symbolic",
                                                     NULL,
                                                     _("Previous Page"));
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_p, 0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        ev_previewer_toolbar->priv->previous_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        button = ev_previewer_toolbar_create_button (ev_previewer_toolbar,
                                                     "preview.go-next-page",
                                                     "go-next-symbolic",
                                                     NULL,
                                                     _("Next Page"));
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_n, 0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        ev_previewer_toolbar->priv->next_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        gtk_header_bar_pack_start (GTK_HEADER_BAR (ev_previewer_toolbar), hbox);
        gtk_widget_show (hbox);

        /* Page selector */
        /* Use EvPageActionWidget for now, since the page selector action is also used by the previewer */
        tool_item = GTK_WIDGET (g_object_new (EV_TYPE_PAGE_ACTION_WIDGET, NULL));
        gtk_widget_set_tooltip_text (tool_item, _("Select page or search in the index"));
        atk_object_set_name (gtk_widget_get_accessible (tool_item), _("Select page"));
        /* FIXME: Add accelerator for grabing the focus in the EvPageActionWidget's entry */
        ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (tool_item),
                                         ev_previewer_window_get_document_model (ev_previewer_toolbar->priv->window));
        ev_previewer_toolbar->priv->page_selector = tool_item;
        gtk_header_bar_pack_start (GTK_HEADER_BAR (ev_previewer_toolbar), tool_item);
        gtk_widget_show (tool_item);

        /* Print */
        button = ev_previewer_toolbar_create_button (ev_previewer_toolbar,
                                                     "preview.print",
                                                     NULL,
                                                     _("Print"),
                                                     _("Print this document"));
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_p, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        ev_previewer_toolbar->priv->print_button = button;
        gtk_header_bar_pack_end (GTK_HEADER_BAR (ev_previewer_toolbar), button);
        gtk_widget_show (button);

        /* Zoom */
        hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_EXPAND);
	gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);

        button = ev_previewer_toolbar_create_button (ev_previewer_toolbar,
                                                     "preview.zoom-in",
                                                     "zoom-in-symbolic",
                                                     NULL,
                                                     _("Enlarge the document"));
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_plus, 0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_plus, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_KP_Add, 0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_KP_Add, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_equal, 0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_equal, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_plus, 0, GTK_ACCEL_VISIBLE);
        ev_previewer_toolbar->priv->zoom_in_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        button = ev_previewer_toolbar_create_button (ev_previewer_toolbar,
                                                     "preview.zoom-default",
                                                     "zoom-fit-best-symbolic",
                                                     NULL,
                                                     _("Reset zoom and make the page fit in the window"));
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_a, 0, GTK_ACCEL_VISIBLE);
        ev_previewer_toolbar->priv->zoom_default_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        button = ev_previewer_toolbar_create_button (ev_previewer_toolbar,
                                                     "preview.zoom-out",
                                                     "zoom-out-symbolic",
                                                     NULL,
                                                     _("Shrink the document"));
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_minus, 0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_minus, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_KP_Subtract, 0, GTK_ACCEL_VISIBLE);
        gtk_widget_add_accelerator (button, "activate", accel_group,
                                    GDK_KEY_KP_Subtract, GDK_CONTROL_MASK,
                                    GTK_ACCEL_VISIBLE);
        ev_previewer_toolbar->priv->zoom_out_button = button;
        gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_widget_show (button);

        gtk_header_bar_pack_end (GTK_HEADER_BAR (ev_previewer_toolbar), hbox);
        gtk_widget_show (hbox);
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
