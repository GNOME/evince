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

#include "ev-sidebar-bookmarks.h"
#include "ev-document-bookmarks.h"
#include "ev-application.h"

/* Amount of time we devote to each iteration of the idle, in microseconds */
#define IDLE_WORK_LENGTH 5000

typedef struct {
	EvDocumentBookmarksIter *bookmarks_iter;
	GtkTreeIter *tree_iter;
} IdleStackData;

struct _EvSidebarBookmarksPrivate {
	GtkWidget *tree_view;
	GtkTreeModel *model;
	EvDocument *current_document;
	GList *idle_stack;
	guint idle_id;
};

enum {
	BOOKMARKS_COLUMN_MARKUP,
	BOOKMARKS_COLUMN_PAGE_NUM,
	BOOKMARKS_COLUMN_PAGE_VALID,
	BOOKMARKS_COLUMN_BOOKMARK,
	BOOKMARKS_COLUMN_NUM_COLUMNS
};

static void bookmarks_page_num_func (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell,
				     GtkTreeModel      *tree_model,
				     GtkTreeIter       *iter,
				     gpointer           data);

G_DEFINE_TYPE (EvSidebarBookmarks, ev_sidebar_bookmarks, GTK_TYPE_VBOX)

#define EV_SIDEBAR_BOOKMARKS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarksPrivate))


static void
ev_sidebar_bookmarks_destroy (GtkObject *object)
{
	EvSidebarBookmarks *ev_sidebar_bookmarks = (EvSidebarBookmarks *) object;

	g_print ("ev_sidebar_bookmarks_destroy!\n");
	ev_sidebar_bookmarks_clear_document (ev_sidebar_bookmarks);
}

static void
ev_sidebar_bookmarks_class_init (EvSidebarBookmarksClass *ev_sidebar_bookmarks_class)
{
	GObjectClass *g_object_class;
	GtkObjectClass *gtk_object_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_bookmarks_class);
	gtk_object_class = GTK_OBJECT_CLASS (ev_sidebar_bookmarks_class);

	gtk_object_class->destroy = ev_sidebar_bookmarks_destroy;

	g_type_class_add_private (g_object_class, sizeof (EvSidebarBookmarksPrivate));
}

static void
selection_changed_cb (GtkTreeSelection   *selection,
		      EvSidebarBookmarks *ev_sidebar_bookmarks)
{
	EvDocument *document;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (EV_IS_SIDEBAR_BOOKMARKS (ev_sidebar_bookmarks));

	document = EV_DOCUMENT (ev_sidebar_bookmarks->priv->current_document);
	g_return_if_fail (ev_sidebar_bookmarks->priv->current_document != NULL);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EvBookmark *bookmark;
		EvApplication *app;
		GtkWidget *window;
		GValue value = {0, };

		gtk_tree_model_get_value (model, &iter,
					  BOOKMARKS_COLUMN_BOOKMARK, &value);

		bookmark = EV_BOOKMARK (g_value_get_object (&value));
		g_return_if_fail (bookmark != NULL);

		window = gtk_widget_get_ancestor (GTK_WIDGET (ev_sidebar_bookmarks),
						  EV_TYPE_WINDOW);
		if (window) {
			app = ev_application_get_instance ();
			ev_application_open_bookmark (app, EV_WINDOW (window),
						      bookmark, NULL);
		}
	}
}

static void
ev_sidebar_bookmarks_construct (EvSidebarBookmarks *ev_sidebar_bookmarks)
{
	EvSidebarBookmarksPrivate *priv;
	GtkWidget *swindow;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;

	priv = ev_sidebar_bookmarks->priv;
	priv->model = (GtkTreeModel *) gtk_tree_store_new (BOOKMARKS_COLUMN_NUM_COLUMNS,
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

	gtk_box_pack_start (GTK_BOX (ev_sidebar_bookmarks), swindow, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (ev_sidebar_bookmarks));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view), column);

	renderer = (GtkCellRenderer*)
		g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
			      "ellipsize", PANGO_ELLIPSIZE_END,
			      NULL);
	gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), renderer, TRUE);
	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
					     "markup", BOOKMARKS_COLUMN_MARKUP,
					     NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (column), renderer,
						 (GtkTreeCellDataFunc) bookmarks_page_num_func,
						 NULL, NULL);


	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (selection_changed_cb),
			  ev_sidebar_bookmarks);
}

static void
ev_sidebar_bookmarks_init (EvSidebarBookmarks *ev_sidebar_bookmarks)
{
	ev_sidebar_bookmarks->priv = EV_SIDEBAR_BOOKMARKS_GET_PRIVATE (ev_sidebar_bookmarks);

	ev_sidebar_bookmarks_construct (ev_sidebar_bookmarks);
}

