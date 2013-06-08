/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2009 Juanjo Marín <juanj.marin@juntadeandalucia.es>
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

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-properties-license.h"

struct _EvPropertiesLicense {
	GtkVBox base_instance;
};

struct _EvPropertiesLicenseClass {
	GtkVBoxClass base_class;
};

G_DEFINE_TYPE (EvPropertiesLicense, ev_properties_license, GTK_TYPE_BOX)

static void
ev_properties_license_class_init (EvPropertiesLicenseClass *properties_license_class)
{
}

static GtkWidget *
get_license_text_widget (EvDocumentLicense *license)
{
	GtkWidget *swindow, *textview;
	GtkTextBuffer *buffer;

	textview = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 8);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (textview), 8);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_set_text (buffer, ev_document_license_get_text (license), -1);

	swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (swindow), textview);
	gtk_widget_show (textview);

	return swindow;
}

static GtkWidget *
get_license_uri_widget (const gchar *uri)
{
	GtkWidget *label;
	gchar     *checked_uri;
	gchar     *markup;

	label = gtk_label_new (NULL);
	g_object_set (G_OBJECT (label),
		      "xalign", 0.0,
		      "width_chars", 25,
		      "selectable", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	checked_uri = g_uri_parse_scheme (uri);
	if (checked_uri) {
		markup = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", uri, uri);
		gtk_label_set_markup (GTK_LABEL (label), markup);
		g_free (markup);
		g_free (checked_uri);
	} else {
		gtk_label_set_text (GTK_LABEL (label), uri);
	}

	return label;
}

static void
ev_properties_license_add_section (EvPropertiesLicense *properties,
				   const gchar         *title_text,
				   GtkWidget           *contents)
{
	GtkWidget *title;
	GtkWidget *alignment;
	gchar     *markup;

	title = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (title), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (title), TRUE);
	markup = g_strdup_printf ("<b>%s</b>", title_text);
	gtk_label_set_markup (GTK_LABEL (title), markup);
	g_free (markup);
	gtk_box_pack_start (GTK_BOX (properties), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	alignment = gtk_alignment_new (0.5, 0.5, 1., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (alignment), contents);
	gtk_widget_show (contents);

	gtk_box_pack_start (GTK_BOX (properties), alignment, FALSE, TRUE, 0);
	gtk_widget_show (alignment);
}

void
ev_properties_license_set_license (EvPropertiesLicense *properties,
				   EvDocumentLicense   *license)
{
	const gchar *text = ev_document_license_get_text (license);
	const gchar *uri = ev_document_license_get_uri (license);
	const gchar *web_statement = ev_document_license_get_web_statement (license);

	if (text) {
		ev_properties_license_add_section (properties,
						   _("Usage terms"),
						   get_license_text_widget (license));
	}

	if (uri) {
		ev_properties_license_add_section (properties,
						   _("Text License"),
						   get_license_uri_widget (uri));
	}

	if (web_statement) {
		ev_properties_license_add_section (properties,
						   _("Further Information"),
						   get_license_uri_widget (web_statement));
	}
}

static void
ev_properties_license_init (EvPropertiesLicense *properties)
{
	gtk_box_set_spacing (GTK_BOX (properties), 12);
	gtk_container_set_border_width (GTK_CONTAINER (properties), 12);
}

GtkWidget *
ev_properties_license_new (void)
{
	EvPropertiesLicense *properties_license;

	properties_license = g_object_new (EV_TYPE_PROPERTIES_LICENSE,
					   "orientation", GTK_ORIENTATION_VERTICAL,
					   NULL);

	return GTK_WIDGET (properties_license);
}
