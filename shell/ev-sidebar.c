/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 *  Author:
 *    Jonathan Blandford <jrb@alum.mit.edu>
 *    Germán Poo-Caamaño <gpoo@gnome.org>
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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "ev-sidebar.h"
#include "ev-sidebar-page.h"
#include "ev-sidebar-links.h"

enum
{
	PROP_0,
	PROP_CURRENT_PAGE,
	PROP_DOCUMENT_MODEL,
	PROP_ACTIVE_ICON_NAME
};

enum
{
	PAGE_COLUMN_NAME,
	PAGE_COLUMN_MAIN_WIDGET,
	PAGE_COLUMN_TITLE,
	PAGE_COLUMN_ICON_NAME,
	PAGE_COLUMN_NUM_COLS
};

typedef struct {
	GtkWidget *stack;
	GtkWidget *switcher;

	EvDocumentModel *model;
} EvSidebarPrivate;

static void ev_sidebar_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EvSidebar, ev_sidebar, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (EvSidebar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                ev_sidebar_buildable_iface_init))

#define GET_PRIVATE(o) ev_sidebar_get_instance_private (o)

static void
ev_sidebar_set_property (GObject      *object,
		         guint         prop_id,
		         const GValue *value,
		         GParamSpec   *pspec)
{
	EvSidebar *sidebar = EV_SIDEBAR (object);

	switch (prop_id)
	{
	case PROP_CURRENT_PAGE:
		ev_sidebar_set_page (sidebar, g_value_get_object (value));
		break;
	case PROP_DOCUMENT_MODEL:
		ev_sidebar_set_model (sidebar,
			EV_DOCUMENT_MODEL (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

GtkWidget *
ev_sidebar_get_current_page (EvSidebar *ev_sidebar)
{
	EvSidebarPrivate *priv = GET_PRIVATE (ev_sidebar);

	return gtk_stack_get_visible_child (GTK_STACK (priv->stack));
}

static gchar *
ev_sidebar_get_visible_icon_name (EvSidebar *ev_sidebar)
{
	GtkStack *stack;
	GtkWidget *widget;
	gchar *icon_name;
	EvSidebarPrivate *priv = GET_PRIVATE (ev_sidebar);

	stack = GTK_STACK (priv->stack);
	widget = gtk_stack_get_visible_child (stack);

	g_object_get (gtk_stack_get_page (stack, widget),
				 "icon-name", &icon_name,
				 NULL);
	return icon_name;
}

static void
on_child_change (GObject    *gobject,
			    GParamSpec *pspec,
			    EvSidebar  *ev_sidebar)
{
	EvSidebarPrivate *priv = GET_PRIVATE (ev_sidebar);
	GtkStack *stack = GTK_STACK (priv->stack);
	const gchar *name;

	name = gtk_stack_get_visible_child_name (stack);
	if (name)
		g_object_notify (G_OBJECT (ev_sidebar), "current-page");
}

static void
ev_sidebar_get_property (GObject *object,
		         guint prop_id,
		         GValue *value,
		         GParamSpec *pspec)
{
	EvSidebar *sidebar = EV_SIDEBAR (object);
	gchar *icon_name;

	switch (prop_id)
	{
	case PROP_CURRENT_PAGE:
		g_value_set_object (value, ev_sidebar_get_current_page (sidebar));
		break;
	case PROP_ACTIVE_ICON_NAME:
		icon_name = ev_sidebar_get_visible_icon_name (sidebar);
		g_value_set_string (value, icon_name);
		g_free (icon_name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_sidebar_class_init (EvSidebarClass *ev_sidebar_class)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (ev_sidebar_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (ev_sidebar_class);

	g_object_class->get_property = ev_sidebar_get_property;
	g_object_class->set_property = ev_sidebar_set_property;

	g_type_ensure (EV_TYPE_SIDEBAR_LINKS);
	gtk_widget_class_set_template_from_resource (widget_class,
			"/org/gnome/evince/ui/sidebar.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebar, switcher);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebar, stack);

	gtk_widget_class_bind_template_callback (widget_class, on_child_change);

	g_object_class_install_property (g_object_class,
					 PROP_CURRENT_PAGE,
					 g_param_spec_object ("current-page",
							      "Current page",
							      "The currently visible page",
							      GTK_TYPE_WIDGET,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_ACTIVE_ICON_NAME,
					 g_param_spec_string ("active-icon-name",
							      "Current page",
							      "The icon name of the currently visible page",
							      NULL,
							      G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

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
ev_sidebar_init (EvSidebar *ev_sidebar)
{
	GtkWidget *widget = GTK_WIDGET (ev_sidebar);

	gtk_widget_init_template (widget);
}


static gboolean
ev_sidebar_current_page_support_document (EvSidebar  *ev_sidebar,
                                          EvDocument *document)
{
	GtkWidget *current_page = ev_sidebar_get_current_page (ev_sidebar);

	return ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (current_page),
						 document);
}


static void
ev_sidebar_document_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvSidebar       *ev_sidebar)
{
	EvSidebarPrivate *priv = GET_PRIVATE (ev_sidebar);
	EvDocument *document = ev_document_model_get_document (model);
	GListModel *list = G_LIST_MODEL (gtk_stack_get_pages (GTK_STACK (priv->stack)));
	EvSidebarPage *first_supported_page = NULL, *sidebar_page;
	guint i = 0;
	GtkStackPage *page;
	gboolean supported;

	while ((page = g_list_model_get_item (list, i)) != NULL) {
		sidebar_page = EV_SIDEBAR_PAGE (gtk_stack_page_get_child (page));

		supported = ev_sidebar_page_support_document (sidebar_page, document);
		gtk_stack_page_set_visible (page, supported);

		if (supported && !first_supported_page)
			first_supported_page = sidebar_page;

		i++;
	}

	if (first_supported_page != NULL) {
		if (!ev_sidebar_current_page_support_document (ev_sidebar, document)) {
			ev_sidebar_set_page (ev_sidebar, GTK_WIDGET (first_supported_page));
		}
	} else {
		gtk_widget_hide (GTK_WIDGET (ev_sidebar));
	}
}

static GtkBuildableIface *parent_buildable_iface;

static GObject *
ev_sidebar_buildable_get_internal_child (GtkBuildable *buildable,
                             GtkBuilder   *builder,
                             const char   *childname)
{
        EvSidebar *sidebar = EV_SIDEBAR (buildable);
	EvSidebarPrivate *priv = GET_PRIVATE (sidebar);

        if (strcmp (childname, "stack") == 0)
                return G_OBJECT (priv->stack);

        return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
ev_sidebar_buildable_iface_init (GtkBuildableIface *iface)
{
        parent_buildable_iface = g_type_interface_peek_parent (iface);

        iface->get_internal_child = ev_sidebar_buildable_get_internal_child;
}

/* Public functions */

GtkWidget *
ev_sidebar_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR, NULL));
}

void
ev_sidebar_add_page (EvSidebar   *ev_sidebar,
                     GtkWidget   *widget,
                     const gchar *name,
		     const gchar *title,
		     const gchar *icon_name)
{
	EvSidebarPrivate *priv = GET_PRIVATE (ev_sidebar);
	GtkStackPage *page;
	GtkStack *stack = GTK_STACK (priv->stack);

	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	ev_sidebar_page_set_model (EV_SIDEBAR_PAGE (widget), priv->model);

	gtk_stack_add_named (stack, widget, name);
	page = gtk_stack_get_page (stack, widget);
	g_object_set (page, "icon-name", icon_name, "title", title, NULL);
}

void
ev_sidebar_set_model (EvSidebar       *ev_sidebar,
		      EvDocumentModel *model)
{
	EvSidebarPrivate *priv;

	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	priv = GET_PRIVATE (ev_sidebar);

	if (model == priv->model)
		return;

	if (priv->model) {
		g_signal_handlers_disconnect_by_func (priv->model,
			G_CALLBACK (ev_sidebar_document_changed_cb), ev_sidebar);
		g_object_unref (priv->model);
	}

	priv->model = g_object_ref (model);
	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_document_changed_cb),
			  ev_sidebar);
}

void
ev_sidebar_set_page (EvSidebar   *ev_sidebar,
		     GtkWidget   *main_widget)
{
	EvSidebarPrivate *priv;

	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (GTK_IS_WIDGET (main_widget));

	priv = GET_PRIVATE (ev_sidebar);

	gtk_stack_set_visible_child (GTK_STACK (priv->stack),
				     main_widget);
	g_object_notify (G_OBJECT (ev_sidebar), "current-page");
}
