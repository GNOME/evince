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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-document-links.h"
#include "ev-job-scheduler.h"
#include "ev-sidebar-links.h"
#include "ev-sidebar-page.h"
#include "ev-window.h"

#define GSTRING_INIT_SIZE 4096

struct _EvSidebarLinksPrivate {
	GtkWidget *tree_view;
	GActionGroup *group;

	/* Keep these ids around for blocking */
	guint selection_id;
	guint page_changed_id;
	guint row_activated_id;

	EvJob *job;
	GtkTreeModel *model;
	EvDocument *document;
	EvDocumentModel *doc_model;

	GTree *page_link_tree;

	GtkWidget  *popup;
};

enum {
	PROP_0,
	PROP_MODEL,
	PROP_WIDGET,
	PROP_DOCUMENT_MODEL,
};

enum {
	LINK_ACTIVATED,
	N_SIGNALS
};

static void update_page_callback 			(EvSidebarLinks    *sidebar_links,
							 gint               old_page,
							 gint               current_page);
static void row_activated_callback 			(GtkTreeView *treeview,
		                                         GtkTreePath *arg1,
	                                                 GtkTreeViewColumn *arg2,
		                                         gpointer user_data);
static void ev_sidebar_links_set_links_model            (EvSidebarLinks *links,
							 GtkTreeModel   *model);
static void job_finished_callback 			(EvJobLinks     *job,
				    		         EvSidebarLinks *sidebar_links);
static void ev_sidebar_links_set_current_page           (EvSidebarLinks *sidebar_links,
							 gint            current_page);
static void sidebar_collapse_recursive                  (EvSidebarLinks *sidebar_links);
static void ev_sidebar_links_page_iface_init 		(EvSidebarPageInterface *iface);
static gboolean ev_sidebar_links_support_document	(EvSidebarPage  *sidebar_page,
						         EvDocument     *document);
static void ev_sidebar_links_set_model			(EvSidebarPage   *sidebar_page,
							 EvDocumentModel *model);

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED (EvSidebarLinks,
                        ev_sidebar_links,
                        GTK_TYPE_BOX,
                        0,
                        G_ADD_PRIVATE (EvSidebarLinks)
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
					       ev_sidebar_links_page_iface_init))


static void
ev_sidebar_links_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	EvSidebarLinks *ev_sidebar_links = EV_SIDEBAR_LINKS (object);

	switch (prop_id)
	{
	case PROP_MODEL:
		ev_sidebar_links_set_links_model (ev_sidebar_links, g_value_get_object (value));
		break;
	case PROP_DOCUMENT_MODEL:
		ev_sidebar_links_set_model (EV_SIDEBAR_PAGE (ev_sidebar_links),
			EV_DOCUMENT_MODEL (g_value_get_object (value)));
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
	case PROP_WIDGET:
		g_value_set_object (value, ev_sidebar_links->priv->tree_view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_sidebar_links_dispose (GObject *object)
{
	EvSidebarLinks *sidebar = EV_SIDEBAR_LINKS (object);

	if (sidebar->priv->job) {
		g_signal_handlers_disconnect_by_func (sidebar->priv->job,
						      job_finished_callback, sidebar);
		ev_job_cancel (sidebar->priv->job);
		g_clear_object (&sidebar->priv->job);
	}

	g_clear_object (&sidebar->priv->model);
	g_clear_pointer (&sidebar->priv->page_link_tree, g_tree_unref);

	if (sidebar->priv->document) {
		g_clear_object (&sidebar->priv->document);
		sidebar->priv->doc_model = NULL;
	}

	G_OBJECT_CLASS (ev_sidebar_links_parent_class)->dispose (object);
}

static void
ev_sidebar_links_map (GtkWidget *widget)
{
	EvSidebarLinks *links;

	links = EV_SIDEBAR_LINKS (widget);

	GTK_WIDGET_CLASS (ev_sidebar_links_parent_class)->map (widget);

	if (links->priv->model) {
		ev_sidebar_links_set_current_page (links,
						   ev_document_model_get_page (links->priv->doc_model));
	}
}

static void
ev_sidebar_links_set_action_enabled (EvSidebarLinks *ev_sidebar_links,
				     const char     *name,
				     gboolean        enabled)
{
	GAction *action = g_action_map_lookup_action (
			G_ACTION_MAP (ev_sidebar_links->priv->group), name);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
selection_changed_callback (GtkTreeSelection   *selection,
		            EvSidebarLinks     *ev_sidebar_links)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (ev_sidebar_links->priv->document != NULL);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EvLink *link;

		gtk_tree_model_get (model, &iter,
				    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
				    -1);

		if (link == NULL)
			return;

		g_signal_handler_block (ev_sidebar_links->priv->doc_model,
					ev_sidebar_links->priv->page_changed_id);
		g_signal_emit (ev_sidebar_links, signals[LINK_ACTIVATED], 0, link);
		g_signal_handler_unblock (ev_sidebar_links->priv->doc_model,
					  ev_sidebar_links->priv->page_changed_id);

		g_object_unref (link);
	}
}

static void
ev_links_popup_cmd_search_outline (GSimpleAction *action,
				   GVariant      *parameter,
				   gpointer       ev_sidebar_links)
{
	GtkNative *window;

	window = gtk_widget_get_native (GTK_WIDGET (ev_sidebar_links));
	if (EV_IS_WINDOW (window)) {
		ev_window_start_page_selector_search (EV_WINDOW (window));
	}
}

static void
ev_links_popup_cmd_print_section (GSimpleAction *action,
				  GVariant      *parameter,
				  gpointer       ev_sidebar_links)
{
	EvSidebarLinks *sidebar = EV_SIDEBAR_LINKS (ev_sidebar_links);
	EvSidebarLinksPrivate *priv = EV_SIDEBAR_LINKS (sidebar)->priv;
	GtkNative *window;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EvLink *link;
		int first_page, last_page = -1;
		EvDocumentLinks *document_links;

		gtk_tree_model_get (model, &iter,
				    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
				    -1);

		if (!link)
			return;

		document_links = EV_DOCUMENT_LINKS (priv->document);

		first_page = ev_document_links_get_link_page (document_links, link);
		if (first_page == -1) {
			g_object_unref (link);
			return;
		}

		first_page++;
		g_object_unref (link);

		if (gtk_tree_model_iter_next (model, &iter)) {
			gtk_tree_model_get (model, &iter,
					    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
					    -1);

			if (link) {
				last_page = ev_document_links_get_link_page (document_links, link);;
				g_object_unref (link);
			}
		} else {
			last_page = ev_document_get_n_pages (priv->document);
		}

		if (last_page == -1)
			last_page = ev_document_get_n_pages (priv->document);

		window = gtk_widget_get_native (GTK_WIDGET (sidebar));
		if (EV_IS_WINDOW (window)) {
			ev_window_print_range (EV_WINDOW (window), first_page, last_page);
		}
	}
}

