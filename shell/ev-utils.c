/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Anders Carlsson <andersca@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <config.h>

#include "ev-utils.h"
#include "ev-file-helpers.h"

#include <string.h>
#include <math.h>
#include <glib/gi18n.h>

/* Simple function to output the contents of a region.  Used solely for testing
 * the region code.
 */
void
ev_print_region_contents (cairo_region_t *region)
{
	gint n_rectangles, i;

	if (region == NULL) {
		g_print ("<empty region>\n");
		return;
	}

	g_print ("<region %p>\n", region);
	n_rectangles = cairo_region_num_rectangles (region);
	for (i = 0; i < n_rectangles; i++) {
		GdkRectangle rect;

		cairo_region_get_rectangle (region, i, &rect);
		g_print ("\t(%d %d, %d %d) [%dx%d]\n",
			 rect.x,
			 rect.y,
			 rect.x + rect.width,
			 rect.y + rect.height,
			 rect.width,
			 rect.height);
	}
}

static void
file_filter_add_mime_types (GdkPixbufFormat *format, GtkFileFilter *filter,
			    GtkFileFilter   *supported_filter)
{
	gchar **mime_types;
	gint i;

	mime_types = gdk_pixbuf_format_get_mime_types (format);
	for (i = 0; mime_types[i] != 0; i++) {
		gtk_file_filter_add_mime_type (filter, mime_types[i]);
		gtk_file_filter_add_mime_type (supported_filter, mime_types[i]);
	}
	g_strfreev (mime_types);
}

void
file_chooser_dialog_add_writable_pixbuf_formats (GtkFileChooser *chooser)
{
	GSList *pixbuf_formats = NULL;
	GSList *iter;
	GtkFileFilter *filter, *supported_filter;

	supported_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (supported_filter, _("Supported Image Files"));
	gtk_file_chooser_add_filter (chooser, supported_filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (chooser, filter);

	pixbuf_formats = gdk_pixbuf_get_formats ();

	for (iter = pixbuf_formats; iter; iter = iter->next) {
		GdkPixbufFormat *format = iter->data;
		gchar *name;

		if (gdk_pixbuf_format_is_disabled (format) ||
		    !gdk_pixbuf_format_is_writable (format))
			continue;

		filter = gtk_file_filter_new ();
		name = gdk_pixbuf_format_get_description (format);
		gtk_file_filter_set_name (filter, name);

		file_filter_add_mime_types (format, filter, supported_filter);
		g_object_set_data (G_OBJECT(filter), "pixbuf-format", format);
		gtk_file_chooser_add_filter (chooser, filter);
	}

	g_slist_free (pixbuf_formats);
}

GdkPixbufFormat*
get_gdk_pixbuf_format_by_extension (const gchar *uri)
{
	GSList *pixbuf_formats = NULL;
	GSList *iter;
	int i;

	pixbuf_formats = gdk_pixbuf_get_formats ();

	for (iter = pixbuf_formats; iter; iter = iter->next) {
		gchar **extension_list;
		GdkPixbufFormat *format = iter->data;

		if (gdk_pixbuf_format_is_disabled (format) ||
	    	    !gdk_pixbuf_format_is_writable (format))
		            continue;

	        extension_list = gdk_pixbuf_format_get_extensions (format);

		for (i = 0; extension_list[i] != 0; i++) {
			if (g_str_has_suffix (uri, extension_list[i])) {
			    	g_slist_free (pixbuf_formats);
				g_strfreev (extension_list);
				return format;
			}
		}
		g_strfreev (extension_list);
	}

	g_slist_free (pixbuf_formats);
	return NULL;
}

/*
 * Replace all occurrences of substr in str with repl
 *
 * @param str a string
 * @param substr some string to replace
 * @param repl a replacement string
 *
 * @return a newly allocated string with the substr replaced by repl; free with g_free
 */
gchar*
ev_str_replace (const char *str, const char *substr, const char *repl)
{
	GString		*gstr;
	const char	*cur;

	if (str == NULL || substr == NULL || repl == NULL)
		return NULL;

	gstr = g_string_sized_new (2 * strlen (str));

	for (cur = str; *cur; ++cur) {
		if (g_str_has_prefix (cur, substr)) {
			g_string_append (gstr, repl);
			cur += strlen (substr) - 1;
		} else
			g_string_append_c (gstr, *cur);
	}

	return g_string_free (gstr, FALSE);
}
