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

#include "config.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "ev-properties.h"
#include "ev-properties-view.h"


struct _EvPropertiesView {
	GtkBox base_instance;

	GtkWidget *grid;
	GtkWidget *labels[N_PROPERTIES];
	gchar     *uri;
	guint64    file_size;
};

struct _EvPropertiesViewClass {
	GtkBoxClass base_class;
};

G_DEFINE_TYPE (EvPropertiesView, ev_properties_view, GTK_TYPE_BOX)

static void
ev_properties_view_dispose (GObject *object)
{
	EvPropertiesView *properties = EV_PROPERTIES_VIEW (object);

	g_clear_pointer (&properties->uri, g_free);

	G_OBJECT_CLASS (ev_properties_view_parent_class)->dispose (object);
}

static void
ev_properties_view_class_init (EvPropertiesViewClass *properties_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (properties_class);

	g_object_class->dispose = ev_properties_view_dispose;
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

static gchar *
cleanup_text (const char *str)
{
	char *valid;
	GString *gstr;
	gboolean prev_isspace = TRUE;

	g_assert_nonnull (str);
	valid = make_valid_utf8 (str);
	gstr = g_string_new (NULL);

	for (str = valid; *str != '\0'; str = g_utf8_next_char (str)) {
		gunichar c = g_utf8_get_char (str);

		if (g_unichar_isspace (c)) {
			/* replace a run of any whitespace characters with a
			 * space single character */
			if (!prev_isspace)
				g_string_append_c (gstr, ' ');
			prev_isspace = TRUE;
		} else {
			g_string_append_unichar (gstr, c);
			prev_isspace = FALSE;
		}
	}

	g_free (valid);
	return g_string_free (gstr, FALSE);
}

static void
set_property (EvPropertiesView *properties,
	      GtkGrid          *grid,
	      Property          property,
	      const gchar      *text,
	      gint             *row)
{
	GtkWidget *property_label = NULL;
	GtkWidget *value_label = NULL;
	gchar     *markup;
	gchar     *valid_text;

	if (!properties->labels[property]) {
		property_label = gtk_label_new (NULL);
		g_object_set (G_OBJECT (property_label), "xalign", 0.0, NULL);
		markup = g_strdup_printf ("<b>%s</b>", _(properties_info[property].label));
		gtk_label_set_markup (GTK_LABEL (property_label), markup);
		g_free (markup);

		gtk_grid_attach (grid, property_label, 0, *row, 1, 1);
	}

	if (!properties->labels[property]) {
		value_label = gtk_label_new (NULL);

		g_object_set (G_OBJECT (value_label),
			      "xalign", 0.0,
			      "width_chars", 25,
			      "selectable", TRUE,
			      "ellipsize", PANGO_ELLIPSIZE_END,
			      "hexpand", TRUE,
			      "max-width-chars", 100,
			      "wrap-mode", PANGO_WRAP_WORD_CHAR,
			      "wrap", TRUE,
			      "lines", 5,
			      NULL);
	} else {
		value_label = properties->labels[property];
	}

	if (text == NULL || text[0] == '\000') {
		/* translators: This is used when a document property does
		   not have a value.  Examples:
		   Author: None
		   Keywords: None
		*/
		markup = g_markup_printf_escaped ("<i>%s</i>", _("None"));
		gtk_label_set_markup (GTK_LABEL (value_label), markup);
		g_free (markup);
	} else {
		valid_text = cleanup_text (text);
		gtk_label_set_text (GTK_LABEL (value_label), valid_text);
		g_free (valid_text);
	}

	if (!properties->labels[property]) {
		gtk_grid_attach (grid, value_label, 1, *row, 1, 1);
		properties->labels[property] = value_label;
	}

	if (property_label && value_label) {
		gtk_accessible_update_relation (GTK_ACCESSIBLE (value_label),
				GTK_ACCESSIBLE_RELATION_LABELLED_BY, property_label,
				NULL, -1);
	}

	*row += 1;
}

void
ev_properties_view_set_info (EvPropertiesView *properties, const EvDocumentInfo *info)
{
	GtkWidget *grid;
	gchar     *text;
	gint       row = 0;
        GDateTime *datetime;

	grid = properties->grid;

	if (info->fields_mask & EV_DOCUMENT_INFO_TITLE) {
		set_property (properties, GTK_GRID (grid), TITLE_PROPERTY, info->title, &row);
	}
	set_property (properties, GTK_GRID (grid), URI_PROPERTY, properties->uri, &row);
	if (info->fields_mask & EV_DOCUMENT_INFO_SUBJECT) {
		set_property (properties, GTK_GRID (grid), SUBJECT_PROPERTY, info->subject, &row);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_AUTHOR) {
		set_property (properties, GTK_GRID (grid), AUTHOR_PROPERTY, info->author, &row);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_KEYWORDS) {
		set_property (properties, GTK_GRID (grid), KEYWORDS_PROPERTY, info->keywords, &row);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_PRODUCER) {
		set_property (properties, GTK_GRID (grid), PRODUCER_PROPERTY, info->producer, &row);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_CREATOR) {
		set_property (properties, GTK_GRID (grid), CREATOR_PROPERTY, info->creator, &row);
	}

        datetime = ev_document_info_get_created_datetime (info);
        if (datetime != NULL) {
                text = ev_document_misc_format_datetime (datetime);
                set_property (properties, GTK_GRID (grid), CREATION_DATE_PROPERTY, text, &row);
                g_free (text);
        } else {
                set_property (properties, GTK_GRID (grid), CREATION_DATE_PROPERTY, NULL, &row);
        }
        datetime = ev_document_info_get_modified_datetime (info);
        if (datetime != NULL) {
                text = ev_document_misc_format_datetime (datetime);
                set_property (properties, GTK_GRID (grid), MOD_DATE_PROPERTY, text, &row);
                g_free (text);
        } else {
                set_property (properties, GTK_GRID (grid), MOD_DATE_PROPERTY, NULL, &row);
        }
	if (info->fields_mask & EV_DOCUMENT_INFO_FORMAT) {
		set_property (properties, GTK_GRID (grid), FORMAT_PROPERTY, info->format, &row);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_N_PAGES) {
		text = g_strdup_printf ("%d", info->n_pages);
		set_property (properties, GTK_GRID (grid), N_PAGES_PROPERTY, text, &row);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_LINEARIZED) {
		set_property (properties, GTK_GRID (grid), LINEARIZED_PROPERTY, info->linearized, &row);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_SECURITY) {
		set_property (properties, GTK_GRID (grid), SECURITY_PROPERTY, info->security, &row);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_PAPER_SIZE) {
		text = ev_regular_paper_size (info);
		set_property (properties, GTK_GRID (grid), PAPER_SIZE_PROPERTY, text, &row);
		g_free (text);
	}
	if (info->fields_mask & EV_DOCUMENT_INFO_CONTAINS_JS) {
		if (info->contains_js == EV_DOCUMENT_CONTAINS_JS_YES) {
			text = _("Yes");
		} else if (info->contains_js == EV_DOCUMENT_CONTAINS_JS_NO) {
			text = _("No");
		} else {
			text = _("Unknown");
		}
		set_property (properties, GTK_GRID (grid), CONTAINS_JS_PROPERTY, text, &row);
	}
	if (properties->file_size) {
		text = g_format_size (properties->file_size);
		set_property (properties, GTK_GRID (grid), FILE_SIZE_PROPERTY, text, &row);
		g_free (text);
	}
}

static void
ev_properties_view_init (EvPropertiesView *properties)
{
	GtkWidget *widget = GTK_WIDGET (properties);

	gtk_widget_set_margin_bottom (widget, 12);
	gtk_widget_set_margin_top (widget, 12);
	gtk_widget_set_margin_start (widget, 12);
	gtk_widget_set_margin_end (widget, 12);

	properties->grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (properties->grid), 12);
	gtk_grid_set_row_spacing (GTK_GRID (properties->grid), 6);
	gtk_box_prepend (GTK_BOX (properties), properties->grid);
}

void
ev_properties_view_register_type (GTypeModule *module)
{
	ev_properties_view_get_type ();
}

GtkWidget *
ev_properties_view_new (EvDocument *document)
{
	EvPropertiesView *properties;

	properties = g_object_new (EV_TYPE_PROPERTIES,
				   "orientation", GTK_ORIENTATION_VERTICAL,
				   NULL);
	properties->uri = g_uri_unescape_string (ev_document_get_uri (document), NULL);
	properties->file_size = ev_document_get_size (document);

	return GTK_WIDGET (properties);
}
