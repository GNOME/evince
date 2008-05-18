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
#include <gtk/gtkversion.h>
#include <glade/glade.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif

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
	SECURITY_PROPERTY,
	PAPER_SIZE_PROPERTY
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
	{ SECURITY_PROPERTY, "security" },
	{ PAPER_SIZE_PROPERTY, "papersize" }
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

	G_OBJECT_CLASS (ev_properties_view_parent_class)->dispose (object);
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

#if GTK_CHECK_VERSION (2, 11, 0)
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
		exact_size = g_strdup_printf(_("%.0f x %.0f mm"),
					     info->paper_width,
					     info->paper_height);
	} else {
		exact_size = g_strdup_printf (_("%.2f x %.2f inch"),
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
#else /* ! GTK 2.11.0 */
/*
 * All values are in mm. 
 * Source: http://en.wikipedia.org/wiki/Paper_size
 */
struct regular_paper_size {
	double width;
	double height;
	double width_tolerance;
	double height_tolerance;
	const char *description;
} const regular_paper_sizes[] = {
	// ISO 216 paper sizes
	{  841.0f, 1189.0f, 3.0f, 3.0f, "A0"  },
	{  594.0f,  841.0f, 2.0f, 3.0f, "A1"  },
	{  420.0f,  594.0f, 2.0f, 2.0f, "A2"  },
	{  297.0f,  420.0f, 2.0f, 2.0f, "A3"  },
	{  210.0f,  297.0f, 2.0f, 2.0f, "A4"  },
	{  148.0f,  210.0f, 1.5f, 2.0f, "A5"  },
	{  105.0f,  148.0f, 1.5f, 1.5f, "A6"  },
	{   74.0f,  105.0f, 1.5f, 1.5f, "A7"  },
	{   52.0f,   74.0f, 1.5f, 1.5f, "A8"  },
	{   37.0f,   52.0f, 1.5f, 1.5f, "A9"  },
	{   26.0f,   37.0f, 1.5f, 1.5f, "A10" },
	{ 1000.0f, 1414.0f, 3.0f, 3.0f, "B0"  },
	{  707.0f, 1000.0f, 3.0f, 3.0f, "B1"  },
	{  500.0f,  707.0f, 2.0f, 3.0f, "B2"  },
	{  353.0f,  500.0f, 2.0f, 2.0f, "B3"  },
	{  250.0f,  353.0f, 2.0f, 2.0f, "B4"  },
	{  176.0f,  250.0f, 2.0f, 2.0f, "B5"  },
	{  125.0f,  176.0f, 1.5f, 2.0f, "B6"  },
	{   88.0f,  125.0f, 1.5f, 1.5f, "B7"  },
	{   62.0f,   88.0f, 1.5f, 1.5f, "B8"  },
	{   44.0f,   62.0f, 1.5f, 1.5f, "B9"  },
	{   31.0f,   44.0f, 1.5f, 1.5f, "B10" },
	{  917.0f, 1297.0f, 3.0f, 3.0f, "C0"  },
	{  648.0f,  917.0f, 3.0f, 3.0f, "C1"  },
	{  458.0f,  648.0f, 2.0f, 3.0f, "C2"  },
	{  324.0f,  458.0f, 2.0f, 2.0f, "C3"  },
	{  229.0f,  324.0f, 2.0f, 2.0f, "C4"  },
	{  162.0f,  229.0f, 2.0f, 2.0f, "C5"  },
	{  114.0f,  162.0f, 1.5f, 2.0f, "C6"  },
	{   81.0f,  114.0f, 1.5f, 1.5f, "C7"  },
	{   57.0f,   81.0f, 1.5f, 1.5f, "C8"  },
	{   40.0f,   57.0f, 1.5f, 1.5f, "C9"  },
	{   28.0f,   40.0f, 1.5f, 1.5f, "C10" },

	// US paper sizes
	{  279.0f,  216.0f, 3.0f, 3.0f, "Letter" },
	{  356.0f,  216.0f, 3.0f, 3.0f, "Legal"  },
	{  432.0f,  279.0f, 3.0f, 3.0f, "Ledger" }
};

typedef enum {
  EV_UNIT_INCH,
  EV_UNIT_MM
} EvUnit; 

static EvUnit
ev_get_default_user_units (void)
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
  if (imperial && imperial[0] == 2 )
    return EV_UNIT_INCH;  /* imperial */
  if (imperial && imperial[0] == 1 )
    return EV_UNIT_MM;  /* metric */
#endif

  if (strcmp (e, "default:inch")==0)
    return EV_UNIT_INCH;
  else if (strcmp (e, "default:mm"))
    g_warning ("Whoever translated default:mm did so wrongly.\n");
  return EV_UNIT_MM;
}

static char *
ev_regular_paper_size (const EvDocumentInfo *info)
{
	const struct regular_paper_size *size;   
	EvUnit unit;
	char *exact_size = NULL;
	char *str = NULL;
	int i;

	unit = ev_get_default_user_units ();	

	if (unit == EV_UNIT_INCH)
		/* Imperial measurement (inches) */
		exact_size = g_strdup_printf( _("%.2f x %.2f in"),
					      info->paper_width  / 25.4f,
					      info->paper_height / 25.4f );
	else
		/* Metric measurement (millimeters) */
		exact_size = g_strdup_printf( _("%.0f x %.0f mm"),
					      info->paper_width,
					      info->paper_height );
	
	for (i = G_N_ELEMENTS ( regular_paper_sizes ) - 1; i >= 0; i--) {
		size = &regular_paper_sizes[i];

		if ( ABS( info->paper_height - size->height ) <= size->height_tolerance &&
		     ABS( info->paper_width  - size->width  ) <= size->width_tolerance ) {
			/* Note to translators: first placeholder is the paper name (eg.
			 * A4), second placeholder is the paper size (eg. 297x210 mm) */
			str = g_strdup_printf ( _("%s, Portrait (%s)"),
						size->description,
						exact_size );
		} else if ( ABS( info->paper_width  - size->height ) <= size->height_tolerance &&
			    ABS( info->paper_height - size->width  ) <= size->width_tolerance ) {
			/* Note to translators: first placeholder is the paper name (eg.
			 * A4), second placeholder is the paper size (eg. 297x210 mm) */
			str = g_strdup_printf ( _("%s, Landscape (%s)"),
						size->description,
						exact_size );
		}
	}

	if (str != NULL) {
		g_free (exact_size);
		return str;
	} else
		return exact_size;
}
#endif /* GTK 2.11.0 */

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
	if (info->fields_mask & EV_DOCUMENT_INFO_PAPER_SIZE) {
		text = ev_regular_paper_size (info);
		set_property (xml, PAPER_SIZE_PROPERTY, text);
		g_free (text);
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
