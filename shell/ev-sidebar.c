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

enum
{
	PROP_0,
	PROP_CURRENT_PAGE,
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

struct _EvSidebarPrivate {
	GtkWidget *stack;
	GtkWidget *switcher;

	EvDocumentModel *model;
	GtkTreeModel *page_model;
};

G_DEFINE_TYPE (EvSidebar, ev_sidebar, GTK_TYPE_BOX)

#define EV_SIDEBAR_GET_PRIVATE(object) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR, EvSidebarPrivate))

static void
ev_sidebar_dispose (GObject *object)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (object);

	if (ev_sidebar->priv->page_model) {
		g_object_unref (ev_sidebar->priv->page_model);
		ev_sidebar->priv->page_model = NULL;
	}

	G_OBJECT_CLASS (ev_sidebar_parent_class)->dispose (object);
}

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
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static GtkWidget *
ev_sidebar_get_current_page (EvSidebar *ev_sidebar)
{
	return gtk_stack_get_visible_child (GTK_STACK (ev_sidebar->priv->stack));
}

static gchar *
ev_sidebar_get_visible_icon_name (EvSidebar *ev_sidebar)
{
	GtkStack *stack;
	GtkWidget *widget;
	gchar *icon_name;

	stack = GTK_STACK (ev_sidebar->priv->stack);
	widget = gtk_stack_get_visible_child (stack);
	gtk_container_child_get (GTK_CONTAINER (stack), widget,
				 "icon-name", &icon_name,
				 NULL);

	return icon_name;
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

	g_type_class_add_private (g_object_class, sizeof (EvSidebarPrivate));

	g_object_class->dispose = ev_sidebar_dispose;
	g_object_class->get_property = ev_sidebar_get_property;
	g_object_class->set_property = ev_sidebar_set_property;

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
}

static void
ev_sidebar_child_change_cb (GObject    *gobject,
			    GParamSpec *pspec,
			    EvSidebar  *ev_sidebar)
{
	GtkStack *stack = GTK_STACK (ev_sidebar->priv->stack);
	const gchar *name;

	name = gtk_stack_get_visible_child_name (stack);
	if (name)
		g_object_notify (G_OBJECT (ev_sidebar), "current-page");
}

static void
ev_sidebar_init (EvSidebar *ev_sidebar)
{
	GtkWidget *switcher;
	GtkWidget *stack;

	ev_sidebar->priv = EV_SIDEBAR_GET_PRIVATE (ev_sidebar);

	/* data model */
	ev_sidebar->priv->page_model = (GtkTreeModel *)
			gtk_list_store_new (PAGE_COLUMN_NUM_COLS,
					    G_TYPE_STRING,
					    GTK_TYPE_WIDGET,
					    G_TYPE_STRING,
					    G_TYPE_STRING);

	switcher = gtk_stack_switcher_new ();
	ev_sidebar->priv->switcher = switcher;
	gtk_box_pack_end (GTK_BOX (ev_sidebar), switcher, FALSE, TRUE, 0);
	g_object_set (switcher, "icon-size", 1, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (switcher), 6);
	gtk_widget_set_halign (switcher, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (switcher, TRUE);
	gtk_box_set_homogeneous (GTK_BOX (switcher), TRUE);
	gtk_widget_show (ev_sidebar->priv->switcher);

	stack = gtk_stack_new ();
	ev_sidebar->priv->stack = stack;
	gtk_stack_set_homogeneous (GTK_STACK (stack), TRUE);
	gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher),
				      GTK_STACK (stack));
	gtk_box_pack_end (GTK_BOX (ev_sidebar), stack, TRUE, TRUE, 0);
	gtk_widget_show (ev_sidebar->priv->stack);

	g_signal_connect (stack, "notify::visible-child",
			  G_CALLBACK (ev_sidebar_child_change_cb),
			  ev_sidebar);
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
	EvSidebarPrivate *priv = ev_sidebar->priv;
	EvDocument *document = ev_document_model_get_document (model);
	GtkTreeIter iter;
	gboolean valid;
	GtkWidget *first_supported_page = NULL;

	for (valid = gtk_tree_model_get_iter_first (priv->page_model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (priv->page_model, &iter)) {
		GtkWidget *widget;
		gchar *title;
		gchar *icon_name;

		gtk_tree_model_get (priv->page_model, &iter,
				    PAGE_COLUMN_MAIN_WIDGET, &widget,
				    PAGE_COLUMN_TITLE, &title,
				    PAGE_COLUMN_ICON_NAME, &icon_name,
				    -1);

		if (ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (widget),	document)) {
			gtk_container_child_set (GTK_CONTAINER (priv->stack),
				 widget,
				 "icon-name", icon_name,
				 "title", title,
				 NULL);
			if (!first_supported_page)
                                first_supported_page = widget;
		} else {
			/* Without icon and title, the page is not shown in
			 * the GtkStackSwitchter */
			gtk_container_child_set (GTK_CONTAINER (priv->stack),
				 widget,
				 "icon-name", NULL,
				 "title", NULL,
				 NULL);
		}
		g_object_unref (widget);
		g_free (title);
		g_free (icon_name);
	}

	if (first_supported_page != NULL) {
		if (!ev_sidebar_current_page_support_document (ev_sidebar, document)) {
			ev_sidebar_set_page (ev_sidebar, first_supported_page);
		}
	} else {
		gtk_widget_hide (GTK_WIDGET (ev_sidebar));
	}
}

/* Public functions */

GtkWidget *
ev_sidebar_new (void)
{
	GtkWidget *ev_sidebar;

	ev_sidebar = g_object_new (EV_TYPE_SIDEBAR,
                                   "orientation", GTK_ORIENTATION_VERTICAL,
				   NULL);

	return ev_sidebar;
}

void
ev_sidebar_add_page (EvSidebar   *ev_sidebar,
                     GtkWidget   *widget,
                     const gchar *name,
		     const gchar *title,
		     const gchar *icon_name)
{
	EvSidebarPrivate *priv;
	GtkTreeIter iter;

	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	priv = ev_sidebar->priv;

	ev_sidebar_page_set_model (EV_SIDEBAR_PAGE (widget), priv->model);

	gtk_stack_add_named (GTK_STACK (priv->stack), widget, name);
	gtk_container_child_set (GTK_CONTAINER (priv->stack), widget,
				 "icon-name", icon_name,
				 "title", title,
				 NULL);

	/* Insert and move to end */
	gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->page_model),
					   &iter, 0,
					   PAGE_COLUMN_NAME, name,
					   PAGE_COLUMN_MAIN_WIDGET, widget,
					   PAGE_COLUMN_TITLE, title,
					   PAGE_COLUMN_ICON_NAME, icon_name,
					   -1);
	gtk_list_store_move_before (GTK_LIST_STORE (priv->page_model),
				    &iter, NULL);
}

void
ev_sidebar_set_model (EvSidebar       *ev_sidebar,
		      EvDocumentModel *model)
{
	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (model == ev_sidebar->priv->model)
		return;

	ev_sidebar->priv->model = model;

	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_document_changed_cb),
			  ev_sidebar);
}

void
ev_sidebar_set_page (EvSidebar   *ev_sidebar,
		     GtkWidget   *main_widget)
{
	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (GTK_IS_WIDGET (main_widget));

	gtk_stack_set_visible_child (GTK_STACK (ev_sidebar->priv->stack),
				     main_widget);
	g_object_notify (G_OBJECT (ev_sidebar), "current-page");
}
