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
#include <time.h>
#include <sys/time.h>

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

GtkDialog *
ev_properties_new (EvDocumentInfo *info)
{
	GladeXML *xml;
	GtkWidget *dialog;
	char *text;
	
	/* Create a new GladeXML object from XML file glade_file */
	xml = glade_xml_new (DATADIR "/evince-properties.glade", NULL, NULL);
	g_return_val_if_fail (xml != NULL, NULL);

	dialog = glade_xml_get_widget (xml, "properties_dialog");
	g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);
					
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
		text = ev_properties_format_date ((GTime) info->creation_date);
		set_property (xml, CREATION_DATE_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_MOD_DATE) {
		text = ev_properties_format_date ((GTime) info->modified_date);
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

	return GTK_DIALOG (dialog); 
}
