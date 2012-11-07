/* ev-sidebar-find-results.c
* this file is part of evince, a gnome document viewer
*
* Copyright (C) 2008 Sergey Pushkin < pushkinsv@gmail.com >
*
* Evince is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* Evince is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 - 1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "ev-sidebar-find-results.h"
#include "ev-sidebar-page.h"

struct _EvSidebarFindResultsPrivate {
	GtkWidget *tree_view;

	guint selection_id;

	EvJob *job;
};

enum {
	PROP_0,
	PROP_WIDGET,
};

enum {
	FIND_RESULT_ACTIVATED,
	N_SIGNALS
};

static void ev_sidebar_find_results_page_iface_init (EvSidebarPageInterface *iface);
static void ev_sidebar_find_results_set_model (EvSidebarPage *sidebar_page,
						EvDocumentModel *model);
static gboolean ev_sidebar_find_results_support_document (EvSidebarPage *sidebar_page,
						EvDocument *document);
static const gchar* ev_sidebar_find_results_get_label (EvSidebarPage *sidebar_page);

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED (EvSidebarFindResults,
ev_sidebar_find_results,
GTK_TYPE_VBOX,
0,
G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
					ev_sidebar_find_results_page_iface_init))


#define EV_SIDEBAR_FIND_RESULTS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_FIND_RESULTS, EvSidebarFindResultsPrivate))

static void
ev_sidebar_find_results_get_property (GObject *object,
			guint prop_id,
			GValue *value,
			GParamSpec *pspec)
{
	EvSidebarFindResults *ev_sidebar_find_results;

	ev_sidebar_find_results = EV_SIDEBAR_FIND_RESULTS (object);

	switch (prop_id) {
	case PROP_WIDGET:
		g_value_set_object (value, ev_sidebar_find_results->priv->tree_view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}


static void
ev_sidebar_find_results_dispose (GObject *object)
{
	EvSidebarFindResults *sidebar = EV_SIDEBAR_FIND_RESULTS (object);

	if (sidebar->priv->job) {
		g_object_unref (sidebar->priv->job);
		sidebar->priv->job = NULL;
	};

	G_OBJECT_CLASS (ev_sidebar_find_results_parent_class)->dispose (object);
}

static void
ev_sidebar_find_results_map (GtkWidget *widget)
{
	EvSidebarFindResults *find_results;

	find_results = EV_SIDEBAR_FIND_RESULTS (widget);

	GTK_WIDGET_CLASS (ev_sidebar_find_results_parent_class)->map (widget);
}

static void
ev_sidebar_find_results_class_init (EvSidebarFindResultsClass *sidebar_find_results_class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (sidebar_find_results_class);
	widget_class = GTK_WIDGET_CLASS (sidebar_find_results_class);

	g_object_class->get_property = ev_sidebar_find_results_get_property;
	g_object_class->dispose = ev_sidebar_find_results_dispose;

	widget_class->map = ev_sidebar_find_results_map;

	signals[FIND_RESULT_ACTIVATED] = g_signal_new ("find-result-activated",
		G_TYPE_FROM_CLASS (g_object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EvSidebarFindResultsClass, find_result_activated),
		NULL, NULL,
		g_cclosure_marshal_generic,
		G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT);

	g_object_class_override_property (g_object_class,
					PROP_WIDGET,
					"main-widget");

	g_type_class_add_private (g_object_class, sizeof (EvSidebarFindResultsPrivate));
}

static void
selection_changed_callback (GtkTreeSelection *selection,
		EvSidebarFindResults *sidebar_find_results)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	EvJobFind *job_find;

	job_find = EV_JOB_FIND (sidebar_find_results->priv->job);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gint pageno;
		gint resultno;

		gtk_tree_model_get (model, &iter,
				EV_DOCUMENT_FIND_RESULTS_COLUMN_PAGENO, &pageno,
				- 1);
		gtk_tree_model_get (model, &iter,
				EV_DOCUMENT_FIND_RESULTS_COLUMN_RESULTNO, &resultno,
				- 1);

		g_signal_emit (sidebar_find_results, signals[FIND_RESULT_ACTIVATED], 0, job_find, pageno - 1, resultno);
	}
}

static gboolean
focus_out_cb (GtkWidget *treeview,
GdkEventButton *event,
EvSidebarFindResults *sidebar_find_results)
{
	g_signal_emit (sidebar_find_results, signals[FIND_RESULT_ACTIVATED], 0, NULL, 0, 0);
	return FALSE;
}

static gboolean
focus_in_cb (GtkWidget *treeview,
GdkEventButton *event,
EvSidebarFindResults *sidebar_find_results)
{
	GtkTreeSelection *selection;
	EvSidebarFindResultsPrivate *priv;
	priv = sidebar_find_results->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	selection_changed_callback (selection, sidebar_find_results);
	return FALSE;
}




static void
ev_sidebar_find_results_construct (EvSidebarFindResults *ev_sidebar_find_results)
{
	EvSidebarFindResultsPrivate *priv;
	GtkWidget *swindow;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	priv = ev_sidebar_find_results->priv;

	swindow = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					GTK_SHADOW_IN);

	priv->tree_view = gtk_tree_view_new_with_model (NULL);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
	gtk_container_add (GTK_CONTAINER (swindow), priv->tree_view);

	gtk_box_pack_start (GTK_BOX (ev_sidebar_find_results), swindow, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (ev_sidebar_find_results));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view), column);

	renderer = (GtkCellRenderer*) g_object_new (
			GTK_TYPE_CELL_RENDERER_TEXT,
			"ellipsize",
			PANGO_ELLIPSIZE_END,
			NULL);
	gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), renderer, TRUE);
	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
					"markup", EV_DOCUMENT_FIND_RESULTS_COLUMN_TEXT,
					NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
					"text", EV_DOCUMENT_FIND_RESULTS_COLUMN_PAGENO,
					NULL);
	g_object_set (G_OBJECT (renderer), "style", PANGO_STYLE_ITALIC, NULL);

	g_signal_connect (GTK_TREE_VIEW (priv->tree_view),
			"focus_out_event",
			G_CALLBACK (focus_out_cb),
			ev_sidebar_find_results);
	g_signal_connect (GTK_TREE_VIEW (priv->tree_view),
			"focus_in_event",
			G_CALLBACK (focus_in_cb),
			ev_sidebar_find_results);

	priv->selection_id = g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view)), "changed",
					G_CALLBACK (selection_changed_callback),
					ev_sidebar_find_results);
}

static void
ev_sidebar_find_results_init (EvSidebarFindResults *ev_sidebar_find_results)
{
	ev_sidebar_find_results->priv = EV_SIDEBAR_FIND_RESULTS_GET_PRIVATE (ev_sidebar_find_results);

	ev_sidebar_find_results_construct (ev_sidebar_find_results);
}

GtkWidget *
ev_sidebar_find_results_new (void)
{
	GtkWidget *ev_sidebar_find_results;

	ev_sidebar_find_results = g_object_new (EV_TYPE_SIDEBAR_FIND_RESULTS, NULL);

	return ev_sidebar_find_results;
}


void
find_result_activate_result (EvSidebarFindResults *sidebar_find_results,
			gpointer results,
			gint pageno,
			gint resultno)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EvSidebarFindResultsPrivate *priv;
	EvJobFind *job_find;
	gint i, index;
	GtkTreePath *path;

	if (!gtk_widget_get_mapped (GTK_WIDGET (sidebar_find_results)))
		return;

	priv = sidebar_find_results->priv;
	if (priv->job == NULL) return;
	job_find = EV_JOB_FIND (priv->job);
	if (ev_job_find_get_n_results (job_find, pageno) == 0) return;

	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sidebar_find_results->priv->tree_view));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gint page;

		gtk_tree_model_get (model, &iter,
				EV_DOCUMENT_FIND_RESULTS_COLUMN_PAGENO, &page,
				- 1);
	};

	index = resultno;
	for (i = 0; i < pageno; i++ )
		index += ev_job_find_get_n_results (job_find, i);

	path = gtk_tree_path_new_from_indices (index, -1);
	g_signal_handler_block (selection, sidebar_find_results->priv->selection_id);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view), path, NULL, FALSE);
	g_signal_handler_unblock (selection, sidebar_find_results->priv->selection_id);
	gtk_tree_path_free (path);
}

static void
ev_sidebar_find_results_set_model (EvSidebarPage *sidebar_page,
			EvDocumentModel *model)
{
}

void
ev_sidebar_find_results_update (EvSidebarFindResults *sidebar_find_results,
			EvJobFind *job_find)
{
	EvSidebarFindResultsPrivate *priv;

	priv = sidebar_find_results->priv;

	if (priv->job) g_object_unref (priv->job);
	priv->job = g_object_ref (job_find);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), job_find->model);
}

static gboolean
ev_sidebar_find_results_support_document (EvSidebarPage *sidebar_page,
				EvDocument *document)
{
	return TRUE;
}

static const gchar*
ev_sidebar_find_results_get_label (EvSidebarPage *sidebar_page)
{
	return _ ("Find results");
}

static void
ev_sidebar_find_results_page_iface_init (EvSidebarPageInterface *iface)
{
	iface->support_document = ev_sidebar_find_results_support_document;
	iface->set_model = ev_sidebar_find_results_set_model;
	iface->get_label = ev_sidebar_find_results_get_label;
}

