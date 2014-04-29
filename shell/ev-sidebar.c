/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Author:
 *    Jonathan Blandford <jrb@alum.mit.edu>
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "ev-sidebar.h"
#include "ev-sidebar-page.h"

enum
{
	PROP_0,
	PROP_CURRENT_PAGE
};

enum
{
	PAGE_COLUMN_TITLE,
	PAGE_COLUMN_MENU_ITEM,
	PAGE_COLUMN_MAIN_WIDGET,
	PAGE_COLUMN_NOTEBOOK_INDEX,
	PAGE_COLUMN_NUM_COLS
};

struct _EvSidebarPrivate {
	GtkWidget *notebook;
	GtkWidget *select_button;
	GtkWidget *menu;
	GtkWidget *hbox;
	GtkWidget *label;

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

	if (ev_sidebar->priv->menu) {
		gtk_menu_detach (GTK_MENU (ev_sidebar->priv->menu));
		ev_sidebar->priv->menu = NULL;
	}
	
	if (ev_sidebar->priv->page_model) {
		g_object_unref (ev_sidebar->priv->page_model);
		ev_sidebar->priv->page_model = NULL;
	}
		
	   
	G_OBJECT_CLASS (ev_sidebar_parent_class)->dispose (object);
}

static void
ev_sidebar_select_page (EvSidebar *ev_sidebar,  GtkTreeIter *iter)
{
	char *title;
	int index;

	gtk_tree_model_get (ev_sidebar->priv->page_model, iter,
			    PAGE_COLUMN_TITLE, &title, 
			    PAGE_COLUMN_NOTEBOOK_INDEX, &index,
			    -1);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (ev_sidebar->priv->notebook), index);
	gtk_label_set_text (GTK_LABEL (ev_sidebar->priv->label), title);

	g_free (title);
}

void
ev_sidebar_set_page (EvSidebar   *ev_sidebar,
		     GtkWidget   *main_widget)
{
	GtkTreeIter iter;
	gboolean valid;

	valid = gtk_tree_model_get_iter_first (ev_sidebar->priv->page_model, &iter);
	   
	while (valid) {
		GtkWidget *widget;

		gtk_tree_model_get (ev_sidebar->priv->page_model, &iter,
				    PAGE_COLUMN_MAIN_WIDGET, &widget,
				    -1);
			 
		if (widget == main_widget) {
			ev_sidebar_select_page (ev_sidebar, &iter);
			valid = FALSE;
		} else {
			valid = gtk_tree_model_iter_next (ev_sidebar->priv->page_model, &iter);
		}
		g_object_unref (widget);
	}

	g_object_notify (G_OBJECT (ev_sidebar), "current-page");
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
ev_sidebar_get_current_page (EvSidebar *sidebar)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (sidebar->priv->notebook);

	return gtk_notebook_get_nth_page
		(notebook, gtk_notebook_get_current_page (notebook));
}

static void
ev_sidebar_get_property (GObject *object,
		         guint prop_id,
		         GValue *value,
		         GParamSpec *pspec)
{
	EvSidebar *sidebar = EV_SIDEBAR (object);

	switch (prop_id)
	{
	case PROP_CURRENT_PAGE:
		g_value_set_object (value, ev_sidebar_get_current_page (sidebar));
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
}

static void
ev_sidebar_menu_position_under (GtkMenu  *menu,
				int      *x,
				int      *y,
				gboolean *push_in,
				gpointer  user_data)
{
	GtkWidget    *widget;
	GtkAllocation allocation;

	g_return_if_fail (GTK_IS_BUTTON (user_data));
	g_return_if_fail (!gtk_widget_get_has_window (GTK_WIDGET (user_data)));

	widget = GTK_WIDGET (user_data);
	   
	gdk_window_get_origin (gtk_widget_get_window (widget), x, y);
	gtk_widget_get_allocation (widget, &allocation);
	   
	*x += allocation.x;
	*y += allocation.y + allocation.height;
	   
	*push_in = FALSE;
}

static gboolean
ev_sidebar_select_button_press_cb (GtkWidget      *widget,
				   GdkEventButton *event,
				   gpointer        user_data)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (user_data);

	if (event->button == 1) {
		GtkRequisition requisition;
		GtkAllocation allocation;
		gint width;

		gtk_widget_get_allocation (widget, &allocation);
		width = allocation.width;
		gtk_widget_set_size_request (ev_sidebar->priv->menu, -1, -1);
                gtk_widget_get_preferred_size (ev_sidebar->priv->menu, &requisition, NULL);
		gtk_widget_set_size_request (ev_sidebar->priv->menu,
					     MAX (width, requisition.width), -1);
		
		gtk_widget_grab_focus (widget);
			 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
		gtk_menu_popup (GTK_MENU (ev_sidebar->priv->menu),
				NULL, NULL, ev_sidebar_menu_position_under, widget,
				event->button, event->time);

		return TRUE;
	}

	return FALSE;
}

