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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ev-properties-fonts.h"
#include "ev-document-fonts.h"
#include "ev-jobs.h"
#include "ev-job-queue.h"

#include <glib/gi18n.h>
#include <gtk/gtktreeview.h>
#include <glade/glade.h>

struct _EvPropertiesFonts {
	GtkVBox base_instance;

	GladeXML *xml;

	GtkWidget *fonts_treeview;
	GtkWidget *fonts_progress_label;

	EvDocument *document;
};

struct _EvPropertiesFontsClass {
	GtkVBoxClass base_class;
};

G_DEFINE_TYPE (EvPropertiesFonts, ev_properties_fonts, GTK_TYPE_VBOX)

static void
ev_properties_fonts_dispose (GObject *object)
{
	EvPropertiesFonts *properties = EV_PROPERTIES_FONTS (object);

	if (properties->xml) {
		g_object_unref (properties->xml);
		properties->xml = NULL;
	}
}

static void
ev_properties_fonts_class_init (EvPropertiesFontsClass *properties_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (properties_class);

	g_object_class->dispose = ev_properties_fonts_dispose;
}

static void
ev_properties_fonts_init (EvPropertiesFonts *properties)
{
	GladeXML *xml;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Create a new GladeXML object from XML file glade_file */
	xml = glade_xml_new (DATADIR "/evince-properties.glade", "fonts_page_root", NULL);
	properties->xml = xml;
	g_assert (xml != NULL);

	gtk_box_pack_start (GTK_BOX (properties),
			    glade_xml_get_widget (xml, "fonts_page_root"),
			    TRUE, TRUE, 0);

	properties->fonts_treeview = glade_xml_get_widget (xml, "fonts_treeview");
	properties->fonts_progress_label = glade_xml_get_widget (xml, "font_progress_label");

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (properties->fonts_treeview), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_title (GTK_TREE_VIEW_COLUMN (column), _("Name"));
	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
					     "text", EV_DOCUMENT_FONTS_COLUMN_NAME,
					     NULL);
}

static void
update_progress_label (GtkWidget *label, double progress)
{
	if (progress > 0) {
		char *progress_text;
		progress_text = g_strdup_printf (_("Gathering font information... %3d%%"),
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
	EvDocumentFonts *document_fonts = EV_DOCUMENT_FONTS (job->document);
	double progress;

	progress = ev_document_fonts_get_progress (document_fonts);
	update_progress_label (properties->fonts_progress_label, progress);

	if (EV_JOB_FONTS (job)->scan_completed) {
		g_signal_handlers_disconnect_by_func
				(job, job_fonts_finished_cb, properties);
	} else {
		GtkTreeModel *model;
		EvJob *new_job;

		model = gtk_tree_view_get_model
				(GTK_TREE_VIEW (properties->fonts_treeview));
		ev_document_doc_mutex_lock ();
		ev_document_fonts_fill_model (document_fonts, model);
		ev_document_doc_mutex_unlock ();
		new_job = ev_job_fonts_new (job->document);
		ev_job_queue_add_job (job, EV_JOB_PRIORITY_LOW);
		g_object_unref (new_job);
	}
}

void
ev_properties_fonts_set_document (EvPropertiesFonts *properties,
				  EvDocument        *document)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (properties->fonts_treeview);
	GtkListStore *list_store;
	EvJob *job;

	properties->document = document;

	list_store = gtk_list_store_new (EV_DOCUMENT_FONTS_COLUMN_NUM_COLUMNS,
					 G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	job = ev_job_fonts_new (properties->document);
	g_signal_connect_object (job, "finished",
			         G_CALLBACK (job_fonts_finished_cb),
				 properties, 0);
	ev_job_queue_add_job (job, EV_JOB_PRIORITY_LOW);
	g_object_unref (job);
}

GtkWidget *
ev_properties_fonts_new ()
{
	EvPropertiesFonts *properties;

	properties = g_object_new (EV_TYPE_PROPERTIES_FONTS, NULL);

	return GTK_WIDGET (properties);
}