static gboolean
model_is_plain_list (GtkTreeModel *model)
{
	GtkTreeIter iter;
	gtk_tree_model_get_iter_first (model, &iter);
	do {
		if (gtk_tree_model_iter_has_child (model, &iter)) {
			return FALSE;
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	return TRUE;
}

static void
check_menu_sensitivity (GtkTreeView *treeview,
			GtkTreePath *selected_path,
			EvSidebarLinks *sidebar)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeIter parent;
	gboolean expand_under_sensitive = FALSE;
	static gboolean is_list = FALSE;
	static gboolean is_list_is_set = FALSE;

	model = sidebar->priv->model;

	/* Disable 'Collapse/Expand all' when tree has no depth, i.e. it's
	 * a plain list. Only calculate it once as tree doesn't change. */
	if (!is_list_is_set) {
		is_list_is_set = TRUE;
		if (model_is_plain_list (model)) {
			is_list = TRUE;
			ev_sidebar_links_set_action_enabled (sidebar, "collapse-all", FALSE);
			ev_sidebar_links_set_action_enabled (sidebar, "expand-all", FALSE);
			ev_sidebar_links_set_action_enabled (sidebar, "expand-element", FALSE);
		}
	}

	if (!selected_path) {
		ev_sidebar_links_set_action_enabled (sidebar, "print-section", FALSE);
		ev_sidebar_links_set_action_enabled (sidebar, "expand-element", FALSE);
	}

	if (is_list || !selected_path)
		return;

	ev_sidebar_links_set_action_enabled (sidebar, "print-section", TRUE);

	/* Enable 'Expand under this' only when 'this' element has any child */
	gtk_tree_model_get_iter (model, &parent, selected_path);
	if (gtk_tree_model_iter_children (model, &iter, &parent)) {
		expand_under_sensitive = TRUE;
	}

	ev_sidebar_links_set_action_enabled (sidebar, "expand-element", expand_under_sensitive);

	/* If we're in collapsed all state, then disable 'collapse all' action */
	if (gtk_tree_path_get_depth (selected_path) == 1) {
		gboolean all_collapsed = TRUE;
		gtk_tree_model_get_iter_first (model, &iter);
		do {
			if (gtk_tree_model_iter_has_child (model, &iter)) {
				GtkTreePath *path;

				path = gtk_tree_model_get_path (model, &iter);
				if (gtk_tree_view_row_expanded (treeview, path)) {
					all_collapsed = FALSE;
					gtk_tree_path_free (path);
					break;
				}
				gtk_tree_path_free (path);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
		ev_sidebar_links_set_action_enabled (sidebar, "collapse-all", !all_collapsed);
	} else
		ev_sidebar_links_set_action_enabled (sidebar, "collapse-all", TRUE);
}

static gboolean
path_is_selected (GtkTreeView *treeview,
                 GtkTreePath *path)
{
       GtkTreeModel *model;
       GtkTreeSelection *selection;
       GtkTreeIter iter;

       model = gtk_tree_view_get_model (treeview);
       selection = gtk_tree_view_get_selection (treeview);
       return gtk_tree_model_get_iter (model, &iter, path) &&
              gtk_tree_selection_iter_is_selected (selection, &iter);
}

static void
button_press_cb (GtkGestureClick	*self,
		 gint 			 n_press,
		 gdouble		 x,
		 gdouble		 y,
		 gpointer		 user_data)
{
	GdkEvent *event;
	GtkTreePath *path;
	EvSidebarLinksPrivate *priv = EV_SIDEBAR_LINKS (user_data)->priv;

	event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (self));

	if (gdk_button_event_get_button (event) == GDK_BUTTON_SECONDARY) {
	        if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
        	                                   x,
                	                           y,
	                                           &path,
        	                                   NULL, NULL, NULL)) {
			if (! path_is_selected (GTK_TREE_VIEW (priv->tree_view), path))
				path = NULL;

			check_menu_sensitivity (GTK_TREE_VIEW (priv->tree_view), path, EV_SIDEBAR_LINKS (user_data));

			gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup), &(const GdkRectangle){ x, y, 0, 0 });
			gtk_popover_popup (GTK_POPOVER (priv->popup));

			gtk_tree_path_free (path);
		}
	}
}

