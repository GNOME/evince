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

#include "ev-sidebar-links.h"
#include "ev-document-links.h"
#include "ev-application.h"

/* Amount of time we devote to each iteration of the idle, in microseconds */
#define IDLE_WORK_LENGTH 5000

typedef struct {
	EvDocumentLinksIter *links_iter;
	GtkTreeIter *tree_iter;
} IdleStackData;

struct _EvSidebarLinksPrivate {
	GtkWidget *tree_view;
	GtkTreeModel *model;
	EvDocument *current_document;
	GList *idle_stack;
	guint idle_id;
};

enum {
	LINKS_COLUMN_MARKUP,
	LINKS_COLUMN_PAGE_NUM,
	LINKS_COLUMN_PAGE_VALID,
	LINKS_COLUMN_LINK,
	LINKS_COLUMN_NUM_COLUMNS
};

static void links_page_num_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);

G_DEFINE_TYPE (EvSidebarLinks, ev_sidebar_links, GTK_TYPE_VBOX)

#define EV_SIDEBAR_LINKS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_LINKS, EvSidebarLinksPrivate))


static void
ev_sidebar_links_destroy (GtkObject *object)
{
	EvSidebarLinks *ev_sidebar_links = (EvSidebarLinks *) object;

	g_print ("ev_sidebar_links_destroy!\n");
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
		EvApplication *app;
		GtkWidget *window;
		GValue value = {0, };

		gtk_tree_model_get_value (model, &iter,
					  LINKS_COLUMN_LINK, &value);

		link = EV_LINK (g_value_get_object (&value));
		g_return_if_fail (link != NULL);

		window = gtk_widget_get_ancestor (GTK_WIDGET (ev_sidebar_links),
						  EV_TYPE_WINDOW);
		if (window) {
			app = ev_application_get_instance ();
			ev_application_open_link (app, EV_WINDOW (window),
						  link, NULL);
		}
	}
}

