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
	GtkBox base_instance;

	GtkWidget *license;
	GtkWidget *uri;
	GtkWidget *web_statement;
};

struct _EvPropertiesLicenseClass {
	GtkBoxClass base_class;
};

G_DEFINE_TYPE (EvPropertiesLicense, ev_properties_license, GTK_TYPE_BOX)

static void
ev_properties_license_class_init (EvPropertiesLicenseClass *properties_license_class)
{
}

static GtkWidget *
get_license_text_widget (EvDocumentLicense *license)
{
	GtkWidget     *textview;
	GtkTextBuffer *buffer;

	textview = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 8);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (textview), 8);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_set_text (buffer, ev_document_license_get_text (license), -1);
	gtk_widget_set_visible (textview, TRUE);

	return textview;
}

static void
set_uri_to_label (GtkLabel    *label,
		  const gchar *uri)
{
	gchar     *checked_uri;
	gchar     *markup;

	checked_uri = g_uri_parse_scheme (uri);
	if (checked_uri) {
		markup = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", uri, uri);
		gtk_label_set_markup (label, markup);
		g_free (markup);
		g_free (checked_uri);
	} else {
		gtk_label_set_text (label, uri);
	}
}

static GtkWidget *
get_license_uri_widget (const gchar *uri)
{
	GtkWidget *label;

	label = gtk_label_new (NULL);
	g_object_set (G_OBJECT (label),
		      "xalign", 0.0,
		      "width_chars", 25,
		      "selectable", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	set_uri_to_label (GTK_LABEL (label), uri);

	return label;
}

static void
ev_properties_license_add_section (EvPropertiesLicense *properties,
				   const gchar         *title_text,
				   GtkWidget           *contents)
{
	GtkWidget *title;
	gchar     *markup;


	title = gtk_label_new (NULL);
	g_object_set (G_OBJECT (title), "xalign", 0., "yalign", 0.5, NULL);
	gtk_label_set_use_markup (GTK_LABEL (title), TRUE);
	markup = g_strdup_printf ("<b>%s</b>", title_text);
	gtk_label_set_markup (GTK_LABEL (title), markup);
	g_free (markup);
	gtk_box_prepend (GTK_BOX (properties), title);

	g_object_set (G_OBJECT (contents), "margin-left", 12, NULL);

	gtk_box_prepend (GTK_BOX (properties), contents);
}

void
ev_properties_license_set_license (EvPropertiesLicense *properties,
				   EvDocumentLicense   *license)
{
	const gchar *text = ev_document_license_get_text (license);
	const gchar *uri = ev_document_license_get_uri (license);
	const gchar *web_statement = ev_document_license_get_web_statement (license);
	GtkTextBuffer *buffer;
	GtkWidget     *swindow;

	if (text) {
		if (!properties->license) {
			properties->license = get_license_text_widget (license);
			swindow = gtk_scrolled_window_new ();
			gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), properties->license);
			ev_properties_license_add_section (properties,
							   _("Usage terms"),
							   swindow);
		} else {
			buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (properties->license));
			gtk_text_buffer_set_text (buffer, text, -1);
		}
	}

	if (uri) {
		if (!properties->uri) {
			properties->uri = get_license_uri_widget (uri);
			ev_properties_license_add_section (properties,
							   _("Text License"),
							   properties->uri);
		} else {
			set_uri_to_label (GTK_LABEL (properties->uri), uri);
		}
	}

	if (web_statement) {
		if (!properties->web_statement) {
			properties->web_statement = get_license_uri_widget (web_statement);
			ev_properties_license_add_section (properties,
							   _("Further Information"),
							   properties->web_statement);
		} else {
			set_uri_to_label (GTK_LABEL (properties->web_statement), web_statement);
		}
	}
}

static void
ev_properties_license_init (EvPropertiesLicense *properties)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (properties),
			GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (properties), 12);

	gtk_widget_set_margin_top (GTK_WIDGET (properties), 12);
	gtk_widget_set_margin_bottom (GTK_WIDGET (properties), 12);
	gtk_widget_set_margin_start (GTK_WIDGET (properties), 12);
	gtk_widget_set_margin_end (GTK_WIDGET (properties), 12);
}

GtkWidget *
ev_properties_license_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_PROPERTIES_LICENSE, NULL));
}