/**
 * remove_path_descendants:
 * @metadata_index: the contents of either "index-expand" or "index-collapse" metadata elements.
 * @path: Path entry to search and remove its descendants.
 *
 * Searches for descendants of @path in @metadata_index and removes them.
 *
 * Returns: New string with descendants removed, or %NULL if no descendants were found.
 */
static gchar *
remove_path_descendants (const gchar *metadata_index, GtkTreePath *path)
{
	gchar *path_str, *path_token, *tmp, *haystack, *ret;
	gboolean first_iteration;

	ret = NULL;
	path_str = gtk_tree_path_to_string (path);
	path_token = g_strconcat ("|", path_str, ":", NULL);
	tmp = g_strstr_len (metadata_index, -1, path_token);
	haystack = (gchar *) metadata_index;
	for (first_iteration = TRUE; tmp; tmp = g_strstr_len (haystack, -1, path_token)) {
		gchar *needle, *separator;

		separator = g_strstr_len (tmp + 1, -1, "|");
		needle = g_strndup (tmp, (gssize) (separator - tmp));
		ret = ev_str_replace (haystack, needle, "");
		if (!strcmp (ret, "|")) {
			g_free (ret);
			ret = g_strdup ("");
		}
		/* Purpose of 'first_iteration' is so that, just in the first iteration, we don't
		 * free 'haystack'. In first iteration, 'haystack' references const char data, while
		 * subsequent iterations will reference allocated char data. This optimization is to
		 * avoid an initial g_strdup() of the passed in 'const gchar *metadata_index' string,
		 * which can be large in documents with big outlines. */
		if (!first_iteration)
			g_free (haystack);
		else
			first_iteration = FALSE;
		haystack = ret;
		g_free (needle);
	}

	g_free (path_token);
	g_free (path_str);

	return ret;
}

/**
 * remove_path:
 * @metadata_index: the contents of either "index-expand" or "index-collapse" metadata elements.
 * @path: Path entry to remove from @metadata_index
 *
 * Returns: New string which is @metadata_index with @path removed, or %NULL if @path was not found.
 */
static gchar *
remove_path (const gchar *metadata_index, GtkTreePath *path)
{
	gchar *path_str, *path_token;
	gchar *ret;

	ret = NULL;
	path_str = gtk_tree_path_to_string (path);
	path_token = g_strconcat ("|", path_str, "|", NULL);

	if (g_strstr_len (metadata_index, -1, path_token)) {
		ret = ev_str_replace (metadata_index, path_token, "|");
		if (!strcmp (ret, "|")) {
			g_free (ret);
			ret = g_strdup ("");
		}
	}
	g_free (path_str);
	g_free (path_token);

	return ret;
}

/* Metadata keys 'index-expand' and 'index-collapse' are explained
 * in the main comment of row_collapsed_cb() function. */
static gboolean
row_expanded_cb (GtkTreeView  *tree_view,
		 GtkTreeIter  *expanded_iter,
		 GtkTreePath  *expanded_path,
		 gpointer      data)
{
	EvSidebarLinks *ev_sidebar_links;
	EvSidebarLinksPrivate *priv;
	GtkNative *window;
	EvMetadata *metadata;
	gchar *index_collapse, *index_expand, *new_index;
	gboolean expand;

	ev_sidebar_links = EV_SIDEBAR_LINKS (data);
	priv = ev_sidebar_links->priv;

	window = gtk_widget_get_native (GTK_WIDGET (ev_sidebar_links));
	if (!EV_IS_WINDOW (window)) {
		g_warning ("Could not find EvWindow metadata, index_{expand,collapse} metadata won't be saved");
		return GDK_EVENT_PROPAGATE;
	}

	metadata = ev_window_get_metadata (EV_WINDOW (window));
	if (metadata == NULL)
		return GDK_EVENT_PROPAGATE;

	index_collapse = NULL;
	/* If expanded row is in 'index_collapse' we remove it from there. */
	if (ev_metadata_get_string (metadata, "index-collapse", &index_collapse)) {
		new_index = remove_path (index_collapse, expanded_path);
		if (new_index) {
			ev_metadata_set_string (metadata, "index-collapse", new_index);
			g_free (new_index);
		}
	}

	gtk_tree_model_get (priv->model, expanded_iter,
			    EV_DOCUMENT_LINKS_COLUMN_EXPAND, &expand,
			    -1);
	/* if it's already marked "expand" by the pdf producer, we'll use that
	 * and so no need to add it to 'index_expand' */
	if (!expand) {
		gchar *path, *path_token;

		path = gtk_tree_path_to_string (expanded_path);
		path_token = g_strconcat ("|", path, "|", NULL);
		index_expand = NULL;

		if (ev_metadata_get_string (metadata, "index-expand", &index_expand)) {
			/* If it's not in 'index_expand' we add it */
			if (g_strstr_len (index_expand, -1, path_token) == NULL) {
				if (!strcmp (index_expand, ""))
					new_index = g_strconcat (index_expand, path_token, NULL);
				else
					new_index = g_strconcat (index_expand, path_token + 1, NULL);

				ev_metadata_set_string (metadata, "index-expand", new_index);
				g_free (new_index);
			}
		} else
			ev_metadata_set_string (metadata, "index-expand", path_token);

		g_free (path_token);
		g_free (path);
	}

	return GDK_EVENT_PROPAGATE;
}