static gboolean
ev_sidebar_select_button_key_press_cb (GtkWidget   *widget,
				       GdkEventKey *event,
				       gpointer     user_data)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (user_data);
	   
	if (event->keyval == GDK_KEY_space ||
	    event->keyval == GDK_KEY_KP_Space ||
	    event->keyval == GDK_KEY_Return ||
	    event->keyval == GDK_KEY_KP_Enter) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
		gtk_menu_popup (GTK_MENU (ev_sidebar->priv->menu),
			        NULL, NULL, ev_sidebar_menu_position_under, widget,
				1, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
ev_sidebar_close_clicked_cb (GtkWidget *widget,
			     gpointer   user_data)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (user_data);

	gtk_widget_hide (GTK_WIDGET (ev_sidebar));
}

static void
ev_sidebar_menu_deactivate_cb (GtkWidget *widget,
			       gpointer   user_data)
{
	GtkWidget *menu_button;

	menu_button = GTK_WIDGET (user_data);
	   
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (menu_button), FALSE);
}

static void
ev_sidebar_menu_detach_cb (GtkWidget *widget,
			   GtkMenu   *menu)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (widget);
	   
	ev_sidebar->priv->menu = NULL;
}

static void
ev_sidebar_menu_item_activate_cb (GtkWidget *widget,
				  gpointer   user_data)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (user_data);
	GtkTreeIter iter;
	GtkWidget *menu_item, *item;
	gboolean valid;

	menu_item = gtk_menu_get_active (GTK_MENU (ev_sidebar->priv->menu));
	valid = gtk_tree_model_get_iter_first (ev_sidebar->priv->page_model, &iter);
	   
	while (valid) {
		gtk_tree_model_get (ev_sidebar->priv->page_model, &iter,
				    PAGE_COLUMN_MENU_ITEM, &item,
				    -1);
			 
		if (item == menu_item) {
			ev_sidebar_select_page (ev_sidebar, &iter);
			valid = FALSE;
		} else {
			valid = gtk_tree_model_iter_next (ev_sidebar->priv->page_model, &iter);
		}
		g_object_unref (item);
	}

	g_object_notify (G_OBJECT (ev_sidebar), "current-page");
}

