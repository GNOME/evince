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

#include "ev-sidebar-links.h"
#include "ev-job-queue.h"
#include "ev-document-links.h"
#include "ev-window.h"

struct _EvSidebarLinksPrivate {
	GtkWidget *tree_view;

	EvJobLinks *job;
	GtkTreeModel *model;
	EvDocument *current_document;
};

#if 0
static void links_page_num_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
#endif
G_DEFINE_TYPE (EvSidebarLinks, ev_sidebar_links, GTK_TYPE_VBOX)

#define EV_SIDEBAR_LINKS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_LINKS, EvSidebarLinksPrivate))


static void
ev_sidebar_links_destroy (GtkObject *object)
{
	EvSidebarLinks *ev_sidebar_links = (EvSidebarLinks *) object;

	ev_sidebar_links_clear_document (ev_sidebar_links);
}

static void
ev_sidebar_links_class_init (EvSidebarLinksClass *ev_sidebar_links_class)
{
	GObjectClass *g_object_class;
	GtkObjectClass *gtk_object_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_links_class);
	gtk_object_class = GTK_OBJECT_CLASS (ev_sidebar_links_class);

	gtk_object_class->destroy = ev_sidebar_links_destroy;

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

	document = EV_DOCUMENT (ev_sidebar_links->priv->current_document);
	g_return_if_fail (ev_sidebar_links->priv->current_document != NULL);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EvLink *link;
		GtkWidget *window;
		GValue value = {0, };

		gtk_tree_model_get_value (model, &iter,
					  EV_DOCUMENT_LINKS_COLUMN_LINK, &value);
		
		link = EV_LINK (g_value_get_object (&value));
		g_return_if_fail (link != NULL);

		window = gtk_widget_get_ancestor (GTK_WIDGET (ev_sidebar_links),
						  EV_TYPE_WINDOW);
		if (window) {
			ev_window_open_link (EV_WINDOW (window), link);
		}
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
						     G_TYPE_BOOLEAN,
						     G_TYPE_OBJECT);

	gtk_list_store_append (GTK_LIST_STORE (retval), &iter);
	markup = g_strdup_printf ("<span size=\"larger\" style=\"italic\">%s</span>", _("Loading..."));
	gtk_list_store_set (GTK_LIST_STORE (retval), &iter,
			    EV_DOCUMENT_LINKS_COLUMN_MARKUP, markup,
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

	
#if 0
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (column), renderer,
						 (GtkTreeCellDataFunc) links_page_num_func,
						 NULL, NULL);
#endif

	
}

static void
ev_sidebar_links_init (EvSidebarLinks *ev_sidebar_links)
{
	ev_sidebar_links->priv = EV_SIDEBAR_LINKS_GET_PRIVATE (ev_sidebar_links);

	ev_sidebar_links_construct (ev_sidebar_links);
}

#if 0
static void
links_page_num_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
	int page_num;
	gboolean page_valid;

	gtk_tree_model_get (tree_model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_MARKUPLINKS_COLUMN_PAGE_NUM, &page_num,
			    LINKS_COLUMN_PAGE_VALID, &page_valid,
			    -1);

	if (page_valid) {
		gchar *markup = g_strdup_printf ("<i>%d</i>", page_num);
		g_object_set (cell,
			      "markup", markup,
			      "visible", TRUE,
			      NULL);
		g_free (markup);
	} else {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
	}
}
#endif

/* Public Functions */

GtkWidget *
ev_sidebar_links_new (void)
{
	GtkWidget *ev_sidebar_links;

	ev_sidebar_links = g_object_new (EV_TYPE_SIDEBAR_LINKS, NULL);

	return ev_sidebar_links;
}

void
ev_sidebar_links_clear_document (EvSidebarLinks *sidebar_links)
{
	EvSidebarLinksPrivate *priv;

	g_return_if_fail (EV_IS_SIDEBAR_LINKS (sidebar_links));

	priv = sidebar_links->priv;

	if (priv->current_document) {
		g_object_unref (priv->current_document);
		priv->current_document = NULL;
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), NULL);
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
	g_signal_connect (selection, "changed",
			  G_CALLBACK (selection_changed_cb),
			  sidebar_links);
}

void
ev_sidebar_links_set_document (EvSidebarLinks *sidebar_links,
			       EvDocument     *document)
{
	EvSidebarLinksPrivate *priv;

	g_return_if_fail (EV_IS_SIDEBAR_LINKS (sidebar_links));
	g_return_if_fail (EV_IS_DOCUMENT (document));

	priv = sidebar_links->priv;

	g_object_ref (document);

	priv->current_document = document;

	priv->job = ev_job_links_new (document);
	g_signal_connect (priv->job,
			  "finished",
			  G_CALLBACK (job_finished_cb),
			  sidebar_links);
	ev_job_queue_add_links_job (priv->job);

}

