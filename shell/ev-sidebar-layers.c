/* ev-sidebar-layers.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include "config.h"

#include <glib/gi18n.h>

#include "ev-document-layers.h"
#include "ev-sidebar-page.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-sidebar-layers.h"

struct _EvSidebarLayersPrivate {
	GtkTreeView  *tree_view;

	EvDocument   *document;
	EvJob        *job;
};

enum {
	PROP_0,
	PROP_WIDGET,
	PROP_DOCUMENT_MODEL,
};

enum {
	LAYERS_VISIBILITY_CHANGED,
	N_SIGNALS
};

static void ev_sidebar_layers_page_iface_init (EvSidebarPageInterface *iface);
static void job_finished_callback             (EvJobLayers            *job,
					       EvSidebarLayers        *sidebar_layers);
static void ev_sidebar_layers_set_model (EvSidebarPage   *sidebar_page,
					 EvDocumentModel *model);

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED (EvSidebarLayers,
                        ev_sidebar_layers,
                        GTK_TYPE_BOX,
                        0,
                        G_ADD_PRIVATE (EvSidebarLayers)
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
					       ev_sidebar_layers_page_iface_init))

static void
ev_sidebar_layers_dispose (GObject *object)
{
	EvSidebarLayers *sidebar = EV_SIDEBAR_LAYERS (object);

	if (sidebar->priv->job) {
		g_signal_handlers_disconnect_by_func (sidebar->priv->job,
						      job_finished_callback,
						      sidebar);
		ev_job_cancel (sidebar->priv->job);
		g_clear_object (&sidebar->priv->job);
	}

	g_clear_object (&sidebar->priv->document);

	G_OBJECT_CLASS (ev_sidebar_layers_parent_class)->dispose (object);
}

static void
ev_sidebar_layers_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	EvSidebarLayers *ev_sidebar_layers;

	ev_sidebar_layers = EV_SIDEBAR_LAYERS (object);

	switch (prop_id) {
	        case PROP_WIDGET:
			g_value_set_object (value, ev_sidebar_layers->priv->tree_view);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ev_sidebar_layers_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	EvSidebarLayers *sidebar_layers = EV_SIDEBAR_LAYERS (object);

	switch (prop_id)
	{
	case PROP_DOCUMENT_MODEL:
		ev_sidebar_layers_set_model (EV_SIDEBAR_PAGE (sidebar_layers),
			EV_DOCUMENT_MODEL (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GtkTreeModel *
ev_sidebar_layers_create_loading_model (void)
{
	GtkTreeModel *retval;
	GtkTreeIter   iter;
	gchar        *markup;

	/* Creates a fake model to indicate that we're loading */
	retval = (GtkTreeModel *)gtk_list_store_new (EV_DOCUMENT_LAYERS_N_COLUMNS,
						     G_TYPE_STRING,
						     G_TYPE_OBJECT,
						     G_TYPE_BOOLEAN,
						     G_TYPE_BOOLEAN,
						     G_TYPE_BOOLEAN,
						     G_TYPE_INT);

	gtk_list_store_append (GTK_LIST_STORE (retval), &iter);
	markup = g_strdup_printf ("<span size=\"larger\" style=\"italic\">%s</span>", _("Loading…"));
	gtk_list_store_set (GTK_LIST_STORE (retval), &iter,
			    EV_DOCUMENT_LAYERS_COLUMN_TITLE, markup,
			    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, FALSE,
			    EV_DOCUMENT_LAYERS_COLUMN_ENABLED, TRUE,
			    EV_DOCUMENT_LAYERS_COLUMN_SHOWTOGGLE, FALSE,
			    EV_DOCUMENT_LAYERS_COLUMN_RBGROUP, -1,
			    EV_DOCUMENT_LAYERS_COLUMN_LAYER, NULL,
			    -1);
	g_free (markup);

	return retval;
}

