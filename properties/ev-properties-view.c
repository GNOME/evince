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

#include "ev-properties-view.h"

typedef enum {
	TITLE_PROPERTY,
	URI_PROPERTY,
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
	SECURITY_PROPERTY,
	CONTAINS_JS_PROPERTY,
	PAPER_SIZE_PROPERTY,
	FILE_SIZE_PROPERTY,
	N_PROPERTIES,
} Property;

typedef struct {
	Property property;
	const char *label;
} PropertyInfo;

static const PropertyInfo properties_info[] = {
	{ TITLE_PROPERTY,         N_("Title:") },
	{ URI_PROPERTY,           N_("Location:") },
	{ SUBJECT_PROPERTY,       N_("Subject:") },
	{ AUTHOR_PROPERTY,        N_("Author:") },
	{ KEYWORDS_PROPERTY,      N_("Keywords:") },
	{ PRODUCER_PROPERTY,      N_("Producer:") },
	{ CREATOR_PROPERTY,       N_("Creator:") },
	{ CREATION_DATE_PROPERTY, N_("Created:") },
	{ MOD_DATE_PROPERTY,      N_("Modified:") },
	{ N_PAGES_PROPERTY,       N_("Number of Pages:") },
	{ LINEARIZED_PROPERTY,    N_("Optimized:") },
	{ FORMAT_PROPERTY,        N_("Format:") },
	{ SECURITY_PROPERTY,      N_("Security:") },
	{ CONTAINS_JS_PROPERTY,   N_("Contains Javascript:") },
	{ PAPER_SIZE_PROPERTY,    N_("Paper Size:") },
	{ FILE_SIZE_PROPERTY,     N_("Size:") }
};

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
	
	if (properties->uri) {
		g_free (properties->uri);
		properties->uri = NULL;
	}
	
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
		gtk_widget_show (property_label);
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

#if 0
	if (property_label && value_label) {
		atk_object_add_relationship (gtk_widget_get_accessible (property_label),
					     ATK_RELATION_LABEL_FOR,
					     gtk_widget_get_accessible (value_label));
		atk_object_add_relationship (gtk_widget_get_accessible (value_label),
					     ATK_RELATION_LABELLED_BY,
					     gtk_widget_get_accessible (property_label));
	}
#endif

	gtk_widget_show (value_label);

	*row += 1;
}

static GtkUnit
get_default_user_units (void)
{
	/* Translate to the default units to use for presenting
	 * lengths to the user. Translate to default:inch if you
	 * want inches, otherwise translate to default:mm.
	 * Do *not* translate it to "predefinito:mm", if it
	 * it isn't default:mm or default:inch it will not work
	 */
	gchar *e = _("default:mm");

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
	gchar *imperial = NULL;
	
	imperial = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
	if (imperial && imperial[0] == 2)
		return GTK_UNIT_INCH;  /* imperial */
	if (imperial && imperial[0] == 1)
		return GTK_UNIT_MM;  /* metric */
#endif

	if (strcmp (e, "default:mm") == 0)
		return GTK_UNIT_MM;
	if (strcmp (e, "default:inch") == 0)
		return GTK_UNIT_INCH;
	
	g_warning ("Whoever translated default:mm did so wrongly.\n");
				
	return GTK_UNIT_MM;
}

static gdouble
get_tolerance (gdouble size)
{
	if (size < 150.0f)
		return 1.5f;
	else if (size >= 150.0f && size <= 600.0f)
		return 2.0f;
	else
		return 3.0f;
}

static char *
ev_regular_paper_size (const EvDocumentInfo *info)
{
	GList *paper_sizes, *l;
	gchar *exact_size;
	gchar *str = NULL;
	GtkUnit units;

	units = get_default_user_units ();

	if (units == GTK_UNIT_MM) {
		exact_size = g_strdup_printf(_("%.0f × %.0f mm"),
					     info->paper_width,
					     info->paper_height);
	} else {
		exact_size = g_strdup_printf (_("%.2f × %.2f inch"),
					      info->paper_width  / 25.4f,
					      info->paper_height / 25.4f);
	}

	paper_sizes = gtk_paper_size_get_paper_sizes (FALSE);
	
	for (l = paper_sizes; l && l->data; l = g_list_next (l)) {
		GtkPaperSize *size = (GtkPaperSize *) l->data;
		gdouble paper_width;
		gdouble paper_height;
		gdouble width_tolerance;
		gdouble height_tolerance;

		paper_width = gtk_paper_size_get_width (size, GTK_UNIT_MM);
		paper_height = gtk_paper_size_get_height (size, GTK_UNIT_MM);

		width_tolerance = get_tolerance (paper_width);
		height_tolerance = get_tolerance (paper_height);

		if (ABS (info->paper_height - paper_height) <= height_tolerance &&
		    ABS (info->paper_width  - paper_width) <= width_tolerance) {
			/* Note to translators: first placeholder is the paper name (eg.
			 * A4), second placeholder is the paper size (eg. 297x210 mm) */
			str = g_strdup_printf (_("%s, Portrait (%s)"),
					       gtk_paper_size_get_display_name (size),
					       exact_size);
		} else if (ABS (info->paper_width  - paper_height) <= height_tolerance &&
			   ABS (info->paper_height - paper_width) <= width_tolerance) {
			/* Note to translators: first placeholder is the paper name (eg.
			 * A4), second placeholder is the paper size (eg. 297x210 mm) */
			str = g_strdup_printf ( _("%s, Landscape (%s)"),
						gtk_paper_size_get_display_name (size),
						exact_size);
		}
	}

	g_list_foreach (paper_sizes, (GFunc) gtk_paper_size_free, NULL);
	g_list_free (paper_sizes);

	if (str != NULL) {
		g_free (exact_size);
		return str;
	}
	
	return exact_size;
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
	properties->grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (properties->grid), 12);
	gtk_grid_set_row_spacing (GTK_GRID (properties->grid), 6);
	gtk_widget_set_margin_bottom (properties->grid, 12);
	gtk_widget_set_margin_top (properties->grid, 12);
	gtk_widget_set_margin_start (properties->grid, 12);
	gtk_widget_set_margin_end (properties->grid, 12);
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
