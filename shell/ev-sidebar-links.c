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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-sidebar-page.h"
#include "ev-sidebar-links.h"
#include "ev-job-queue.h"
#include "ev-document-links.h"
#include "ev-window.h"

struct _EvSidebarLinksPrivate {
	GtkWidget *tree_view;

	/* Keep these ids around for blocking */
	guint selection_id;
	guint page_changed_id;

	EvJob *job;
	GtkTreeModel *model;
	EvDocument *document;
	EvPageCache *page_cache;
};

enum {
	PROP_0,
	PROP_MODEL,
};


static void links_page_num_func				(GtkTreeViewColumn *tree_column,
							 GtkCellRenderer   *cell,
							 GtkTreeModel      *tree_model,
							 GtkTreeIter       *iter,
							 EvSidebarLinks    *sidebar_links);
static void update_page_callback 			(EvPageCache       *page_cache,
							 gint               current_page,
						         EvSidebarLinks    *sidebar_links);
static void ev_sidebar_links_page_iface_init 		(EvSidebarPageIface *iface);
static void ev_sidebar_links_clear_document     	(EvSidebarLinks *sidebar_links);
static void ev_sidebar_links_set_document      	 	(EvSidebarPage  *sidebar_page,
		    			        	 EvDocument     *document);
static gboolean ev_sidebar_links_support_document	(EvSidebarPage  *sidebar_page,
						         EvDocument     *document);
static const gchar* ev_sidebar_links_get_label 		(EvSidebarPage *sidebar_page);


G_DEFINE_TYPE_EXTENDED (EvSidebarLinks, 
                        ev_sidebar_links, 
                        GTK_TYPE_VBOX,
                        0, 
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE, 
					       ev_sidebar_links_page_iface_init))


#define EV_SIDEBAR_LINKS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_LINKS, EvSidebarLinksPrivate))


static void
ev_sidebar_links_destroy (GtkObject *object)
{
	EvSidebarLinks *ev_sidebar_links = (EvSidebarLinks *) object;

	ev_sidebar_links_clear_document (ev_sidebar_links);
}

