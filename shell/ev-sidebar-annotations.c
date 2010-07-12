/* ev-sidebar-annotations.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include "config.h"

#include <glib/gi18n.h>

#include "ev-document-annotations.h"
#include "ev-sidebar-page.h"
#include "ev-sidebar-annotations.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-stock-icons.h"

enum {
	PROP_0,
	PROP_WIDGET
};

enum {
	COLUMN_MARKUP,
	COLUMN_ICON,
	COLUMN_ANNOT_MAPPING,
	N_COLUMNS
};

enum {
	ANNOT_ACTIVATED,
	N_SIGNALS
};

struct _EvSidebarAnnotationsPrivate {
	GtkWidget  *notebook;
	GtkWidget  *tree_view;

	EvJob      *job;
	guint       selection_changed_id;
};

static void ev_sidebar_annotations_page_iface_init (EvSidebarPageInterface *iface);

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED (EvSidebarAnnotations,
                        ev_sidebar_annotations,
                        GTK_TYPE_VBOX,
                        0,
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
					       ev_sidebar_annotations_page_iface_init))

#define EV_SIDEBAR_ANNOTATIONS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_ANNOTATIONS, EvSidebarAnnotationsPrivate))

static GtkTreeModel *
ev_sidebar_annotations_create_simple_model (const gchar *message)
{
	GtkTreeModel *retval;
	GtkTreeIter iter;
	gchar *markup;

	/* Creates a fake model to indicate that we're loading */
	retval = (GtkTreeModel *)gtk_list_store_new (N_COLUMNS,
						     G_TYPE_STRING,
						     GDK_TYPE_PIXBUF,
						     G_TYPE_POINTER);

	gtk_list_store_append (GTK_LIST_STORE (retval), &iter);
	markup = g_strdup_printf ("<span size=\"larger\" style=\"italic\">%s</span>",
				  message);
	gtk_list_store_set (GTK_LIST_STORE (retval), &iter,
			    COLUMN_MARKUP, markup,
			    -1);
	g_free (markup);

	return retval;
}

static void
ev_sidebar_annotations_add_annots_list (EvSidebarAnnotations *ev_annots)
{
	GtkWidget         *swindow;
	GtkTreeModel      *loading_model;
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection  *selection;

	swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);

	/* Create tree view */
	loading_model = ev_sidebar_annotations_create_simple_model (_("Loading…"));
	ev_annots->priv->tree_view = gtk_tree_view_new_with_model (loading_model);
	g_object_unref (loading_model);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ev_annots->priv->tree_view),
					   FALSE);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ev_annots->priv->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "pixbuf", COLUMN_ICON,
					     NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "markup", COLUMN_MARKUP,
					     NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (ev_annots->priv->tree_view),
				     column);

	gtk_container_add (GTK_CONTAINER (swindow), ev_annots->priv->tree_view);
	gtk_widget_show (ev_annots->priv->tree_view);

	gtk_notebook_append_page (GTK_NOTEBOOK (ev_annots->priv->notebook),
				  swindow, NULL);
	gtk_widget_show (swindow);
}

static void
ev_sidebar_annotations_init (EvSidebarAnnotations *ev_annots)
{
	ev_annots->priv = EV_SIDEBAR_ANNOTATIONS_GET_PRIVATE (ev_annots);

	ev_annots->priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (ev_annots->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (ev_annots->priv->notebook), FALSE);
	ev_sidebar_annotations_add_annots_list (ev_annots);
	gtk_container_add (GTK_CONTAINER (ev_annots), ev_annots->priv->notebook);
	gtk_widget_show (ev_annots->priv->notebook);
}

static void
ev_sidebar_annotations_get_property (GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
	EvSidebarAnnotations *ev_sidebar_annots;

	ev_sidebar_annots = EV_SIDEBAR_ANNOTATIONS (object);

	switch (prop_id) {
	        case PROP_WIDGET:
			g_value_set_object (value, ev_sidebar_annots->priv->notebook);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ev_sidebar_annotations_class_init (EvSidebarAnnotationsClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->get_property = ev_sidebar_annotations_get_property;

	g_type_class_add_private (g_object_class, sizeof (EvSidebarAnnotationsPrivate));

	g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");

	signals[ANNOT_ACTIVATED] =
		g_signal_new ("annot-activated",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarAnnotationsClass, annot_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

GtkWidget *
ev_sidebar_annotations_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_ANNOTATIONS, NULL));
}

static void
selection_changed_cb (GtkTreeSelection     *selection,
		      EvSidebarAnnotations *sidebar_annots)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EvMapping *mapping = NULL;

		gtk_tree_model_get (model, &iter,
				    COLUMN_ANNOT_MAPPING, &mapping,
				    -1);
		if (mapping)
			g_signal_emit (sidebar_annots, signals[ANNOT_ACTIVATED], 0, mapping);
	}
}