static gboolean
update_kids (GtkTreeModel *model,
	     GtkTreePath  *path,
	     GtkTreeIter  *iter,
	     GtkTreeIter  *parent)
{
	if (gtk_tree_store_is_ancestor (GTK_TREE_STORE (model), parent, iter)) {
		gboolean visible;

		gtk_tree_model_get (model, parent,
				    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, &visible,
				    -1);
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    EV_DOCUMENT_LAYERS_COLUMN_ENABLED, visible,
				    -1);
	}

	return FALSE;
}

static gboolean
clear_rb_group (GtkTreeModel *model,
		GtkTreePath  *path,
		GtkTreeIter  *iter,
		gint         *rb_group)
{
	gint group;

	gtk_tree_model_get (model, iter,
			    EV_DOCUMENT_LAYERS_COLUMN_RBGROUP, &group,
			    -1);

	if (group == *rb_group) {
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, FALSE,
				    -1);
	}

	return FALSE;
}

static void
ev_sidebar_layers_visibility_toggled (GtkCellRendererToggle *cell,
				      gchar                 *path_str,
				      EvSidebarLayers       *ev_layers)
{
	GtkTreeModel *model;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	gboolean      visible;
	EvLayer      *layer;

	model = gtk_tree_view_get_model (ev_layers->priv->tree_view);

	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, &visible,
			    EV_DOCUMENT_LAYERS_COLUMN_LAYER, &layer,
			    -1);

	visible = !visible;
	if (visible) {
		gint rb_group;

		ev_document_layers_show_layer (EV_DOCUMENT_LAYERS (ev_layers->priv->document),
					       layer);

		rb_group = ev_layer_get_rb_group (layer);
		if (rb_group) {
			gtk_tree_model_foreach (model,
						(GtkTreeModelForeachFunc)clear_rb_group,
						&rb_group);
		}
	} else {
		ev_document_layers_hide_layer (EV_DOCUMENT_LAYERS (ev_layers->priv->document),
					       layer);
	}

	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, visible,
			    -1);

	if (ev_layer_is_parent (layer)) {
		gtk_tree_model_foreach (model,
					(GtkTreeModelForeachFunc)update_kids,
					&iter);
	}

	gtk_tree_path_free (path);

	g_signal_emit (ev_layers, signals[LAYERS_VISIBILITY_CHANGED], 0);
}

static GtkTreeView *
ev_sidebar_layers_create_tree_view (EvSidebarLayers *ev_layers)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *renderer;

	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (tree_view),
				     GTK_SELECTION_NONE);


	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "active", EV_DOCUMENT_LAYERS_COLUMN_VISIBLE,
					     "activatable", EV_DOCUMENT_LAYERS_COLUMN_ENABLED,
					     "visible", EV_DOCUMENT_LAYERS_COLUMN_SHOWTOGGLE,
					     "sensitive", EV_DOCUMENT_LAYERS_COLUMN_ENABLED,
					     NULL);
	g_object_set (G_OBJECT (renderer),
		      "xpad", 0,
		      "ypad", 0,
		      NULL);

	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (ev_sidebar_layers_visibility_toggled),
			  (gpointer)ev_layers);


	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "markup", EV_DOCUMENT_LAYERS_COLUMN_TITLE,
					     "sensitive", EV_DOCUMENT_LAYERS_COLUMN_ENABLED,
					     NULL);
	g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_tree_view_append_column (tree_view, column);

	return tree_view;
}

static void
ev_sidebar_layers_init (EvSidebarLayers *ev_layers)
{
	GtkWidget    *swindow;
	GtkTreeModel *model;

	ev_layers->priv = ev_sidebar_layers_get_instance_private (ev_layers);

	swindow = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (swindow, TRUE);
	gtk_widget_set_hexpand (swindow, TRUE);

	/* Data Model */
	model = ev_sidebar_layers_create_loading_model ();

	/* Layers list */
	ev_layers->priv->tree_view = ev_sidebar_layers_create_tree_view (ev_layers);
	gtk_tree_view_set_model (ev_layers->priv->tree_view, model);
	g_object_unref (model);

	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow),
		GTK_WIDGET (ev_layers->priv->tree_view));

        gtk_box_prepend (GTK_BOX (ev_layers), swindow);
}

