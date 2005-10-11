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

#include "ev-properties-view.h"
#include "ev-document-fonts.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

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

struct _EvPropertiesView {
	GtkVBox base_instance;

	GladeXML *xml;
};

struct _EvPropertiesViewClass {
	GtkVBoxClass base_class;
};

G_DEFINE_TYPE (EvPropertiesView, ev_properties_view, GTK_TYPE_VBOX)

static void
ev_properties_view_dispose (GObject *object)
{
	EvPropertiesView *properties = EV_PROPERTIES_VIEW (object);

	if (properties->xml) {
		g_object_unref (properties->xml);
		properties->xml = NULL;
	}
}

static void
ev_properties_view_class_init (EvPropertiesViewClass *properties_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (properties_class);

	g_object_class->dispose = ev_properties_view_dispose;
}

/* Returns a locale specific date and time representation */
static char *
ev_properties_view_format_date (GTime utime)
{
	time_t time = (time_t) utime;
	struct tm t;
	char s[256];
	const char *fmt_hack = "%c";
	size_t len;

	if (time == 0 || !localtime_r (&time, &t)) return NULL;

	len = strftime (s, sizeof (s), fmt_hack, &t);
	if (len == 0 || s[0] == '\0') return NULL;

	return g_locale_to_utf8 (s, -1, NULL, NULL, NULL);
}

/* This is cut out of gconvert.c from glib (and mildly modified).  Not all
   backends give valid UTF-8 for properties, so we make sure that is.
 */
static gchar *
make_valid_utf8 (const gchar *name)
{
  GString *string;
  const gchar *remainder, *invalid;
  gint remaining_bytes, valid_bytes;
  
  string = NULL;
  remainder = name;
  remaining_bytes = strlen (name);
  
  while (remaining_bytes != 0) 
    {
      if (g_utf8_validate (remainder, remaining_bytes, &invalid)) 
	break;
      valid_bytes = invalid - remainder;
    
      if (string == NULL) 
	string = g_string_sized_new (remaining_bytes);

      g_string_append_len (string, remainder, valid_bytes);
      g_string_append_c (string, '?');
      
      remaining_bytes -= valid_bytes + 1;
      remainder = invalid + 1;
    }
  
  if (string == NULL)
    return g_strdup (name);
  
  g_string_append (string, remainder);

  g_assert (g_utf8_validate (string->str, -1, NULL));
  
  return g_string_free (string, FALSE);
}

static void
set_property (GladeXML *xml, Property property, const char *text)
{
	GtkWidget *widget;
	char *valid_text;

	widget = glade_xml_get_widget (xml, properties_info[property].label_id);
	g_return_if_fail (GTK_IS_LABEL (widget));

	if (text == NULL || text[0] == '\000') {
		gchar *markup;

		markup = g_markup_printf_escaped ("<i>%s</i>", _("None"));
		gtk_label_set_markup (GTK_LABEL (widget), markup);
		g_free (markup);

		return;
	}
	text = text ? text : "";

	valid_text = make_valid_utf8 (text);

	gtk_label_set_text (GTK_LABEL (widget), valid_text);

	g_free (valid_text);
}

void
ev_properties_view_set_info (EvPropertiesView *properties, const EvDocumentInfo *info)
{
	GladeXML *xml = properties->xml;
	char *text;

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
		text = ev_properties_view_format_date (info->creation_date);
		set_property (xml, CREATION_DATE_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_MOD_DATE) {
		text = ev_properties_view_format_date (info->modified_date);
		set_property (xml, MOD_DATE_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_FORMAT) {
		text = g_strdup_printf ("%s", info->format);
		set_property (xml, FORMAT_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_N_PAGES) {
		text = g_strdup_printf ("%d", info->n_pages);
		set_property (xml, N_PAGES_PROPERTY, text);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_LINEARIZED) {
		set_property (xml, LINEARIZED_PROPERTY, info->linearized);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_SECURITY) {
		set_property (xml, SECURITY_PROPERTY, info->security);
	}
}

static void
ev_properties_view_init (EvPropertiesView *properties)
{
	GladeXML *xml;

	/* Create a new GladeXML object from XML file glade_file */
	xml = glade_xml_new (DATADIR "/evince-properties.glade", "general_page_root", GETTEXT_PACKAGE);
	properties->xml = xml;
	g_assert (xml != NULL);

	gtk_box_pack_start (GTK_BOX (properties),
			    glade_xml_get_widget (xml, "general_page_root"),
			    TRUE, TRUE, 0);
}

void
ev_properties_view_register_type (GTypeModule *module)
{
	ev_properties_view_get_type ();
}

GtkWidget *
ev_properties_view_new (void)
{
	EvPropertiesView *properties;

	properties = g_object_new (EV_TYPE_PROPERTIES, NULL);

	return GTK_WIDGET (properties);
}
