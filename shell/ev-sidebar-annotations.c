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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
	BEGIN_ANNOT_ADD,
	ANNOT_ADD_CANCELLED,
	N_SIGNALS
};

struct _EvSidebarAnnotationsPrivate {
	EvDocument  *document;

	GtkWidget   *notebook;
	GtkWidget   *tree_view;
	GtkWidget   *palette;
	GtkToolItem *annot_text_item;

	EvJob       *job;
	guint        selection_changed_id;
};

static void ev_sidebar_annotations_page_iface_init (EvSidebarPageInterface *iface);
static void ev_sidebar_annotations_load            (EvSidebarAnnotations   *sidebar_annots);

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED (EvSidebarAnnotations,
                        ev_sidebar_annotations,
                        GTK_TYPE_BOX,
                        0,
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
					       ev_sidebar_annotations_page_iface_init))

#define EV_SIDEBAR_ANNOTATIONS_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_ANNOTATIONS, EvSidebarAnnotationsPrivate))

static void
ev_sidebar_annotations_dispose (GObject *object)
{
	EvSidebarAnnotations *sidebar_annots = EV_SIDEBAR_ANNOTATIONS (object);
	EvSidebarAnnotationsPrivate *priv = sidebar_annots->priv;

	if (priv->document) {
		g_object_unref (priv->document);
		priv->document = NULL;
	}

	G_OBJECT_CLASS (ev_sidebar_annotations_parent_class)->dispose (object);
}

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
	GtkWidget         *label;

	swindow = gtk_scrolled_window_new (NULL, NULL);
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

	label = gtk_label_new (_("List"));
	gtk_notebook_append_page (GTK_NOTEBOOK (ev_annots->priv->notebook),
				  swindow, label);
	gtk_widget_show (label);

	gtk_widget_show (swindow);
}

static void
ev_sidebar_annotations_text_annot_button_toggled (GtkToggleToolButton  *toolbutton,
						  EvSidebarAnnotations *sidebar_annots)
{
	EvAnnotationType annot_type;

	if (!gtk_toggle_tool_button_get_active (toolbutton)) {
		g_signal_emit (sidebar_annots, signals[ANNOT_ADD_CANCELLED], 0, NULL);
		return;
	}

	if (GTK_TOOL_ITEM (toolbutton) == sidebar_annots->priv->annot_text_item)
		annot_type = EV_ANNOTATION_TYPE_TEXT;
	else
		annot_type = EV_ANNOTATION_TYPE_UNKNOWN;

	g_signal_emit (sidebar_annots, signals[BEGIN_ANNOT_ADD], 0, annot_type);
}

static void
ev_sidebar_annotations_add_annots_palette (EvSidebarAnnotations *ev_annots)
{
	GtkWidget   *swindow;
	GtkWidget   *group;
	GtkToolItem *item;
	GtkWidget   *label;

	swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);

	ev_annots->priv->palette = gtk_tool_palette_new ();
	group = gtk_tool_item_group_new (_("Annotations"));
	gtk_container_add (GTK_CONTAINER (ev_annots->priv->palette), group);

	/* FIXME: use a better icon than EDIT */
	item = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), GTK_STOCK_EDIT);
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Text"));
	gtk_widget_set_tooltip_text (GTK_WIDGET (item), _("Add text annotation"));
	ev_annots->priv->annot_text_item = item;
	g_signal_connect (item, "toggled",
			  G_CALLBACK (ev_sidebar_annotations_text_annot_button_toggled),
			  ev_annots);
	gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (group), item, -1);
	gtk_widget_show (GTK_WIDGET (item));

	gtk_container_add (GTK_CONTAINER (swindow), ev_annots->priv->palette);
	gtk_widget_show (ev_annots->priv->palette);

	label = gtk_label_new (_("Add"));
	gtk_notebook_append_page (GTK_NOTEBOOK (ev_annots->priv->notebook),
				  swindow, label);
	gtk_widget_show (label);

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
	ev_sidebar_annotations_add_annots_palette (ev_annots);
        gtk_box_pack_start (GTK_BOX (ev_annots), ev_annots->priv->notebook, TRUE, TRUE, 0);
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
	g_object_class->dispose = ev_sidebar_annotations_dispose;

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
	signals[BEGIN_ANNOT_ADD] =
		g_signal_new ("begin-annot-add",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarAnnotationsClass, begin_annot_add),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1,
			      EV_TYPE_ANNOTATION_TYPE);
	signals[ANNOT_ADD_CANCELLED] =
		g_signal_new ("annot-add-cancelled",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarAnnotationsClass, annot_add_cancelled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0,
			      G_TYPE_NONE);
}