static void
ev_sidebar_layers_class_init (EvSidebarLayersClass *ev_layers_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_layers_class);

	g_object_class->get_property = ev_sidebar_layers_get_property;
	g_object_class->set_property = ev_sidebar_layers_set_property;
	g_object_class->dispose = ev_sidebar_layers_dispose;

	g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");
	g_object_class_override_property (g_object_class, PROP_DOCUMENT_MODEL, "document-model");

	signals[LAYERS_VISIBILITY_CHANGED] =
		g_signal_new ("layers_visibility_changed",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarLayersClass, layers_visibility_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0, G_TYPE_NONE);
}

GtkWidget *
ev_sidebar_layers_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_LAYERS,
				   "orientation", GTK_ORIENTATION_VERTICAL,
                                         NULL));
}

static void
update_layers_state (GtkTreeModel     *model,
		     GtkTreeIter      *iter,
		     EvDocumentLayers *document_layers)
{
	EvLayer    *layer;
	gboolean    visible;
	GtkTreeIter child_iter;

	do {
		gtk_tree_model_get (model, iter,
				    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, &visible,
				    EV_DOCUMENT_LAYERS_COLUMN_LAYER, &layer,
				    -1);
		if (layer) {
			gboolean layer_visible;

			layer_visible = ev_document_layers_layer_is_visible (document_layers, layer);
			if (layer_visible != visible) {
				gtk_tree_store_set (GTK_TREE_STORE (model), iter,
						    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, layer_visible,
						    -1);
			}
		}

		if (gtk_tree_model_iter_children (model, &child_iter, iter))
			update_layers_state (model, &child_iter, document_layers);
	} while (gtk_tree_model_iter_next (model, iter));
}

void
ev_sidebar_layers_update_layers_state (EvSidebarLayers *sidebar_layers)
{
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	EvDocumentLayers *document_layers;

	document_layers = EV_DOCUMENT_LAYERS (sidebar_layers->priv->document);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (sidebar_layers->priv->tree_view));
	if (gtk_tree_model_get_iter_first (model, &iter))
		update_layers_state (model, &iter, document_layers);
}

static void
job_finished_callback (EvJobLayers     *job,
		       EvSidebarLayers *sidebar_layers)
{
	EvSidebarLayersPrivate *priv;

	priv = sidebar_layers->priv;

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), job->model);

	g_clear_object (&priv->job);
}

static void
ev_sidebar_layers_document_changed_cb (EvDocumentModel *model,
				       GParamSpec      *pspec,
				       EvSidebarLayers *sidebar_layers)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvSidebarLayersPrivate *priv = sidebar_layers->priv;

	if (!EV_IS_DOCUMENT_LAYERS (document))
		return;

	if (priv->document) {
		gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), NULL);
		g_object_unref (priv->document);
	}

	priv->document = g_object_ref (document);

	if (priv->job) {
		g_signal_handlers_disconnect_by_func (priv->job,
						      job_finished_callback,
						      sidebar_layers);
		g_object_unref (priv->job);
	}

	priv->job = ev_job_layers_new (document);
	g_signal_connect (priv->job, "finished",
			  G_CALLBACK (job_finished_callback),
			  sidebar_layers);
	/* The priority doesn't matter for this job */
	ev_job_scheduler_push_job (priv->job, EV_JOB_PRIORITY_NONE);
}

static void
ev_sidebar_layers_set_model (EvSidebarPage   *sidebar_page,
			     EvDocumentModel *model)
{
	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_layers_document_changed_cb),
			  sidebar_page);
}

static gboolean
ev_sidebar_layers_support_document (EvSidebarPage *sidebar_page,
				    EvDocument    *document)
{
	return (EV_IS_DOCUMENT_LAYERS (document) &&
		ev_document_layers_has_layers (EV_DOCUMENT_LAYERS (document)));
}

static void
ev_sidebar_layers_page_iface_init (EvSidebarPageInterface *iface)
{
	iface->support_document = ev_sidebar_layers_support_document;
	iface->set_model = ev_sidebar_layers_set_model;
}