static void
ev_sidebar_links_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	EvSidebarLinks *ev_sidebar_links;
	GtkTreeModel *model;
  
	ev_sidebar_links = EV_SIDEBAR_LINKS (object);

	switch (prop_id)
	{
	case PROP_MODEL:
		model = ev_sidebar_links->priv->model;
		ev_sidebar_links->priv->model = GTK_TREE_MODEL (g_value_dup_object (value));
		if (model)
			g_object_unref (model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_sidebar_links_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	EvSidebarLinks *ev_sidebar_links;
  
	ev_sidebar_links = EV_SIDEBAR_LINKS (object);

	switch (prop_id)
	{
	case PROP_MODEL:
		g_value_set_object (value, ev_sidebar_links->priv->model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static void
ev_sidebar_links_class_init (EvSidebarLinksClass *ev_sidebar_links_class)
{
	GObjectClass *g_object_class;
	GtkObjectClass *gtk_object_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_links_class);
	gtk_object_class = GTK_OBJECT_CLASS (ev_sidebar_links_class);

	g_object_class->set_property = ev_sidebar_links_set_property;
	g_object_class->get_property = ev_sidebar_links_get_property;

	gtk_object_class->destroy = ev_sidebar_links_destroy;

	g_object_class_install_property (g_object_class,
					 PROP_MODEL,
					 g_param_spec_object ("model",
							      "Model",
							      "Current Model",
							      GTK_TYPE_TREE_MODEL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (g_object_class, sizeof (EvSidebarLinksPrivate));
}

static void
selection_changed_cb (GtkTreeSelection   *selection,
		      EvSidebarLinks     *ev_sidebar_links)
{
	EvDocument *document;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (EV_IS_SIDEBAR_LINKS (ev_sidebar_links));

	document = EV_DOCUMENT (ev_sidebar_links->priv->document);
	g_return_if_fail (ev_sidebar_links->priv->document != NULL);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EvLink *link;

		gtk_tree_model_get (model, &iter,
				    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
				    -1);
		
		if (link == NULL)
			return;

		g_signal_handler_block (ev_sidebar_links->priv->page_cache,
					ev_sidebar_links->priv->page_changed_id);
		/* FIXME: we should handle this better.  This breaks w/ URLs */
		ev_page_cache_set_link (ev_sidebar_links->priv->page_cache, link);
		g_signal_handler_unblock (ev_sidebar_links->priv->page_cache,
					  ev_sidebar_links->priv->page_changed_id);
	}
}

static GtkTreeModel *
create_loading_model (void)
{
	GtkTreeModel *retval;
	GtkTreeIter iter;
	gchar *markup;

	/* Creates a fake model to indicate that we're loading */
	retval = (GtkTreeModel *)gtk_list_store_new (EV_DOCUMENT_LINKS_COLUMN_NUM_COLUMNS,
						     G_TYPE_STRING,
						     G_TYPE_OBJECT);

	gtk_list_store_append (GTK_LIST_STORE (retval), &iter);
	markup = g_strdup_printf ("<span size=\"larger\" style=\"italic\">%s</span>", _("Loading..."));
	gtk_list_store_set (GTK_LIST_STORE (retval), &iter,
			    EV_DOCUMENT_LINKS_COLUMN_MARKUP, markup,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, NULL,
			    -1);
	g_free (markup);

	return retval;
}

static void
ev_sidebar_links_construct (EvSidebarLinks *ev_sidebar_links)
{
	EvSidebarLinksPrivate *priv;
	GtkWidget *swindow;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeModel *loading_model;

	priv = ev_sidebar_links->priv;

	swindow = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);

	/* Create tree view */
	loading_model = create_loading_model ();
	priv->tree_view = gtk_tree_view_new_with_model (loading_model);
	g_object_unref (loading_model);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
	gtk_container_add (GTK_CONTAINER (swindow), priv->tree_view);

	gtk_box_pack_start (GTK_BOX (ev_sidebar_links), swindow, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (ev_sidebar_links));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view), column);

	renderer = (GtkCellRenderer*)
		g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
			      "ellipsize", PANGO_ELLIPSIZE_END,
			      NULL);
	gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), renderer, TRUE);
	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
					     "markup", EV_DOCUMENT_LINKS_COLUMN_MARKUP,
					     NULL);

	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (column), renderer,
						 (GtkTreeCellDataFunc) links_page_num_func,
						 ev_sidebar_links, NULL);

	
}

static void
ev_sidebar_links_init (EvSidebarLinks *ev_sidebar_links)
{
	ev_sidebar_links->priv = EV_SIDEBAR_LINKS_GET_PRIVATE (ev_sidebar_links);

	ev_sidebar_links_construct (ev_sidebar_links);
}

static void
links_page_num_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     EvSidebarLinks    *sidebar_links)
{
	EvLink *link;

	gtk_tree_model_get (tree_model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);
	
	if (link != NULL &&
	    ev_link_get_link_type (link) == EV_LINK_TYPE_PAGE) {
		gchar *page_label;
		gchar *page_string;

		page_label = ev_page_cache_get_page_label (sidebar_links->priv->page_cache, ev_link_get_page (link));
		page_string = g_markup_printf_escaped ("<i>%s</i>", page_label);

		g_object_set (cell,
 			      "markup", page_string,
			      "visible", TRUE,
			      NULL);

		g_free (page_label);
		g_free (page_string);
	} else {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
	}
}

/* Public Functions */

GtkWidget *
ev_sidebar_links_new (void)
{
	GtkWidget *ev_sidebar_links;

	ev_sidebar_links = g_object_new (EV_TYPE_SIDEBAR_LINKS, NULL);

	return ev_sidebar_links;
}

static void
ev_sidebar_links_clear_document (EvSidebarLinks *sidebar_links)
{
	EvSidebarLinksPrivate *priv;

	g_return_if_fail (EV_IS_SIDEBAR_LINKS (sidebar_links));

	priv = sidebar_links->priv;

	if (priv->document) {
		g_object_unref (priv->document);
		priv->document = NULL;
		priv->page_cache = NULL;
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), NULL);
}

