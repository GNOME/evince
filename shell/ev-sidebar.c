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
	PAGE_COLUMN_MAIN_WIDGET,
	PAGE_COLUMN_NOTEBOOK_INDEX,
	PAGE_COLUMN_NUM_COLS
};

struct _EvSidebarPrivate {
	GtkWidget *option_menu;
	GtkWidget *notebook;

	GtkTreeModel *page_model;
};

static void ev_sidebar_omenu_changed_cb (GtkComboBox *combo_box,
					 gpointer     user_data);

G_DEFINE_TYPE (EvSidebar, ev_sidebar, GTK_TYPE_VBOX)

#define EV_SIDEBAR_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR, EvSidebarPrivate))

static void
ev_sidebar_class_init (EvSidebarClass *ev_sidebar_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_class);

	g_type_class_add_private (g_object_class, sizeof (EvSidebarPrivate));

}

static void
ev_sidebar_init (EvSidebar *ev_sidebar)
{
	GtkWidget *hbox;
	GtkCellRenderer *renderer;
	
	ev_sidebar->priv = EV_SIDEBAR_GET_PRIVATE (ev_sidebar);
	gtk_box_set_spacing (GTK_BOX (ev_sidebar), 6);

	/* data model */
	ev_sidebar->priv->page_model = (GtkTreeModel *)
		gtk_list_store_new (PAGE_COLUMN_NUM_COLS,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    GTK_TYPE_WIDGET,
				    G_TYPE_INT);

	/* top option menu */
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (ev_sidebar), hbox,
			    FALSE, FALSE, 0);
	ev_sidebar->priv->option_menu =
		gtk_combo_box_new_with_model (ev_sidebar->priv->page_model);
	g_signal_connect (ev_sidebar->priv->option_menu, "changed",
			  G_CALLBACK (ev_sidebar_omenu_changed_cb), ev_sidebar);
	gtk_box_pack_start (GTK_BOX (hbox),
			    ev_sidebar->priv->option_menu,
			    FALSE, FALSE, 0);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ev_sidebar->priv->option_menu),
				    renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (ev_sidebar->priv->option_menu),
					renderer,
					"text", PAGE_COLUMN_TITLE,
					NULL);

	ev_sidebar->priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (ev_sidebar->priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (ev_sidebar->priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (ev_sidebar), ev_sidebar->priv->notebook,
			    TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (ev_sidebar));
}

static void
ev_sidebar_omenu_changed_cb (GtkComboBox *combo_box,
			     gpointer     user_data)
{
	GtkTreeIter iter;
	EvSidebar *ev_sidebar = EV_SIDEBAR (user_data);

	if (gtk_combo_box_get_active_iter (combo_box, &iter)) {
		gint index;

		gtk_tree_model_get (ev_sidebar->priv->page_model,
				    &iter,
				    PAGE_COLUMN_NOTEBOOK_INDEX, &index,
				    -1);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (ev_sidebar->priv->notebook),
					       index);
					       
	}
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
	int index;

	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));
	g_return_if_fail (page_id != NULL);
	g_return_if_fail (title != NULL);
	g_return_if_fail (GTK_IS_WIDGET (main_widget));

	index = gtk_notebook_append_page (GTK_NOTEBOOK (ev_sidebar->priv->notebook),
					  main_widget, NULL);
					  
	gtk_list_store_insert_with_values (GTK_LIST_STORE (ev_sidebar->priv->page_model),
					   &iter, 0,
					   PAGE_COLUMN_ID, page_id,
					   PAGE_COLUMN_TITLE, title,
					   PAGE_COLUMN_MAIN_WIDGET, main_widget,
					   PAGE_COLUMN_NOTEBOOK_INDEX, index,
					   -1);

	/* Set the first item added as active */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (ev_sidebar->priv->option_menu)))
		gtk_combo_box_set_active (GTK_COMBO_BOX (ev_sidebar->priv->option_menu), 0);
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
		    && ev_document_links_has_document_links (EV_DOCUMENT_LINKS (document)))
			ev_sidebar_links_set_document (EV_SIDEBAR_LINKS (widget),
						       document);
		else if (EV_IS_SIDEBAR_THUMBNAILS (widget) &&
			 EV_IS_DOCUMENT_THUMBNAILS (document))
			ev_sidebar_thumbnails_set_document (EV_SIDEBAR_THUMBNAILS (widget),
							    document);
	}
	

}

void
ev_sidebar_clear (EvSidebar *ev_sidebar)
{
	g_return_if_fail (EV_IS_SIDEBAR (ev_sidebar));

	gtk_list_store_clear (GTK_LIST_STORE (ev_sidebar->priv->page_model));
}