GtkWidget *
ev_sidebar_annotations_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_ANNOTATIONS,
                                         "orientation", GTK_ORIENTATION_VERTICAL,
                                         NULL));
}

void
ev_sidebar_annotations_annot_added (EvSidebarAnnotations *sidebar_annots,
				    EvAnnotation         *annot)
{
	GtkToggleToolButton *toolbutton;

	if (EV_IS_ANNOTATION_TEXT (annot)) {
		toolbutton = GTK_TOGGLE_TOOL_BUTTON (sidebar_annots->priv->annot_text_item);
		g_signal_handlers_block_by_func (toolbutton,
						 ev_sidebar_annotations_text_annot_button_toggled,
						 sidebar_annots);
		gtk_toggle_tool_button_set_active (toolbutton, FALSE);
		g_signal_handlers_unblock_by_func (toolbutton,
						   ev_sidebar_annotations_text_annot_button_toggled,
						   sidebar_annots);
	}

	ev_sidebar_annotations_load (sidebar_annots);
}

void
ev_sidebar_annotations_annot_removed (EvSidebarAnnotations *sidebar_annots)
{
	ev_sidebar_annotations_load (sidebar_annots);
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
			const gchar  *label;
			const gchar  *modified;
			gchar        *markup;
			GtkTreeIter   child_iter;
			GdkPixbuf    *pixbuf = NULL;

			annot = ((EvMapping *)(ll->data))->data;
			if (!EV_IS_ANNOTATION_MARKUP (annot))
				continue;

			label = ev_annotation_markup_get_label (EV_ANNOTATION_MARKUP (annot));
			modified = ev_annotation_get_modified (annot);
			if (modified) {
				markup = g_strdup_printf ("<span weight=\"bold\">%s</span>\n%s",
							  label, modified);
			} else {
				markup = g_strdup_printf ("<span weight=\"bold\">%s</span>", label);
			}

			if (EV_IS_ANNOTATION_TEXT (annot)) {
				if (!text_icon) {
					/* FIXME: use a better icon than EDIT */
					text_icon = gtk_widget_render_icon_pixbuf (priv->tree_view,
                                                                                   GTK_STOCK_EDIT,
                                                                                   GTK_ICON_SIZE_BUTTON);
				}
				pixbuf = text_icon;
			} else if (EV_IS_ANNOTATION_ATTACHMENT (annot)) {
				if (!attachment_icon) {
					attachment_icon = gtk_widget_render_icon_pixbuf (priv->tree_view,
                                                                                         EV_STOCK_ATTACHMENT,
                                                                                         GTK_ICON_SIZE_BUTTON);
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
ev_sidebar_annotations_load (EvSidebarAnnotations *sidebar_annots)
{
	EvSidebarAnnotationsPrivate *priv = sidebar_annots->priv;

	if (priv->job) {
		g_signal_handlers_disconnect_by_func (priv->job,
						      job_finished_callback,
						      sidebar_annots);
		g_object_unref (priv->job);
	}

	priv->job = ev_job_annots_new (priv->document);
	g_signal_connect (priv->job, "finished",
			  G_CALLBACK (job_finished_callback),
			  sidebar_annots);
	/* The priority doesn't matter for this job */
	ev_job_scheduler_push_job (priv->job, EV_JOB_PRIORITY_NONE);
}

static void
ev_sidebar_annotations_document_changed_cb (EvDocumentModel      *model,
					    GParamSpec           *pspec,
					    EvSidebarAnnotations *sidebar_annots)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvSidebarAnnotationsPrivate *priv = sidebar_annots->priv;
	gboolean show_tabs;

	if (!EV_IS_DOCUMENT_ANNOTATIONS (document))
		return;

	if (priv->document)
		g_object_unref (priv->document);
	priv->document = g_object_ref (document);

	show_tabs = ev_document_annotations_can_add_annotation (EV_DOCUMENT_ANNOTATIONS (document));
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), show_tabs);

	ev_sidebar_annotations_load (sidebar_annots);
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
