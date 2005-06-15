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

#include "ev-properties.h"
#include "ev-document-fonts.h"
#include "ev-jobs.h"
#include "ev-job-queue.h"
#include "ev-page-cache.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <time.h>
#include <sys/time.h>

enum
{
	FONT_NAME_COL,
	NUM_COLS
};

typedef enum
{
	TITLE_PROPERTY,
	SUBJECT_PROPERTY,
	AUTHOR_PROPERTY,
	KEYWORDS_PROPERTY,
	PRODUCER_PROPERTY,
	CREATOR_PROPERTY,
	CREATION_DATE_PROPERTY,
	MOD_DATE_PROPERTY,
	N_PAGES_PROPERTY,
	LINEARIZED_PROPERTY,
	FORMAT_PROPERTY,
	SECURITY_PROPERTY
} Property;

typedef struct
{
	Property property;
	const char *label_id;
} PropertyInfo;

static const PropertyInfo properties_info[] = {
	{ TITLE_PROPERTY, "title" },
	{ SUBJECT_PROPERTY, "subject" },
	{ AUTHOR_PROPERTY, "author" },
	{ KEYWORDS_PROPERTY, "keywords" },
	{ PRODUCER_PROPERTY, "producer" },
	{ CREATOR_PROPERTY, "creator" },
	{ CREATION_DATE_PROPERTY, "created" },
	{ MOD_DATE_PROPERTY, "modified" },
	{ N_PAGES_PROPERTY, "pages" },
	{ LINEARIZED_PROPERTY, "optimized" },
	{ FORMAT_PROPERTY, "version" },
	{ SECURITY_PROPERTY, "security" }
};

struct _EvProperties {
	GObject base_instance;

	GladeXML *xml;

	GtkWidget *dialog;
	GtkWidget *fonts_treeview;
	GtkWidget *fonts_progress_label;
	GtkWidget *font_page;

	EvDocument *document;
};

struct _EvPropertiesClass {
	GObjectClass base_class;
};

G_DEFINE_TYPE (EvProperties, ev_properties, G_TYPE_OBJECT)

static void
ev_properties_dispose (GObject *object)
{
	EvProperties *properties = EV_PROPERTIES (object);

	if (properties->xml) {
		g_object_unref (properties->xml);
		properties->xml = NULL;
	}
}

static void
ev_properties_class_init (EvPropertiesClass *properties_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (properties_class);

	g_object_class->dispose = ev_properties_dispose;
}

static void
dialog_destroy_cb (GtkWidget *dialog, EvProperties *properties)
{
	g_object_unref (properties);
}

static void
ev_properties_init (EvProperties *properties)
{
	GladeXML *xml;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Create a new GladeXML object from XML file glade_file */
	xml = glade_xml_new (DATADIR "/evince-properties.glade", NULL, NULL);
	properties->xml = xml;
	g_assert (xml != NULL);

	properties->dialog = glade_xml_get_widget (xml, "properties_dialog");
	properties->fonts_treeview = glade_xml_get_widget (xml, "fonts_treeview");
	properties->fonts_progress_label = glade_xml_get_widget (xml, "font_progress_label");
	properties->font_page = glade_xml_get_widget (xml, "fonts_page");

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (properties->fonts_treeview), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_title (GTK_TREE_VIEW_COLUMN (column), _("Name"));
	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
					     "text", EV_DOCUMENT_FONTS_COLUMN_NAME,
					     NULL);

        g_signal_connect (properties->dialog, "destroy",
                          G_CALLBACK (dialog_destroy_cb), properties);
}

/* Returns a locale specific date and time representation */
static char *
ev_properties_format_date (GTime utime)
{
	time_t time = (time_t) utime;
	struct tm t;
	char s[256];
	const char *fmt_hack = "%c";
	size_t len;

	if (!localtime_r (&time, &t)) return NULL;

	len = strftime (s, sizeof (s), fmt_hack, &t);
	if (len == 0 || s[0] == '\0') return NULL;

	return g_locale_to_utf8 (s, -1, NULL, NULL, NULL);
}

static void
set_property (GladeXML *xml, Property property, const char *text)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (xml, properties_info[property].label_id);
	g_return_if_fail (GTK_IS_LABEL (widget));

	gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
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
job_fonts_finished_cb (EvJob *job, EvProperties *properties)
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

static void
setup_fonts_view (EvProperties *properties)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (properties->fonts_treeview);
	GtkListStore *list_store;
	EvJob *job;

	list_store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	job = ev_job_fonts_new (properties->document);
	g_signal_connect_object (job, "finished",
			         G_CALLBACK (job_fonts_finished_cb),
				 properties, 0);
	ev_job_queue_add_job (job, EV_JOB_PRIORITY_LOW);
	g_object_unref (job);
}

void
ev_properties_set_document (EvProperties *properties,
			    EvDocument   *document)
{
	EvPageCache *page_cache;
	GladeXML *xml = properties->xml;
	const EvDocumentInfo *info;
	char *text;

	properties->document = document;

	page_cache = ev_page_cache_get (document);
	info = ev_page_cache_get_info (page_cache);

	if (info->fields_mask & EV_DOCUMENT_INFO_TITLE) {
		set_property (xml, TITLE_PROPERTY, info->title);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_SUBJECT) {
		set_property (xml, SUBJECT_PROPERTY, info->subject);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_AUTHOR) {
		set_property (xml, AUTHOR_PROPERTY, info->author);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_KEYWORDS) {
		set_property (xml, KEYWORDS_PROPERTY, info->keywords);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_PRODUCER) {
		set_property (xml, PRODUCER_PROPERTY, info->producer);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_CREATOR) {
		set_property (xml, CREATOR_PROPERTY, info->creator);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_CREATION_DATE) {
		text = ev_properties_format_date (info->creation_date);
		set_property (xml, CREATION_DATE_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_MOD_DATE) {
		text = ev_properties_format_date (info->modified_date);
		set_property (xml, MOD_DATE_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_FORMAT) {
		char **format_str = g_strsplit (info->format, "-", 2);

		text = g_strdup_printf (_("%s"), format_str[1]);
		set_property (xml, FORMAT_PROPERTY, text);

		g_free (text);
		g_strfreev (format_str);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_N_PAGES) {
		text = g_strdup_printf (_("%d"), info->n_pages);
		set_property (xml, N_PAGES_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_LINEARIZED) {
		set_property (xml, LINEARIZED_PROPERTY, info->linearized);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_SECURITY) {
		set_property (xml, SECURITY_PROPERTY, info->security);
	}

	if (EV_IS_DOCUMENT_FONTS (document)) {
		gtk_widget_show (properties->font_page);
		setup_fonts_view (properties);
	} else {
		gtk_widget_hide (properties->font_page);
	}
}

EvProperties *
ev_properties_new ()
{
	EvProperties *properties;

	properties = g_object_new (EV_TYPE_PROPERTIES, NULL);

	return properties;
}

void
ev_properties_show (EvProperties *properties, GtkWidget *parent)
{
	gtk_window_set_transient_for (GTK_WINDOW (properties->dialog),
				      GTK_WINDOW (parent));
	g_signal_connect (properties->dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show (properties->dialog);
}