static void
job_finished_callback (EvJobAnnots          *job,
		       EvSidebarAnnotations *sidebar_annots)
{
	EvSidebarAnnotationsPrivate *priv;
	GtkTreeStore *model;
	GtkTreeSelection *selection;
	GList *l;
	GdkPixbuf *text_icon = NULL;
	GdkPixbuf *attachment_icon = NULL;

	priv = sidebar_annots->priv;

	if (!job->annots) {
		GtkTreeModel *list;

		list = ev_sidebar_annotations_create_simple_model (_("Document contains no annotations"));
		gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), list);
		g_object_unref (list);

		g_object_unref (job);
		priv->job = NULL;

		return;
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	if (priv->selection_changed_id == 0) {
		priv->selection_changed_id =
			g_signal_connect (selection, "changed",
					  G_CALLBACK (selection_changed_cb),
					  sidebar_annots);
	}

	model = gtk_tree_store_new (N_COLUMNS,
				    G_TYPE_STRING,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_POINTER);

	for (l = job->annots; l; l = g_list_next (l)) {
		EvMappingList *mapping_list;
		GList         *ll;
		gchar         *page_label;
		GtkTreeIter    iter;
		gboolean       found = FALSE;

		mapping_list = (EvMappingList *)l->data;
		page_label = g_strdup_printf (_("Page %d"),
					      ev_mapping_list_get_page (mapping_list) + 1);
		gtk_tree_store_append (model, &iter, NULL);
		gtk_tree_store_set (model, &iter,
				    COLUMN_MARKUP, page_label,
				    -1);
		g_free (page_label);

		for (ll = ev_mapping_list_get_list (mapping_list); ll; ll = g_list_next (ll)) {
			EvAnnotation *annot;
			gchar        *label;
			gchar        *markup;
			GtkTreeIter   child_iter;
			GdkPixbuf    *pixbuf = NULL;

			annot = ((EvMapping *)(ll->data))->data;
			if (!EV_IS_ANNOTATION_MARKUP (annot))
				continue;

			label = ev_annotation_markup_get_label (EV_ANNOTATION_MARKUP (annot));
			if (annot->modified) {
				markup = g_strdup_printf ("<span weight=\"bold\">%s</span>\n%s",
							  label, annot->modified);
			} else {
				markup = g_strdup_printf ("<span weight=\"bold\">%s</span>", label);
			}
			g_free (label);

			if (EV_IS_ANNOTATION_TEXT (annot)) {
				if (!text_icon) {
					/* FIXME: use a better icon than EDIT */
					text_icon = gtk_widget_render_icon (priv->tree_view,
									    GTK_STOCK_EDIT,
									    GTK_ICON_SIZE_BUTTON,
									    NULL);
				}
				pixbuf = text_icon;
			} else if (EV_IS_ANNOTATION_ATTACHMENT (annot)) {
				if (!attachment_icon) {
					attachment_icon = gtk_widget_render_icon (priv->tree_view,
										  EV_STOCK_ATTACHMENT,
										  GTK_ICON_SIZE_BUTTON,
										  NULL);
				}
				pixbuf = attachment_icon;
			}

			gtk_tree_store_append (model, &child_iter, &iter);
			gtk_tree_store_set (model, &child_iter,
					    COLUMN_MARKUP, markup,
					    COLUMN_ICON, pixbuf,
					    COLUMN_ANNOT_MAPPING, ll->data,
					    -1);
			g_free (markup);
			found = TRUE;
		}

		if (!found)
			gtk_tree_store_remove (model, &iter);
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view),
				 GTK_TREE_MODEL (model));
	g_object_unref (model);

	if (text_icon)
		g_object_unref (text_icon);
	if (attachment_icon)
		g_object_unref (attachment_icon);

	g_object_unref (job);
	priv->job = NULL;
}


static void
ev_sidebar_annotations_document_changed_cb (EvDocumentModel      *model,
					    GParamSpec           *pspec,
					    EvSidebarAnnotations *sidebar_annots)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvSidebarAnnotationsPrivate *priv = sidebar_annots->priv;

	if (!EV_IS_DOCUMENT_ANNOTATIONS (document))
		return;

	if (priv->job) {
		g_signal_handlers_disconnect_by_func (priv->job,
						      job_finished_callback,
						      sidebar_annots);
		g_object_unref (priv->job);
	}

	priv->job = ev_job_annots_new (document);
	g_signal_connect (priv->job, "finished",
			  G_CALLBACK (job_finished_callback),
			  sidebar_annots);
	/* The priority doesn't matter for this job */
	ev_job_scheduler_push_job (priv->job, EV_JOB_PRIORITY_NONE);
}

/* EvSidebarPageIface */
static void
ev_sidebar_annotations_set_model (EvSidebarPage   *sidebar_page,
				  EvDocumentModel *model)
{
	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_annotations_document_changed_cb),
			  sidebar_page);
}

static gboolean
ev_sidebar_annotations_support_document (EvSidebarPage *sidebar_page,
					 EvDocument    *document)
{
	return (EV_IS_DOCUMENT_ANNOTATIONS (document));
}

static const gchar *
ev_sidebar_annotations_get_label (EvSidebarPage *sidebar_page)
{
	return _("Annotations");
}

static void
ev_sidebar_annotations_page_iface_init (EvSidebarPageInterface *iface)
{
	iface->support_document = ev_sidebar_annotations_support_document;
	iface->set_model = ev_sidebar_annotations_set_model;
	iface->get_label = ev_sidebar_annotations_get_label;
}