static gboolean
update_page_callback_foreach (GtkTreeModel *model,
			      GtkTreePath  *path,
			      GtkTreeIter  *iter,
			      gpointer      data)
{
	EvSidebarLinks *sidebar_links = (data);
	EvLink *link;

	gtk_tree_model_get (model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);

	if (link && ev_link_get_link_type (link) == EV_LINK_TYPE_PAGE) {
		int current_page;

		current_page = ev_page_cache_get_current_page (sidebar_links->priv->page_cache);
		if (ev_link_get_page (link) == current_page) {
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sidebar_links->priv->tree_view));

			gtk_tree_selection_select_path (selection, path);

			return TRUE;
		}
	}
	
	return FALSE;
}

static void
update_page_callback (EvPageCache    *page_cache,
		      gint            current_page,
		      EvSidebarLinks *sidebar_links)
{
	GtkTreeSelection *selection;
	/* We go through the tree linearly looking for the first page that
	 * matches.  This is pretty inefficient.  We can do something neat with
	 * a GtkTreeModelSort here to make it faster, if it turns out to be
	 * slow.
	 */

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sidebar_links->priv->tree_view));

	g_signal_handler_block (selection, sidebar_links->priv->selection_id);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_model_foreach (sidebar_links->priv->model,
				update_page_callback_foreach,
				sidebar_links);

	g_signal_handler_unblock (selection, sidebar_links->priv->selection_id);
}

static void
job_finished_cb (EvJobLinks     *job,
		 EvSidebarLinks *sidebar_links)
{
	EvSidebarLinksPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean result;

	priv = sidebar_links->priv;

	priv->model = g_object_ref (job->model);
	g_object_notify (G_OBJECT (sidebar_links), "model");

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), job->model);
	g_object_unref (job);

	/* Expand one level of the tree */
	path = gtk_tree_path_new_first ();
	for (result = gtk_tree_model_get_iter_first (priv->model, &iter);
	     result;
	     result = gtk_tree_model_iter_next (priv->model, &iter)) {
		gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->tree_view), path, FALSE);
		gtk_tree_path_next (path);
	}
	gtk_tree_path_free (path);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	priv->selection_id = g_signal_connect (selection, "changed",
					       G_CALLBACK (selection_changed_cb),
					       sidebar_links);
	priv->page_changed_id = g_signal_connect (priv->page_cache, "page-changed",
						  G_CALLBACK (update_page_callback),
						  sidebar_links);
	update_page_callback (priv->page_cache,
			      ev_page_cache_get_current_page (priv->page_cache),
			      sidebar_links);

}

static void
ev_sidebar_links_set_document (EvSidebarPage  *sidebar_page,
			       EvDocument     *document)
{
	EvSidebarLinks *sidebar_links;
	EvSidebarLinksPrivate *priv;

	g_return_if_fail (EV_IS_SIDEBAR_PAGE (sidebar_page));
	g_return_if_fail (EV_IS_DOCUMENT (document));
	
	sidebar_links = EV_SIDEBAR_LINKS (sidebar_page);

	priv = sidebar_links->priv;

	g_object_ref (document);

	priv->document = document;
	priv->page_cache = ev_document_get_page_cache (document);

	priv->job = ev_job_links_new (document);
	g_signal_connect (priv->job,
			  "finished",
			  G_CALLBACK (job_finished_cb),
			  sidebar_links);
	/* The priority doesn't matter for this job */
	ev_job_queue_add_job (priv->job, EV_JOB_PRIORITY_LOW);

}

static gboolean
ev_sidebar_links_support_document (EvSidebarPage  *sidebar_page,
				   EvDocument *document)
{
	return (EV_IS_DOCUMENT_LINKS (document) &&
		    ev_document_links_has_document_links (EV_DOCUMENT_LINKS (document)));
}

static const gchar*
ev_sidebar_links_get_label (EvSidebarPage *sidebar_page)
{
    return _("Index");
}

static void
ev_sidebar_links_page_iface_init (EvSidebarPageIface *iface)
{
	iface->support_document = ev_sidebar_links_support_document;
	iface->set_document = ev_sidebar_links_set_document;
	iface->get_label = ev_sidebar_links_get_label;
}

