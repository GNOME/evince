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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "ev-sidebar.h"
#include "ev-document-thumbnails.h"
#include "ev-document-links.h"
#include "ev-sidebar-links.h"
#include "ev-sidebar-thumbnails.h"

typedef struct
{
	char *id;
	char *title;
	GtkWidget *main_widget;
} EvSidebarPage;

enum
{
	PAGE_COLUMN_ID,
	PAGE_COLUMN_TITLE,
	PAGE_COLUMN_MENU_ITEM,
	PAGE_COLUMN_MAIN_WIDGET,
	PAGE_COLUMN_NOTEBOOK_INDEX,
	PAGE_COLUMN_NUM_COLS
};

struct _EvSidebarPrivate {
	GtkWidget *notebook;
	GtkWidget *menu;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *label;
	   
	GtkTreeModel *page_model;
};

G_DEFINE_TYPE (EvSidebar, ev_sidebar, GTK_TYPE_VBOX)

#define EV_SIDEBAR_GET_PRIVATE(object) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR, EvSidebarPrivate))

static void
ev_sidebar_destroy (GtkObject *object)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (object);

	if (ev_sidebar->priv->menu) {
		gtk_menu_detach (GTK_MENU (ev_sidebar->priv->menu));
		ev_sidebar->priv->menu = NULL;
	}
	   
	(* GTK_OBJECT_CLASS (ev_sidebar_parent_class)->destroy) (object);
}

static void
ev_sidebar_class_init (EvSidebarClass *ev_sidebar_class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;
	GtkObjectClass *gtk_object_klass;
 
	g_object_class = G_OBJECT_CLASS (ev_sidebar_class);
	widget_class = GTK_WIDGET_CLASS (ev_sidebar_class);
	gtk_object_klass = GTK_OBJECT_CLASS (ev_sidebar_class);
	   
	g_type_class_add_private (g_object_class, sizeof (EvSidebarPrivate));
	   
	gtk_object_klass->destroy = ev_sidebar_destroy;
}

static void
ev_sidebar_menu_position_under (GtkMenu  *menu,
				int      *x,
				int      *y,
				gboolean *push_in,
				gpointer  user_data)
{
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_BUTTON (user_data));
	g_return_if_fail (GTK_WIDGET_NO_WINDOW (user_data));

	widget = GTK_WIDGET (user_data);
	   
	gdk_window_get_origin (widget->window, x, y);
	   
	*x += widget->allocation.x;
	*y += widget->allocation.y + widget->allocation.height;
	   
	*push_in = FALSE;
}

