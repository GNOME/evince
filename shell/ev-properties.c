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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

GtkDialog *
ev_properties_new (EvDocument	*document, 
		   		   GtkWidget	*toplevel)
{
	const char *glade_file = DATADIR "/evince-properties.glade";
	GladeXML *xml;
	GtkWidget *dialog = NULL;
	EvDocumentInfo *info;
	GtkWidget *title, *subject, *author, *keywords, *producer, *creator;
	GtkWidget *created, *modified, *security, *version, *pages, *optimized;
	gchar *n_pages, **format_str, *pdf_version;
	gchar *creation_date, *modified_date;
	gchar *secured_document;
	
	/* Create a new GladeXML object from XML file glade_file */
	xml = glade_xml_new (glade_file, NULL, NULL);
	g_return_val_if_fail (xml != NULL, NULL);

	/* Retrieve the document structure */
	info = ev_document_get_info (document);

	/* Assign variables to labels */
	dialog = glade_xml_get_widget (xml, "properties_dialog"); 
	title = glade_xml_get_widget (xml, "title");
	subject = glade_xml_get_widget (xml, "subject");
	author = glade_xml_get_widget (xml, "author");
	keywords = glade_xml_get_widget (xml, "keywords");
	producer = glade_xml_get_widget (xml, "producer");
	creator = glade_xml_get_widget (xml, "creator");
	created = glade_xml_get_widget (xml, "created");
	modified = glade_xml_get_widget (xml, "modified");
	security = glade_xml_get_widget (xml, "security");
	version = glade_xml_get_widget (xml, "version");
	pages = glade_xml_get_widget (xml, "pages");
	optimized = glade_xml_get_widget (xml, "optimized");

	/* Number of pages */
	n_pages = g_strdup_printf (_("%d"), ev_document_get_n_pages (document));

	/* PDF version */
	format_str = g_strsplit (info->format, "-", 2);
	pdf_version = g_strdup_printf (_("%s"), format_str[1]);
	
	/* Creation and modified date */
	creation_date = ev_properties_format_date ((GTime) info->creation_date);
	modified_date = ev_properties_format_date ((GTime) info->modified_date);
	
	/* Does the document have security? */
	if (ev_document_security_has_document_security (EV_DOCUMENT_SECURITY (document))) {
		secured_document = "Yes";
	} else {
		secured_document = "No";
	}
					
	/* Shorten label values to fit window size by ellipsizing */
	gtk_label_set_ellipsize (GTK_LABEL (title), PANGO_ELLIPSIZE_END);
	gtk_label_set_ellipsize (GTK_LABEL (keywords), PANGO_ELLIPSIZE_END);
	
	/* Assign values to label fields */
	gtk_label_set_text (GTK_LABEL (title), info->title);
	gtk_label_set_text (GTK_LABEL (subject), info->subject);
	gtk_label_set_text (GTK_LABEL (author), info->author);
	gtk_label_set_text (GTK_LABEL (keywords), info->keywords);
	gtk_label_set_text (GTK_LABEL (producer), info->producer);
	gtk_label_set_text (GTK_LABEL (creator), info->creator);
	gtk_label_set_text (GTK_LABEL (created), creation_date);
	gtk_label_set_text (GTK_LABEL (modified), modified_date);
	gtk_label_set_text (GTK_LABEL (security), secured_document);
	gtk_label_set_text (GTK_LABEL (version), pdf_version);
	gtk_label_set_text (GTK_LABEL (pages), n_pages);
	gtk_label_set_text (GTK_LABEL (optimized), info->linearized);

	/* Clean up */
	g_strfreev (format_str);
	g_free (n_pages);
	g_free (pdf_version);
	g_free (creation_date);
	g_free (modified_date);	
		
	return GTK_DIALOG (dialog); 
}

/* Returns a locale specific date and time representation */
gchar *
ev_properties_format_date (GTime utime)
{
	struct tm *time;
	gchar *date_string;
	
	date_string = g_new0 (char, 101);
	
	time = localtime ((const time_t *) &utime);			
	my_strftime (date_string, 100, "%c", time);		
	
	return date_string;
}

/* Some buggy versions of gcc complain about the use of %c: 
 * warning: `%c' yields  only last 2 digits of year in some locales.
 * 
 * This is a relatively clean one is to add an intermediate
 * function thanks to the strftime(3) manpage
 */
size_t  
my_strftime (char  *s, size_t max, 
			 const char *fmt, 
			 const struct tm *tm) 
{
	return strftime (s, max, fmt, tm);
}