static void
ev_sidebar_links_construct (EvSidebarLinks *ev_sidebar_links)
{
	EvSidebarLinksPrivate *priv;
	GtkWidget *swindow;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;

	priv = ev_sidebar_links->priv;
	priv->model = (GtkTreeModel *) gtk_tree_store_new (LINKS_COLUMN_NUM_COLUMNS,
							   G_TYPE_STRING,
							   G_TYPE_INT,
							   G_TYPE_BOOLEAN,
							   G_TYPE_OBJECT);

	swindow = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);

	/* Create tree view */
	priv->tree_view = gtk_tree_view_new_with_model (priv->model);
	g_object_unref (priv->model);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
	gtk_container_add (GTK_CONTAINER (swindow), priv->tree_view);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree_view), TRUE);

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
					     "markup", LINKS_COLUMN_MARKUP,
					     NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (column), renderer,
						 (GtkTreeCellDataFunc) links_page_num_func,
						 NULL, NULL);


	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (selection_changed_cb),
			  ev_sidebar_links);
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
		     gpointer           data)
{
	int page_num;
	gboolean page_valid;

	gtk_tree_model_get (tree_model, iter,
			    LINKS_COLUMN_PAGE_NUM, &page_num,
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

/* Public Functions */

GtkWidget *
ev_sidebar_links_new (void)
{
	GtkWidget *ev_sidebar_links;

	ev_sidebar_links = g_object_new (EV_TYPE_SIDEBAR_LINKS, NULL);

	return ev_sidebar_links;
}

static void
stack_data_free (IdleStackData       *stack_data,
		 EvDocumentLinks     *document_links)
{
	g_assert (stack_data);

	if (stack_data->tree_iter)
		gtk_tree_iter_free (stack_data->tree_iter);
	if (stack_data->links_iter)
		ev_document_links_free_iter (document_links, stack_data->links_iter);
	g_free (stack_data);
}

static gboolean
do_one_iteration (EvSidebarLinks *ev_sidebar_links)
{
	EvSidebarLinksPrivate *priv = ev_sidebar_links->priv;
	EvLink *link;
	IdleStackData *stack_data;
	GtkTreeIter tree_iter;
	EvDocumentLinksIter *child_iter;
	gint page;

	g_assert (priv->idle_stack);

	stack_data = (IdleStackData *) priv->idle_stack->data;

	link = ev_document_links_get_link
		(EV_DOCUMENT_LINKS (priv->current_document),
		 stack_data->links_iter);
	if (link == NULL) {
		g_warning ("mismatch in model.  No values available at current level.\n");
		return FALSE;
	}

	page = ev_link_get_page (link);
	gtk_tree_store_append (GTK_TREE_STORE (priv->model), &tree_iter, stack_data->tree_iter);
	gtk_tree_store_set (GTK_TREE_STORE (priv->model), &tree_iter,
			    LINKS_COLUMN_MARKUP, ev_link_get_title (link),
			    LINKS_COLUMN_PAGE_NUM, page,
			    /* FIXME: Handle links for real. */
			    LINKS_COLUMN_PAGE_VALID, (page >= 0),
			    LINKS_COLUMN_LINK, link,
			    -1);
	g_object_unref (link);
	
	child_iter = ev_document_links_get_child (EV_DOCUMENT_LINKS (priv->current_document),
						      stack_data->links_iter);
	if (child_iter) {
		IdleStackData *child_stack_data;

		child_stack_data = g_new0 (IdleStackData, 1);
		child_stack_data->tree_iter = gtk_tree_iter_copy (&tree_iter);
		child_stack_data->links_iter = child_iter;
		priv->idle_stack = g_list_prepend (priv->idle_stack, child_stack_data);

		return TRUE;
	}

	/* We don't have children, so we need to walk to the next node */
	while (TRUE) {
		if (ev_document_links_next (EV_DOCUMENT_LINKS (priv->current_document),
						stack_data->links_iter))
			return TRUE;

		/* We're done with this level.  Pop it off the idle stack and go
		 * to the next level */
		stack_data_free (stack_data, EV_DOCUMENT_LINKS (priv->current_document));
		priv->idle_stack = g_list_delete_link (priv->idle_stack, priv->idle_stack);
		if (priv->idle_stack == NULL)
			return FALSE;
		stack_data = priv->idle_stack->data;
	}
}

static gboolean
populate_links_idle (gpointer data)
{
	GTimer *timer;
	gint i;
	gulong microseconds = 0;

	EvSidebarLinks *ev_sidebar_links = (EvSidebarLinks *)data;
	EvSidebarLinksPrivate *priv = ev_sidebar_links->priv;

	if (priv->idle_stack == NULL) {
		priv->idle_id = 0;
		return FALSE;
	}

	/* The amount of time that reading the next bookmark takes is wildly
	 * inconsistent, so we constrain it to IDLE_WORK_LENGTH microseconds per
	 * idle iteration. */
	timer = g_timer_new ();
	i = 0;
	g_timer_start (timer);
	while (do_one_iteration (ev_sidebar_links)) {
		i++;
		g_timer_elapsed (timer, &microseconds);
		if (microseconds > IDLE_WORK_LENGTH)
			break;
	}
	g_timer_destroy (timer);
#if 0
	g_print ("%d rows done this idle in %d\n", i, (int)microseconds);
#endif
	return TRUE;
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
	gtk_tree_store_clear (GTK_TREE_STORE (priv->model));

	/* Clear the idle */
	if (priv->idle_id != 0) {
		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	}
	g_list_foreach (priv->idle_stack, (GFunc) stack_data_free, priv->current_document);
	g_list_free (priv->idle_stack);
	priv->idle_stack = NULL;

}

void
ev_sidebar_links_set_document (EvSidebarLinks *sidebar_links,
			       EvDocument     *document)
{
	EvSidebarLinksPrivate *priv;
	EvDocumentLinksIter *links_iter;

	g_return_if_fail (EV_IS_SIDEBAR_LINKS (sidebar_links));
	g_return_if_fail (EV_IS_DOCUMENT (document));

	priv = sidebar_links->priv;

	g_object_ref (document);
	ev_sidebar_links_clear_document (sidebar_links);

	priv->current_document = document;
	links_iter = ev_document_links_begin_read (EV_DOCUMENT_LINKS (document));
	if (links_iter) {
		IdleStackData *stack_data;

		stack_data = g_new0 (IdleStackData, 1);
		stack_data->links_iter = links_iter;
		stack_data->tree_iter = NULL;

		priv->idle_stack = g_list_prepend (priv->idle_stack, stack_data);
		priv->idle_id = g_idle_add (populate_links_idle, sidebar_links);
	}
}