/* Metadata key 'index-expand' is a string containing the GtkTreePath's that the user
 * has explicitly expanded, except those already marked expanded by the pdf producer
 * data. The string is like "|path1|path2|path3|" (starting and ending in pipe).
 * A case with only one element would be "|path1|". Case with no elements would be
 * the empty string "". This is to facilitate the search of the paths.
 *
 * Metadata key 'index-collapse' is a string containing the GtkTreePath's that the
 * pdf producer data had them marked as expanded but the user has explicitly collapsed
 * them. The string format is the same as in 'index-expand'. */
static gboolean
row_collapsed_cb (GtkTreeView *tree_view,
		  GtkTreeIter *collapsed_iter,
		  GtkTreePath *collapsed_path,
		  gpointer     data)
{
	GtkNative *window;
	EvMetadata *metadata;
	EvSidebarLinks *ev_sidebar_links;
	EvSidebarLinksPrivate *priv;
	gchar *index_expand, *index_collapse, *new_index;
	gboolean expand;

	ev_sidebar_links = EV_SIDEBAR_LINKS (data);
	priv = ev_sidebar_links->priv;

	window = gtk_widget_get_native (GTK_WIDGET (ev_sidebar_links));
	if (!EV_IS_WINDOW (window)) {
		g_warning ("Could not find EvWindow metadata, index_{expand,collapse} metadata won't be saved");
		return GDK_EVENT_PROPAGATE;
	}

	metadata = ev_window_get_metadata (EV_WINDOW (window));
	if (metadata == NULL)
		return GDK_EVENT_PROPAGATE;

	index_expand = NULL;
	/* If collapsed row is in 'index_expand' we remove it from there and also its descendants rows */
	if (ev_metadata_get_string (metadata, "index-expand", &index_expand)) {
		gchar *tmp;
		new_index = remove_path (index_expand, collapsed_path);
		if (new_index) {
			tmp = remove_path_descendants (new_index, collapsed_path);
			if (tmp) {
				ev_metadata_set_string (metadata, "index-expand", tmp);
				g_free (tmp);
			} else {
				ev_metadata_set_string (metadata, "index-expand", new_index);
			}
			g_free (new_index);
		} else {
			tmp = remove_path_descendants (index_expand, collapsed_path);
			if (tmp) {
				ev_metadata_set_string (metadata, "index-expand", tmp);
				g_free (tmp);
			}
		}
	}

	gtk_tree_model_get (priv->model, collapsed_iter,
			    EV_DOCUMENT_LINKS_COLUMN_EXPAND, &expand,
			    -1);
	/* We only add the collapsed row to 'index_collapse' if the row
	 * was marked expanded by the pdf producer data. */
	if (expand) {
		gchar *path, *path_token;
		path = gtk_tree_path_to_string (collapsed_path);
		path_token = g_strconcat ("|", path, "|", NULL);
		index_collapse = NULL;
		if (ev_metadata_get_string (metadata, "index-collapse", &index_collapse)) {
			/* If collapsed row is not in 'index_collapse' we add it. */
			if (g_strstr_len (index_collapse, -1, path_token) == NULL) {
				if (!index_expand || !strcmp (index_expand, ""))
					new_index = g_strconcat (index_collapse, path_token, NULL);
				else
					new_index = g_strconcat (index_collapse, path_token + 1, NULL);

				ev_metadata_set_string (metadata, "index-collapse", new_index);
				g_free (new_index);
			}
		}
		else
			ev_metadata_set_string (metadata, "index-collapse", path_token);

		g_free (path);
		g_free (path_token);
	}

	return GDK_EVENT_PROPAGATE;
}

/* This function recursively collapses all rows except those marked EXPAND
 * by the pdf producer and those expanded explicitly by the user (which
 * are stored in metadata "index-expand" key).
 *
 * The final purpose is to close all rows that were automatically expanded
 * by ev_sidebar_links_set_current_page() function (which automatically
 * expands rows according to current page in view).
 *
 * As the 'collapse' action is only meaningful in rows that have children,
 * we only traverse those. */