static void
ev_sidebar_init (EvSidebar *ev_sidebar)
{
	GtkWidget *hbox;
	GtkWidget *close_button;
	GtkWidget *select_hbox;
	GtkWidget *arrow;
	GtkWidget *image;

	ev_sidebar->priv = EV_SIDEBAR_GET_PRIVATE (ev_sidebar);

	/* data model */
	ev_sidebar->priv->page_model = (GtkTreeModel *)
			gtk_list_store_new (PAGE_COLUMN_NUM_COLS,
					    G_TYPE_STRING,
					    GTK_TYPE_WIDGET,
					    GTK_TYPE_WIDGET,
					    G_TYPE_INT);

	/* top option menu */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	ev_sidebar->priv->hbox = hbox;
	gtk_box_pack_start (GTK_BOX (ev_sidebar), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	ev_sidebar->priv->select_button = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (ev_sidebar->priv->select_button), GTK_RELIEF_NONE);
	g_signal_connect (ev_sidebar->priv->select_button, "button_press_event",
			  G_CALLBACK (ev_sidebar_select_button_press_cb),
			  ev_sidebar);
	g_signal_connect (ev_sidebar->priv->select_button, "key_press_event",
			  G_CALLBACK (ev_sidebar_select_button_key_press_cb),
			  ev_sidebar);

	select_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	ev_sidebar->priv->label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (select_hbox),
			    ev_sidebar->priv->label,
			    FALSE, FALSE, 0);
	gtk_widget_show (ev_sidebar->priv->label);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_end (GTK_BOX (select_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_show (arrow);

	gtk_container_add (GTK_CONTAINER (ev_sidebar->priv->select_button), select_hbox);
	gtk_widget_show (select_hbox);

	gtk_box_pack_start (GTK_BOX (hbox), ev_sidebar->priv->select_button, TRUE, TRUE, 0);
	gtk_widget_show (ev_sidebar->priv->select_button);

	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
	g_signal_connect (close_button, "clicked",
			  G_CALLBACK (ev_sidebar_close_clicked_cb),
			  ev_sidebar);
	   
	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE,
					  GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_widget_show (image);
   
	gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
	gtk_widget_show (close_button);
   
	ev_sidebar->priv->menu = gtk_menu_new ();
	g_signal_connect (ev_sidebar->priv->menu, "deactivate",
			  G_CALLBACK (ev_sidebar_menu_deactivate_cb),
			  ev_sidebar->priv->select_button);
	gtk_menu_attach_to_widget (GTK_MENU (ev_sidebar->priv->menu),
				   GTK_WIDGET (ev_sidebar),
				   ev_sidebar_menu_detach_cb);
	gtk_widget_show (ev_sidebar->priv->menu);

	ev_sidebar->priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (ev_sidebar->priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (ev_sidebar->priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (ev_sidebar), ev_sidebar->priv->notebook,
			    TRUE, TRUE, 0);
	gtk_widget_show (ev_sidebar->priv->notebook);

	gtk_widget_set_sensitive (GTK_WIDGET (ev_sidebar->priv->notebook), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (ev_sidebar->priv->select_button), FALSE);
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
		     GtkWidget   *main_widget)
{
	GtkTreeIter iter;
	GtkWidget *menu_item;
	gchar *label_title;
	const gchar *title;
	int index;
	   
	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (EV_IS_SIDEBAR_PAGE (main_widget));
	g_return_if_fail (GTK_IS_WIDGET (main_widget));

	ev_sidebar_page_set_model (EV_SIDEBAR_PAGE (main_widget),
				   ev_sidebar->priv->model);
	title = ev_sidebar_page_get_label (EV_SIDEBAR_PAGE (main_widget));
	   
	index = gtk_notebook_append_page (GTK_NOTEBOOK (ev_sidebar->priv->notebook),
					  main_widget, NULL);
	   
	menu_item = gtk_image_menu_item_new_with_label (title);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (ev_sidebar_menu_item_activate_cb),
			  ev_sidebar);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (ev_sidebar->priv->menu),
			       menu_item);

	/* Insert and move to end */
	gtk_list_store_insert_with_values (GTK_LIST_STORE (ev_sidebar->priv->page_model),
					   &iter, 0,
					   PAGE_COLUMN_TITLE, title,
					   PAGE_COLUMN_MENU_ITEM, menu_item,
					   PAGE_COLUMN_MAIN_WIDGET, main_widget,
					   PAGE_COLUMN_NOTEBOOK_INDEX, index,
					   -1);
	gtk_list_store_move_before(GTK_LIST_STORE(ev_sidebar->priv->page_model),
					   &iter, NULL);


	/* Set the first item added as active */
	gtk_tree_model_get_iter_first (ev_sidebar->priv->page_model, &iter);
	gtk_tree_model_get (ev_sidebar->priv->page_model,
			    &iter,
			    PAGE_COLUMN_TITLE, &label_title,
			    PAGE_COLUMN_NOTEBOOK_INDEX, &index,
			    -1);

	gtk_menu_set_active (GTK_MENU (ev_sidebar->priv->menu), index);
	gtk_label_set_text (GTK_LABEL (ev_sidebar->priv->label), label_title);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (ev_sidebar->priv->notebook),
				       index);
	g_free (label_title);
}

static gboolean
ev_sidebar_current_page_support_document (EvSidebar  *sidebar,
                                          EvDocument *document)
{
	GtkWidget *current_page = ev_sidebar_get_current_page (sidebar);

	return ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (current_page), document);
}


static void
ev_sidebar_document_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvSidebar       *sidebar)
{
	EvSidebarPrivate *priv = sidebar->priv;
	EvDocument *document = ev_document_model_get_document (model);
	GtkTreeIter iter;
	gboolean valid;
	GtkWidget *first_supported_page = NULL;

	for (valid = gtk_tree_model_get_iter_first (priv->page_model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (priv->page_model, &iter)) {
		GtkWidget *widget;
		GtkWidget *menu_widget;

		gtk_tree_model_get (priv->page_model, &iter,
				    PAGE_COLUMN_MAIN_WIDGET, &widget,
				    PAGE_COLUMN_MENU_ITEM, &menu_widget,
				    -1);

		if (ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (widget),	document)) {
			gtk_widget_set_sensitive (menu_widget, TRUE);
			if (!first_supported_page)
                                first_supported_page = widget;
		} else {
			gtk_widget_set_sensitive (menu_widget, FALSE);
		}
		g_object_unref (widget);
		g_object_unref (menu_widget);
	}

	if (first_supported_page != NULL) {
		if (!ev_sidebar_current_page_support_document (sidebar, document)) {
			ev_sidebar_set_page (sidebar, first_supported_page);
		}
		gtk_widget_set_sensitive (GTK_WIDGET (sidebar->priv->notebook), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (sidebar->priv->select_button), TRUE);
	} else {
		gtk_widget_hide (GTK_WIDGET (sidebar));
	}
}

void
ev_sidebar_set_model (EvSidebar       *sidebar,
		      EvDocumentModel *model)
{
	g_return_if_fail (EV_IS_SIDEBAR (sidebar));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (model == sidebar->priv->model)
		return;

	sidebar->priv->model = model;
	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_document_changed_cb),
			  sidebar);
}
