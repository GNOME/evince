/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-document-fonts.h"
#include "ev-job-scheduler.h"
#include "ev-jobs.h"
#include "ev-properties-fonts.h"

struct _EvPropertiesFonts {
	GtkBox base_instance;

	GtkTreeView *fonts_treeview;
	GtkWidget *fonts_progress_label;
	GtkWidget *fonts_summary;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	EvJob     *fonts_job;

	EvDocument *document;
};

struct _EvPropertiesFontsClass {
	GtkBoxClass base_class;
};

static void
job_fonts_finished_cb (EvJob *job, EvPropertiesFonts *properties);

G_DEFINE_TYPE (EvPropertiesFonts, ev_properties_fonts, GTK_TYPE_BOX)

static void
ev_properties_fonts_dispose (GObject *object)
{
	EvPropertiesFonts *properties = EV_PROPERTIES_FONTS (object);

	if (properties->fonts_job) {
		g_signal_handlers_disconnect_by_func (properties->fonts_job,
						      job_fonts_finished_cb,
						      properties);
		ev_job_cancel (properties->fonts_job);

		g_clear_object (&properties->fonts_job);
	}

	G_OBJECT_CLASS (ev_properties_fonts_parent_class)->dispose (object);
}

static void
ev_properties_fonts_class_init (EvPropertiesFontsClass *properties_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (properties_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (properties_class);

	g_object_class->dispose = ev_properties_fonts_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
				"/org/gnome/evince/ui/properties-fonts.ui");
	gtk_widget_class_bind_template_child (widget_class, EvPropertiesFonts, column);
	gtk_widget_class_bind_template_child (widget_class, EvPropertiesFonts, renderer);
	gtk_widget_class_bind_template_child (widget_class,
					      EvPropertiesFonts, fonts_treeview);
	gtk_widget_class_bind_template_child (widget_class,
					      EvPropertiesFonts, fonts_progress_label);
	gtk_widget_class_bind_template_child (widget_class,
					      EvPropertiesFonts, fonts_summary);
}

static void
font_cell_data_func (GtkTreeViewColumn *col, GtkCellRenderer *renderer,
		     GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	char *name;
	char *details;
	char *markup;

	gtk_tree_model_get (model, iter,
			    EV_DOCUMENT_FONTS_COLUMN_NAME, &name,
			    EV_DOCUMENT_FONTS_COLUMN_DETAILS, &details,
			    -1);

	if (details) {
		markup = g_strdup_printf ("<b><big>Font: %s</big></b>\n<small>%s</small>",
					  name, details);
	} else {
		markup = g_strdup_printf ("<b><big>%s</big></b>", name);
	}

	g_object_set (renderer, "markup", markup, NULL, NULL);

	g_free (markup);
	g_free (details);
	g_free (name);
}

static void
ev_properties_fonts_init (EvPropertiesFonts *properties)
{
	GtkWidget *widget = GTK_WIDGET (properties);

	gtk_widget_init_template (widget);
	gtk_tree_view_column_set_cell_data_func (properties->column,
						 properties->renderer,
						 font_cell_data_func,
						 NULL, NULL);
}

static void
update_progress_label (GtkWidget *label, double progress)
{
	if (progress > 0) {
		char *progress_text;
		progress_text = g_strdup_printf (_("Gathering font information… %3d%%"),
						 (int) (progress * 100));
		gtk_label_set_text (GTK_LABEL (label), progress_text);
		g_free (progress_text);
		gtk_widget_show (label);
	} else {
		gtk_widget_hide (label);
	}
}

static void
job_fonts_finished_cb (EvJob *job, EvPropertiesFonts *properties)
{
	EvDocumentFonts *document_fonts = EV_DOCUMENT_FONTS (properties->document);
	const gchar     *font_summary;

	g_signal_handlers_disconnect_by_func (job, job_fonts_finished_cb, properties);
	g_clear_object (&properties->fonts_job);

	font_summary = ev_document_fonts_get_fonts_summary (document_fonts);
	if (font_summary) {
		gtk_label_set_text (GTK_LABEL (properties->fonts_summary),
				    font_summary);
		/* show the label only when fonts are scanned, so the label
		 * does not take space while it is loading */
		gtk_widget_show (properties->fonts_summary);
	}
}

static void
job_fonts_updated_cb (EvJobFonts *job, gdouble progress, EvPropertiesFonts *properties)
{
	GtkTreeModel *model;
	EvDocumentFonts *document_fonts = EV_DOCUMENT_FONTS (properties->document);

	update_progress_label (properties->fonts_progress_label, progress);

	model = gtk_tree_view_get_model (properties->fonts_treeview);
	/* Document lock is already held by the jop */
	ev_document_fonts_fill_model (document_fonts, model);
}

void
ev_properties_fonts_set_document (EvPropertiesFonts *properties,
				  EvDocument        *document)
{
	GtkTreeView *tree_view = properties->fonts_treeview;
	GtkListStore *list_store;

	properties->document = document;

	list_store = gtk_list_store_new (EV_DOCUMENT_FONTS_COLUMN_NUM_COLUMNS,
					 G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	properties->fonts_job = ev_job_fonts_new (properties->document);
	g_signal_connect (properties->fonts_job, "updated",
			  G_CALLBACK (job_fonts_updated_cb),
			  properties);
	g_signal_connect (properties->fonts_job, "finished",
			  G_CALLBACK (job_fonts_finished_cb),
			  properties);
	ev_job_scheduler_push_job (properties->fonts_job, EV_JOB_PRIORITY_NONE);
}

GtkWidget *
ev_properties_fonts_new (void)
{
	GtkWidget *retval;

	retval = GTK_WIDGET (g_object_new (EV_TYPE_PROPERTIES_FONTS, NULL));

	gtk_widget_show_all (retval);

	return retval;
}