static void
collapse_recursive (GtkTreeView  *tree_view,
		    GtkTreeModel *model,
		    GtkTreeIter  *parent,
		    const gchar  *index_expand)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean expand;

	if (gtk_tree_model_iter_children (model, &iter, parent)) {
		do {
			if (!gtk_tree_model_iter_has_child (model, &iter))
				continue;

			gtk_tree_model_get (model, &iter,
					    EV_DOCUMENT_LINKS_COLUMN_EXPAND, &expand,
					    -1);
			if (expand)
				continue;

			path = gtk_tree_model_get_path (model, &iter);
			if (gtk_tree_view_row_expanded (tree_view, path)) {
				if (index_expand == NULL)
					gtk_tree_view_collapse_row (tree_view, path);
				else {
					gchar *path_str, *path_token;

					path_str = gtk_tree_path_to_string (path);
					path_token = g_strconcat ("|", path_str, "|", NULL);

					if (!g_strstr_len (index_expand, -1, path_token))
						gtk_tree_view_collapse_row (tree_view, path);

					g_free (path_str);
					g_free (path_token);
				}
			}
			gtk_tree_path_free (path);
			collapse_recursive (tree_view, model, &iter, index_expand);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static void
sidebar_collapse_recursive (EvSidebarLinks *sidebar_links)
{
	EvSidebarLinksPrivate *priv;
	GtkNative *window;
	EvMetadata *metadata;
	gchar *index_expand;

	priv = sidebar_links->priv;

	window = gtk_widget_get_native (GTK_WIDGET (sidebar_links));
	index_expand = NULL;
	if (EV_IS_WINDOW (window)) {
		metadata = ev_window_get_metadata (EV_WINDOW (window));
		if (metadata)
			ev_metadata_get_string (metadata, "index-expand", &index_expand);
	}

	g_signal_handlers_block_by_func (priv->tree_view, row_collapsed_cb, sidebar_links);
	collapse_recursive (GTK_TREE_VIEW (priv->tree_view), priv->model, NULL, index_expand);
	g_signal_handlers_unblock_by_func (priv->tree_view, row_collapsed_cb, sidebar_links);
}

static void
collapse_all_recursive (GtkTreeView  *tree_view,
			GtkTreeModel *model,
			GtkTreeIter  *parent,
			GString *index_collapse)
{
	GtkTreePath *path;
	gchar *path_str;
	GtkTreeIter iter;
	gboolean expand;

	if (gtk_tree_model_iter_children (model, &iter, parent)) {
		do {
			if (!gtk_tree_model_iter_has_child (model, &iter))
				continue;

			gtk_tree_model_get (model, &iter,
					    EV_DOCUMENT_LINKS_COLUMN_EXPAND, &expand,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			if (gtk_tree_view_row_expanded (tree_view, path))
				gtk_tree_view_collapse_row (tree_view, path);

			if (expand && index_collapse) {
				path_str = gtk_tree_path_to_string (path);
				g_string_append (index_collapse, path_str);
				g_string_append (index_collapse, "|");
				g_free (path_str);
			}
			gtk_tree_path_free (path);

			collapse_all_recursive (tree_view, model, &iter, index_collapse);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static void
ev_links_popup_cmd_collapse_all (GSimpleAction *action,
				 GVariant      *parameter,
				 gpointer       user_data)
{
	EvSidebarLinks *sidebar_links = EV_SIDEBAR_LINKS (user_data);
	EvSidebarLinksPrivate *priv = sidebar_links->priv;
	GtkNative *window;
	EvMetadata *metadata;
	GString *index_collapse = NULL;

	window = gtk_widget_get_native (GTK_WIDGET (sidebar_links));
	if (EV_IS_WINDOW (window)) {
		metadata = ev_window_get_metadata (EV_WINDOW (window));
		if (metadata) {
			ev_metadata_set_string (metadata, "index-expand", "");
			index_collapse = g_string_sized_new (GSTRING_INIT_SIZE);
			g_string_append (index_collapse, "|");
		}
	}

	g_signal_handlers_block_by_func (priv->tree_view, row_collapsed_cb, sidebar_links);
	collapse_all_recursive (GTK_TREE_VIEW (priv->tree_view), priv->model, NULL, index_collapse);
	g_signal_handlers_unblock_by_func (priv->tree_view, row_collapsed_cb, sidebar_links);

	if (index_collapse && strcmp (index_collapse->str, "|")) {
		ev_metadata_set_string (metadata, "index-collapse", index_collapse->str);
		g_string_free (index_collapse, TRUE);
	} else if (index_collapse) {
		ev_metadata_set_string (metadata, "index-collapse", "");
		g_string_free (index_collapse, TRUE);
	}
}

static void
expand_all_recursive (GtkTreeView  *tree_view,
		      GtkTreeModel *model,
		      GtkTreeIter  *parent,
		      GString *index_expand,
		      gboolean index_expand_started_empty)
{
	GtkTreePath *path;
	gchar *path_str;
	GtkTreeIter iter;
	gboolean expand;
	gboolean already_added;

	if (gtk_tree_model_iter_children (model, &iter, parent)) {
		do {
			if (!gtk_tree_model_iter_has_child (model, &iter))
				continue;

			gtk_tree_model_get (model, &iter,
					    EV_DOCUMENT_LINKS_COLUMN_EXPAND, &expand,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			if (!gtk_tree_view_row_expanded (tree_view, path))
				gtk_tree_view_expand_row (tree_view, path, FALSE);

			if (!expand && index_expand) {
				already_added = FALSE;
				path_str = gtk_tree_path_to_string (path);
				if (!index_expand_started_empty) {
					gchar *path_token;
					path_token = g_strconcat ("|", path_str, "|", NULL);

					if (g_strstr_len (index_expand->str, -1, path_token))
						already_added = TRUE;

					g_free (path_token);
				}

				if (!already_added) {
					g_string_append (index_expand, path_str);
					g_string_append (index_expand, "|");
				}
				g_free (path_str);
			}
			gtk_tree_path_free (path);

			expand_all_recursive (tree_view, model, &iter, index_expand, index_expand_started_empty);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static void
ev_links_popup_cmd_expand_all (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvSidebarLinks *sidebar_links = EV_SIDEBAR_LINKS (user_data);
	EvSidebarLinksPrivate *priv = sidebar_links->priv;
	GtkNative *window;
	EvMetadata *metadata;
	GString *index_expand = NULL;

	window = gtk_widget_get_native (GTK_WIDGET (sidebar_links));
	if (EV_IS_WINDOW (window)) {
		metadata = ev_window_get_metadata (EV_WINDOW (window));
		if (metadata) {
			ev_metadata_set_string (metadata, "index-collapse", "");
			index_expand = g_string_sized_new (GSTRING_INIT_SIZE);
			g_string_append (index_expand, "|");
		}
	}

	g_signal_handlers_block_by_func (priv->tree_view, row_expanded_cb, sidebar_links);
	expand_all_recursive (GTK_TREE_VIEW (priv->tree_view), priv->model, NULL, index_expand, TRUE);
	g_signal_handlers_unblock_by_func (priv->tree_view, row_expanded_cb, sidebar_links);

	if (index_expand && strcmp (index_expand->str, "|")) {
		ev_metadata_set_string (metadata, "index-expand", index_expand->str);
		g_string_free (index_expand, TRUE);
	} else if (index_expand) {
		ev_metadata_set_string (metadata, "index-expand", "");
		g_string_free (index_expand, TRUE);
	}
}

static void
ev_links_popup_cmd_expand_element (GSimpleAction *action,
				   GVariant      *parameter,
				   gpointer       user_data)
{
	EvSidebarLinks *sidebar_links = EV_SIDEBAR_LINKS (user_data);
	EvSidebarLinksPrivate *priv = sidebar_links->priv;
	GtkNative *window;
	GtkTreeView *treeview;
	EvMetadata *metadata;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GString *index_expand = NULL;
	gchar *index_expand_chars;

	treeview = GTK_TREE_VIEW (priv->tree_view);
	selection = gtk_tree_view_get_selection (treeview);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	path = gtk_tree_model_get_path (model, &iter);
	if (!gtk_tree_view_row_expanded (treeview, path))
		gtk_tree_view_expand_row (treeview, path, FALSE);

	window = gtk_widget_get_native (GTK_WIDGET (sidebar_links));
	if (EV_IS_WINDOW (window)) {
		metadata = ev_window_get_metadata (EV_WINDOW (window));
		if (metadata) {
			if (ev_metadata_get_string (metadata, "index-expand", &index_expand_chars))
				index_expand = g_string_new (index_expand_chars);
		}
	}

	g_signal_handlers_block_by_func (priv->tree_view, row_expanded_cb, sidebar_links);
	expand_all_recursive (GTK_TREE_VIEW (priv->tree_view), model, &iter, index_expand, FALSE);
	g_signal_handlers_unblock_by_func (priv->tree_view, row_expanded_cb, sidebar_links);

	if (index_expand && strcmp (index_expand->str, index_expand_chars))
		ev_metadata_set_string (metadata, "index-expand", index_expand->str);

	if (index_expand)
		g_string_free (index_expand, TRUE);

	gtk_tree_path_free (path);
}

static GActionGroup *
create_links_action_group (EvSidebarLinks *ev_sidebar_links) {
	const GActionEntry popup_entries[] = {
		{ "search-outline", ev_links_popup_cmd_search_outline },
		{ "print-section", ev_links_popup_cmd_print_section },
		{ "collapse-all", ev_links_popup_cmd_collapse_all },
		{ "expand-all", ev_links_popup_cmd_expand_all },
		{ "expand-element", ev_links_popup_cmd_expand_element },
	};
	GSimpleActionGroup *group;

	group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (group),
					 popup_entries,
					 G_N_ELEMENTS (popup_entries),
					 ev_sidebar_links);

	return G_ACTION_GROUP (group);
}

static void
ev_sidebar_links_init (EvSidebarLinks *ev_sidebar_links)
{
	EvSidebarLinksPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *markup;

	priv = ev_sidebar_links_get_instance_private (ev_sidebar_links);;
	ev_sidebar_links->priv = priv;

	gtk_widget_init_template (GTK_WIDGET (ev_sidebar_links));

	priv->group = create_links_action_group (ev_sidebar_links);
	gtk_widget_insert_action_group (priv->popup, "links", priv->group);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	markup = g_strdup_printf ("<span size=\"larger\" style=\"italic\">%s</span>", _("Loading…"));
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    EV_DOCUMENT_LINKS_COLUMN_MARKUP, markup,
			    EV_DOCUMENT_LINKS_COLUMN_EXPAND, FALSE,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, NULL,
			    -1);
	g_free (markup);
}

static void
ev_sidebar_links_class_init (EvSidebarLinksClass *ev_sidebar_links_class)
{
	GObjectClass   *g_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (ev_sidebar_links_class);
	widget_class = GTK_WIDGET_CLASS (ev_sidebar_links_class);

	g_object_class->set_property = ev_sidebar_links_set_property;
	g_object_class->get_property = ev_sidebar_links_get_property;
	g_object_class->dispose = ev_sidebar_links_dispose;

	widget_class->map = ev_sidebar_links_map;

	gtk_widget_class_set_template_from_resource (widget_class,
			"/org/gnome/evince/ui/sidebar-links.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarLinks, tree_view);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarLinks, popup);

	gtk_widget_class_bind_template_callback (widget_class, button_press_cb);
	gtk_widget_class_bind_template_callback (widget_class, row_collapsed_cb);
	gtk_widget_class_bind_template_callback (widget_class, row_expanded_cb);

	signals[LINK_ACTIVATED] = g_signal_new ("link-activated",
			 G_TYPE_FROM_CLASS (g_object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         G_STRUCT_OFFSET (EvSidebarLinksClass, link_activated),
		         NULL, NULL,
		         g_cclosure_marshal_VOID__OBJECT,
		         G_TYPE_NONE, 1, G_TYPE_OBJECT);

	g_object_class_install_property (g_object_class,
					 PROP_MODEL,
					 g_param_spec_object ("model",
							      "Model",
							      "Current Model",
							      GTK_TYPE_TREE_MODEL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");
	g_object_class_override_property (g_object_class, PROP_DOCUMENT_MODEL, "document-model");
}

/* Public Functions */

GtkWidget *
ev_sidebar_links_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_LINKS, NULL));
}

typedef struct EvSidebarLinkPageSearch {
	gint page;
	gint best_existing;
} EvSidebarLinkPageSearch;

static gint
page_link_tree_search_best_page (gpointer page_ptr, EvSidebarLinkPageSearch* data)
{
	gint page = GPOINTER_TO_INT (page_ptr);

	if (page <= data->page && page > data->best_existing)
		data->best_existing = page;

	return data->page - page;
}

static void
ev_sidebar_links_set_current_page (EvSidebarLinks *sidebar_links,
				   gint            current_page)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreePath *path;
	GtkTreePath *start_path = NULL;
	GtkTreePath *end_path = NULL;
	EvSidebarLinkPageSearch search_data;

	/* Widget is not currently visible */
	if (!gtk_widget_is_visible (GTK_WIDGET (sidebar_links)))
		return;

	search_data.page = current_page;
	search_data.best_existing = G_MININT;

	path = g_tree_search (sidebar_links->priv->page_link_tree, (GCompareFunc) page_link_tree_search_best_page, &search_data);
	/* No direct hit, try a lookup on the best match. */
	if (!path)
		path = g_tree_lookup (sidebar_links->priv->page_link_tree, GINT_TO_POINTER (search_data.best_existing));

	/* Still no hit, give up. */
	if (!path)
		return;

	tree_view = GTK_TREE_VIEW (sidebar_links->priv->tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	g_signal_handler_block (selection, sidebar_links->priv->selection_id);
	g_signal_handler_block (sidebar_links->priv->tree_view, sidebar_links->priv->row_activated_id);
	g_signal_handlers_block_by_func (sidebar_links->priv->tree_view, row_expanded_cb, sidebar_links);

	/* To mimic previous auto-expand behaviour, let's collapse at the moment when path is 'not
	 * visible', and thus will be revealed and focused at the start of treeview visible range */
	if (gtk_tree_view_get_visible_range (tree_view, &start_path, &end_path) &&
	    (gtk_tree_path_compare (path, start_path) < 0 || gtk_tree_path_compare (path, end_path) > 0))
		sidebar_collapse_recursive (sidebar_links);

	/* This function also scrolls the tree_view to reveal 'path' */
	gtk_tree_view_expand_to_path (tree_view, path);
	/* This functions marks 'path' selected */
	gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

	g_signal_handler_unblock (selection, sidebar_links->priv->selection_id);
	g_signal_handler_unblock (sidebar_links->priv->tree_view, sidebar_links->priv->row_activated_id);
	g_signal_handlers_unblock_by_func (sidebar_links->priv->tree_view, row_expanded_cb, sidebar_links);

	gtk_tree_path_free (start_path);
	gtk_tree_path_free (end_path);
}

static void
update_page_callback (EvSidebarLinks *sidebar_links,
		      gint            old_page,
		      gint            new_page)
{
	ev_sidebar_links_set_current_page (sidebar_links, new_page);
}

static void
row_activated_callback (GtkTreeView       *treeview,
			GtkTreePath       *arg1,
			GtkTreeViewColumn *arg2,
			gpointer           user_data)
{
	if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (treeview), arg1)) {
		gtk_tree_view_collapse_row (GTK_TREE_VIEW (treeview), arg1);
	} else {
		gtk_tree_view_expand_row (GTK_TREE_VIEW (treeview), arg1, FALSE);
	}
}

static void
expand_open_links (GtkTreeView  *tree_view,
		   GtkTreeModel *model,
		   GtkTreeIter  *parent,
		   gchar        *index_expand,
		   gchar        *index_collapse)
{
	GtkTreeIter iter;
	gboolean expand;

	if (gtk_tree_model_iter_children (model, &iter, parent)) {
		do {
			GtkTreePath *path;
			gchar *path_str, *path_token;

			gtk_tree_model_get (model, &iter,
					    EV_DOCUMENT_LINKS_COLUMN_EXPAND, &expand,
					    -1);
			path = gtk_tree_model_get_path (model, &iter);

			if (expand) {
				if (index_collapse) {
					path_str = gtk_tree_path_to_string (path);
					path_token = g_strconcat ("|", path_str, "|", NULL);

					if (!g_strstr_len (index_collapse, -1, path_token))
						gtk_tree_view_expand_row (tree_view, path, FALSE);

					g_free (path_str);
					g_free (path_token);
				}
				else
					gtk_tree_view_expand_row (tree_view, path, FALSE);
			} else if (index_expand) {
				path_str = gtk_tree_path_to_string (path);
				path_token = g_strconcat ("|", path_str, "|", NULL);

				if (g_strstr_len (index_expand, -1, path_token))
					gtk_tree_view_expand_to_path (tree_view, path);

				g_free (path_str);
				g_free (path_token);
			}
			gtk_tree_path_free (path);
			expand_open_links (tree_view, model, &iter, index_expand, index_collapse);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}


static gint
page_link_tree_sort (gconstpointer a, gconstpointer b, void *data)
{
	gint a_int = GPOINTER_TO_INT (a);
	gint b_int = GPOINTER_TO_INT (b);

	return (a_int < b_int) ? -1 : (a_int > b_int);
}

static gboolean
update_page_link_tree_foreach (GtkTreeModel *model,
			       GtkTreePath  *path,
			       GtkTreeIter  *iter,
			       gpointer      data)
{
	EvSidebarLinks *sidebar_links = data;
	EvSidebarLinksPrivate *priv = sidebar_links->priv;
	EvDocumentLinks *document_links = EV_DOCUMENT_LINKS (priv->document);
	EvLink *link;
	int page;

	gtk_tree_model_get (model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);

	if (!link)
		return FALSE;

	page = ev_document_links_get_link_page (document_links, link);
	g_object_unref (link);

	/* Only save the first link we find per page. */
	if (!g_tree_lookup (priv->page_link_tree, GINT_TO_POINTER (page)))
		g_tree_insert (priv->page_link_tree, GINT_TO_POINTER (page), gtk_tree_path_copy (path));

	return FALSE;
}

static void
ev_sidebar_links_set_links_model (EvSidebarLinks *sidebar_links,
				  GtkTreeModel   *model)
{
	EvSidebarLinksPrivate *priv = sidebar_links->priv;

	if (priv->model == model)
		return;

	if (priv->model)
		g_object_unref (priv->model);
	priv->model = g_object_ref (model);

	/* Rebuild the binary search tree for finding links on pages. */
	if (priv->page_link_tree)
		g_tree_unref (priv->page_link_tree);
	priv->page_link_tree = g_tree_new_full (page_link_tree_sort, NULL, NULL, (GDestroyNotify) gtk_tree_path_free);

	gtk_tree_model_foreach (model,
				update_page_link_tree_foreach,
				sidebar_links);

	g_object_notify (G_OBJECT (sidebar_links), "model");
}

static void
job_finished_callback (EvJobLinks     *job,
		       EvSidebarLinks *sidebar_links)
{
	EvSidebarLinksPrivate *priv = sidebar_links->priv;
	GtkTreeSelection *selection;
	GtkNative *window;
	EvMetadata *metadata;
	gchar *index_expand = NULL;
	gchar *index_collapse = NULL;

	ev_sidebar_links_set_links_model (sidebar_links, job->model);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), job->model);

	g_clear_object (&priv->job);

	window = gtk_widget_get_native (GTK_WIDGET (sidebar_links));
	if (EV_IS_WINDOW (window)) {
		metadata = ev_window_get_metadata (EV_WINDOW (window));
		if (metadata) {
			ev_metadata_get_string (metadata, "index-expand", &index_expand);
			ev_metadata_get_string (metadata, "index-collapse", &index_collapse);
		}
	}

	g_signal_handlers_block_by_func (priv->tree_view, row_expanded_cb, sidebar_links);
	expand_open_links (GTK_TREE_VIEW (priv->tree_view), priv->model, NULL, index_expand, index_collapse);
	g_signal_handlers_unblock_by_func (priv->tree_view, row_expanded_cb, sidebar_links);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	if (priv->selection_id <= 0) {
		priv->selection_id =
			g_signal_connect (selection, "changed",
					  G_CALLBACK (selection_changed_callback),
					  sidebar_links);
	}
	priv->page_changed_id =
		g_signal_connect_swapped (priv->doc_model, "page-changed",
					  G_CALLBACK (update_page_callback),
					  sidebar_links);
	if (priv->row_activated_id <= 0) {
		priv->row_activated_id =
			g_signal_connect (priv->tree_view, "row-activated",
					  G_CALLBACK (row_activated_callback),
					  sidebar_links);
	}

	ev_sidebar_links_set_current_page (sidebar_links,
					   ev_document_model_get_page (priv->doc_model));
}

static void
ev_sidebar_links_document_changed_cb (EvDocumentModel *model,
				      GParamSpec      *pspec,
				      EvSidebarLinks  *sidebar_links)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvSidebarLinksPrivate *priv = sidebar_links->priv;

	if (!EV_IS_DOCUMENT_LINKS (document))
		return;

	if (!ev_document_links_has_document_links (EV_DOCUMENT_LINKS (document)))
		return;

	if (priv->document) {
		gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), NULL);
		g_object_unref (priv->document);
	}

	priv->document = g_object_ref (document);

	if (priv->job) {
		g_signal_handlers_disconnect_by_func (priv->job,
						      job_finished_callback,
						      sidebar_links);
		g_object_unref (priv->job);
	}

	priv->job = ev_job_links_new (document);
	g_signal_connect (priv->job,
			  "finished",
			  G_CALLBACK (job_finished_callback),
			  sidebar_links);
	/* The priority doesn't matter for this job */
	ev_job_scheduler_push_job (priv->job, EV_JOB_PRIORITY_NONE);
}

static void
ev_sidebar_links_set_model (EvSidebarPage   *sidebar_page,
			    EvDocumentModel *model)
{
	EvSidebarLinks *sidebar_links = EV_SIDEBAR_LINKS (sidebar_page);
	EvSidebarLinksPrivate *priv = sidebar_links->priv;

	if (priv->doc_model == model)
		return;

	priv->doc_model = model;
	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_links_document_changed_cb),
			  sidebar_page);
}

static gboolean
ev_sidebar_links_support_document (EvSidebarPage  *sidebar_page,
				   EvDocument *document)
{
	return (EV_IS_DOCUMENT_LINKS (document) &&
		    ev_document_links_has_document_links (EV_DOCUMENT_LINKS (document)));
}

static void
ev_sidebar_links_page_iface_init (EvSidebarPageInterface *iface)
{
	iface->support_document = ev_sidebar_links_support_document;
	iface->set_model = ev_sidebar_links_set_model;
}