static void
bookmarks_page_num_func (GtkTreeViewColumn *tree_column,
			 GtkCellRenderer   *cell,
			 GtkTreeModel      *tree_model,
			 GtkTreeIter       *iter,
			 gpointer           data)
{
	int page_num;
	gboolean page_valid;

	gtk_tree_model_get (tree_model, iter,
			    BOOKMARKS_COLUMN_PAGE_NUM, &page_num,
			    BOOKMARKS_COLUMN_PAGE_VALID, &page_valid,
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
ev_sidebar_bookmarks_new (void)
{
	GtkWidget *ev_sidebar_bookmarks;

	ev_sidebar_bookmarks = g_object_new (EV_TYPE_SIDEBAR_BOOKMARKS, NULL);

	return ev_sidebar_bookmarks;
}

static void
stack_data_free (IdleStackData       *stack_data,
		 EvDocumentBookmarks *document_bookmarks)
{
	g_assert (stack_data);

	if (stack_data->tree_iter)
		gtk_tree_iter_free (stack_data->tree_iter);
	if (stack_data->bookmarks_iter)
		ev_document_bookmarks_free_iter (document_bookmarks, stack_data->bookmarks_iter);
	g_free (stack_data);
}

static gboolean
do_one_iteration (EvSidebarBookmarks *ev_sidebar_bookmarks)
{
	EvSidebarBookmarksPrivate *priv = ev_sidebar_bookmarks->priv;
	EvBookmark *bookmark;
	IdleStackData *stack_data;
	GtkTreeIter tree_iter;
	EvDocumentBookmarksIter *child_iter;
	gint page;

	g_assert (priv->idle_stack);

	stack_data = (IdleStackData *) priv->idle_stack->data;

	bookmark = ev_document_bookmarks_get_bookmark
		(EV_DOCUMENT_BOOKMARKS (priv->current_document),
		 stack_data->bookmarks_iter);
	if (bookmark == NULL) {
		g_warning ("mismatch in model.  No values available at current level.\n");
		return FALSE;
	}

	page = ev_bookmark_get_page (bookmark);
	gtk_tree_store_append (GTK_TREE_STORE (priv->model), &tree_iter, stack_data->tree_iter);
	gtk_tree_store_set (GTK_TREE_STORE (priv->model), &tree_iter,
			    BOOKMARKS_COLUMN_MARKUP, ev_bookmark_get_title (bookmark),
			    BOOKMARKS_COLUMN_PAGE_NUM, page,
			    /* FIXME: Handle links for real. */
			    BOOKMARKS_COLUMN_PAGE_VALID, (page >= 0),
			    BOOKMARKS_COLUMN_BOOKMARK, bookmark,
			    -1);
	g_object_unref (bookmark);
	
	child_iter = ev_document_bookmarks_get_child (EV_DOCUMENT_BOOKMARKS (priv->current_document),
						      stack_data->bookmarks_iter);
	if (child_iter) {
		IdleStackData *child_stack_data;

		child_stack_data = g_new0 (IdleStackData, 1);
		child_stack_data->tree_iter = gtk_tree_iter_copy (&tree_iter);
		child_stack_data->bookmarks_iter = child_iter;
		priv->idle_stack = g_list_prepend (priv->idle_stack, child_stack_data);

		return TRUE;
	}

	/* We don't have children, so we need to walk to the next node */
	while (TRUE) {
		if (ev_document_bookmarks_next (EV_DOCUMENT_BOOKMARKS (priv->current_document),
						stack_data->bookmarks_iter))
			return TRUE;

		/* We're done with this level.  Pop it off the idle stack and go
		 * to the next level */
		stack_data_free (stack_data, EV_DOCUMENT_BOOKMARKS (priv->current_document));
		priv->idle_stack = g_list_delete_link (priv->idle_stack, priv->idle_stack);
		if (priv->idle_stack == NULL)
			return FALSE;
		stack_data = priv->idle_stack->data;
	}
}

static gboolean
populate_bookmarks_idle (gpointer data)
{
	GTimer *timer;
	gint i;
	gulong microseconds = 0;

	EvSidebarBookmarks *ev_sidebar_bookmarks = (EvSidebarBookmarks *)data;
	EvSidebarBookmarksPrivate *priv = ev_sidebar_bookmarks->priv;

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
	while (do_one_iteration (ev_sidebar_bookmarks)) {
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
ev_sidebar_bookmarks_clear_document (EvSidebarBookmarks *sidebar_bookmarks)
{
	EvSidebarBookmarksPrivate *priv;

	g_return_if_fail (EV_IS_SIDEBAR_BOOKMARKS (sidebar_bookmarks));

	priv = sidebar_bookmarks->priv;
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
ev_sidebar_bookmarks_set_document (EvSidebarBookmarks *sidebar_bookmarks,
				   EvDocument         *document)
{
	EvSidebarBookmarksPrivate *priv;
	EvDocumentBookmarksIter *bookmarks_iter;

	g_return_if_fail (EV_IS_SIDEBAR_BOOKMARKS (sidebar_bookmarks));
	g_return_if_fail (EV_IS_DOCUMENT (document));

	priv = sidebar_bookmarks->priv;

	g_object_ref (document);
	ev_sidebar_bookmarks_clear_document (sidebar_bookmarks);

	priv->current_document = document;
	bookmarks_iter = ev_document_bookmarks_begin_read (EV_DOCUMENT_BOOKMARKS (document));
	if (bookmarks_iter) {
		IdleStackData *stack_data;

		stack_data = g_new0 (IdleStackData, 1);
		stack_data->bookmarks_iter = bookmarks_iter;
		stack_data->tree_iter = NULL;

		priv->idle_stack = g_list_prepend (priv->idle_stack, stack_data);
		priv->idle_id = g_idle_add (populate_bookmarks_idle, sidebar_bookmarks);
	}
}