static gboolean
ev_sidebar_select_button_press_cb (GtkWidget      *widget,
				   GdkEventButton *event,
				   gpointer        user_data)
{
	EvSidebar *ev_sidebar = EV_SIDEBAR (user_data);

	if (event->button == 1) {
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
	   
	if (event->keyval == GDK_space ||
	    event->keyval == GDK_KP_Space ||
	    event->keyval == GDK_Return ||
	    event->keyval == GDK_KP_Enter) {
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
	gchar *title;
	gboolean valid;
	gint index;

	menu_item = gtk_menu_get_active (GTK_MENU (ev_sidebar->priv->menu));
	valid = gtk_tree_model_get_iter_first (ev_sidebar->priv->page_model, &iter);
	   
	while (valid) {
		gtk_tree_model_get (ev_sidebar->priv->page_model,
				    &iter,
				    PAGE_COLUMN_TITLE, &title, 
				    PAGE_COLUMN_MENU_ITEM, &item,
				    PAGE_COLUMN_NOTEBOOK_INDEX, &index,
				    -1);
			 
		if (item == menu_item) {
			gtk_notebook_set_current_page
				(GTK_NOTEBOOK (ev_sidebar->priv->notebook), index);
			gtk_label_set_text (GTK_LABEL (ev_sidebar->priv->label), title);
			valid = FALSE;
		} else {
			valid = gtk_tree_model_iter_next (ev_sidebar->priv->page_model, &iter);
		}
	}
}

static void
ev_sidebar_init (EvSidebar *ev_sidebar)
{
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *close_button;
	GtkWidget *select_button;
	GtkWidget *select_hbox;
	GtkWidget *arrow;
	GtkWidget *image;

	ev_sidebar->priv = EV_SIDEBAR_GET_PRIVATE (ev_sidebar);

	/* data model */
	ev_sidebar->priv->page_model = (GtkTreeModel *)
			gtk_list_store_new (PAGE_COLUMN_NUM_COLS,
					    G_TYPE_STRING,
					    G_TYPE_STRING,
					    GTK_TYPE_WIDGET,
					    GTK_TYPE_WIDGET,
					    G_TYPE_INT);

	/* top option menu */
	frame = gtk_frame_new (NULL);
	ev_sidebar->priv->frame = frame;
	gtk_box_pack_start (GTK_BOX (ev_sidebar), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	hbox = gtk_hbox_new (FALSE, 0);
	ev_sidebar->priv->hbox = hbox;
	gtk_container_add (GTK_CONTAINER (frame), hbox);
	gtk_widget_show (hbox);

	select_button = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (select_button), GTK_RELIEF_NONE);
	g_signal_connect (select_button, "button_press_event",
			  G_CALLBACK (ev_sidebar_select_button_press_cb),
			  ev_sidebar);
	g_signal_connect (select_button, "key_press_event",
			  G_CALLBACK (ev_sidebar_select_button_key_press_cb),
			  ev_sidebar);

	select_hbox = gtk_hbox_new (FALSE, 0);

	ev_sidebar->priv->label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (select_hbox),
			    ev_sidebar->priv->label,
			    FALSE, FALSE, 0);
	gtk_widget_show (ev_sidebar->priv->label);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_end (GTK_BOX (select_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_show (arrow);

	gtk_container_add (GTK_CONTAINER (select_button), select_hbox);
	gtk_widget_show (select_hbox);

	gtk_box_pack_start (GTK_BOX (hbox), select_button, TRUE, TRUE, 0);
	gtk_widget_show (select_button);

	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
	g_signal_connect (close_button, "clicked",
			  G_CALLBACK (ev_sidebar_close_clicked_cb),
			  ev_sidebar);
	   
	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE,
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_widget_show (image);
   
	gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
	gtk_widget_show (close_button);
   
	ev_sidebar->priv->menu = gtk_menu_new ();
	g_signal_connect (ev_sidebar->priv->menu, "deactivate",
			  G_CALLBACK (ev_sidebar_menu_deactivate_cb),
			  select_button);
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
}

/* Public functions */

GtkWidget *
ev_sidebar_new (void)
{
	GtkWidget *ev_sidebar;

	ev_sidebar = g_object_new (EV_TYPE_SIDEBAR, NULL);

	return ev_sidebar;
}

void
ev_sidebar_add_page (EvSidebar   *ev_sidebar,
		     const gchar *page_id,
		     const gchar *title,
		     GtkWidget   *main_widget)
{
	GtkTreeIter iter;
	GtkWidget *menu_item;
	gchar *label_title;
	int index;
	   
	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (page_id != NULL);
	g_return_if_fail (title != NULL);
	g_return_if_fail (GTK_IS_WIDGET (main_widget));
	   
	index = gtk_notebook_append_page (GTK_NOTEBOOK (ev_sidebar->priv->notebook),
					  main_widget, NULL);
	   
	menu_item = gtk_image_menu_item_new_with_label (title);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (ev_sidebar_menu_item_activate_cb),
			  ev_sidebar);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (ev_sidebar->priv->menu),
			       menu_item);
	   
	gtk_list_store_insert_with_values (GTK_LIST_STORE (ev_sidebar->priv->page_model),
					   &iter, 0,
					   PAGE_COLUMN_ID, page_id,
					   PAGE_COLUMN_TITLE, title,
					   PAGE_COLUMN_MENU_ITEM, menu_item,
					   PAGE_COLUMN_MAIN_WIDGET, main_widget,
					   PAGE_COLUMN_NOTEBOOK_INDEX, index,
					   -1);
	   
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
}

void
ev_sidebar_set_document (EvSidebar   *sidebar,
			 EvDocument  *document)
{
	EvSidebarPrivate *priv;
	GtkTreeIter iter;
	gboolean result;
	   
	g_return_if_fail (EV_IS_SIDEBAR (sidebar));
	g_return_if_fail (EV_IS_DOCUMENT (document));
	   
	priv = sidebar->priv;
	
	/* FIXME: We should prolly make sidebars have an interface.  For now, we	
	 * do this bad hack (TM)	
	 */
	for (result = gtk_tree_model_get_iter_first (priv->page_model, &iter);
	     result;
	     result = gtk_tree_model_iter_next (priv->page_model, &iter)) {
		GtkWidget *widget;

		gtk_tree_model_get (priv->page_model, &iter,
				    PAGE_COLUMN_MAIN_WIDGET, &widget,
				    -1);
			 
		if (EV_IS_SIDEBAR_LINKS (widget)
		    && EV_IS_DOCUMENT_LINKS (document)
		    && ev_document_links_has_document_links (EV_DOCUMENT_LINKS (document))) {
			ev_sidebar_links_set_document (EV_SIDEBAR_LINKS (widget), document);
		} else if (EV_IS_SIDEBAR_THUMBNAILS (widget) &&
			   EV_IS_DOCUMENT_THUMBNAILS (document)) {
			ev_sidebar_thumbnails_set_document
				(EV_SIDEBAR_THUMBNAILS (widget), document);
		}
	}
}
