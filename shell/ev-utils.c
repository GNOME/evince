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

typedef struct
{
  int size;
  double *data;
} ConvFilter;

static double
gaussian (double x, double y, double r)
{
    return ((1 / (2 * M_PI * r)) *
	    exp ((- (x * x + y * y)) / (2 * r * r)));
}

static ConvFilter *
create_blur_filter (int radius)
{
  ConvFilter *filter;
  int x, y;
  double sum;
  
  filter = g_new0 (ConvFilter, 1);
  filter->size = radius * 2 + 1;
  filter->data = g_new (double, filter->size * filter->size);

  sum = 0.0;
  
  for (y = 0 ; y < filter->size; y++)
    {
      for (x = 0 ; x < filter->size; x++)
	{
	  sum += filter->data[y * filter->size + x] = gaussian (x - (filter->size >> 1),
								y - (filter->size >> 1),
								radius);
	}
    }

  for (y = 0; y < filter->size; y++)
    {
      for (x = 0; x < filter->size; x++)
	{
	  filter->data[y * filter->size + x] /= sum;
	}
    }

  return filter;
  
}

static GdkPixbuf *
create_shadow (GdkPixbuf *src, int blur_radius,
	       int x_offset, int y_offset, double opacity)
{
  int x, y, i, j;
  int width, height;
  GdkPixbuf *dest;
  static ConvFilter *filter = NULL;
  int src_rowstride, dest_rowstride;
  int src_bpp, dest_bpp;
  
  guchar *src_pixels, *dest_pixels;

  if (!filter)
    filter = create_blur_filter (blur_radius);

  if (x_offset < 0)
	  x_offset = (blur_radius * 4) / 5;
  
  if (y_offset < 0)
	  y_offset = (blur_radius * 4) / 5;

  
  width = gdk_pixbuf_get_width (src) + blur_radius * 2 + x_offset;
  height = gdk_pixbuf_get_height (src) + blur_radius * 2 + y_offset;

  dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src), TRUE,
			 gdk_pixbuf_get_bits_per_sample (src),
			 width, height);
  gdk_pixbuf_fill (dest, 0);  
  src_pixels = gdk_pixbuf_get_pixels (src);
  src_rowstride = gdk_pixbuf_get_rowstride (src);
  src_bpp = gdk_pixbuf_get_has_alpha (src) ? 4 : 3;
  
  dest_pixels = gdk_pixbuf_get_pixels (dest);
  dest_rowstride = gdk_pixbuf_get_rowstride (dest);
  dest_bpp = gdk_pixbuf_get_has_alpha (dest) ? 4 : 3;
  
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
	{
	  int sumr = 0, sumg = 0, sumb = 0, suma = 0;

	  for (i = 0; i < filter->size; i++)
	    {
	      for (j = 0; j < filter->size; j++)
		{
		  int src_x, src_y;

		  src_y = -(blur_radius + x_offset) + y - (filter->size >> 1) + i;
		  src_x = -(blur_radius + y_offset) + x - (filter->size >> 1) + j;

		  if (src_y < 0 || src_y > gdk_pixbuf_get_height (src) ||
		      src_x < 0 || src_x > gdk_pixbuf_get_width (src))
		    continue;

		  sumr += src_pixels [src_y * src_rowstride +
				      src_x * src_bpp + 0] *
		    filter->data [i * filter->size + j];
		  sumg += src_pixels [src_y * src_rowstride +
				      src_x * src_bpp + 1] * 
		    filter->data [i * filter->size + j];

		  sumb += src_pixels [src_y * src_rowstride +
				      src_x * src_bpp + 2] * 
		    filter->data [i * filter->size + j];
		  
		  if (src_bpp == 4)
		    suma += src_pixels [src_y * src_rowstride +
					src_x * src_bpp + 3] *
		    filter->data [i * filter->size + j];
		  else
			  suma += 0xff;
		    
		}
	    }

	  if (dest_bpp == 4)
	    dest_pixels [y * dest_rowstride +
			 x * dest_bpp + 3] = (suma * opacity) / (filter->size * filter->size);

	}
    }
  
  return dest;
}

GdkPixbuf *
ev_pixbuf_add_shadow (GdkPixbuf *src, int size,
		      int x_offset, int y_offset, double opacity)
{
  GdkPixbuf *dest;
  
  dest = create_shadow (src, size, x_offset, y_offset, opacity);

  gdk_pixbuf_composite (src, dest,
			size, size,
			gdk_pixbuf_get_width (src),
			gdk_pixbuf_get_height (src),
 			size, size,
			1.0, 1.0,
			GDK_INTERP_NEAREST, 255);

  return dest;
}


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

/**
 * ev_gui_menu_popup_at_tree_view_selection:
 * @menu: a #GtkMenu to show
 * @tree_view: a #GtkTreeView
 *
 * Opens a popup menu positioned at the currently selected row of @tree_view.
 */
void
ev_gui_menu_popup_at_tree_view_selection (GtkMenu     *menu,
					  GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GList            *selected_rows;

	selection = gtk_tree_view_get_selection (tree_view);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (selected_rows) {
		GdkWindow     *window;
		GdkRectangle   rect;
		GtkAllocation  allocation;

	        window = gtk_widget_get_window (GTK_WIDGET (tree_view));
		gtk_tree_view_get_cell_area (tree_view, selected_rows->data,
					     NULL, &rect);
		gtk_tree_view_convert_bin_window_to_widget_coords (tree_view, 0, rect.y,
								   NULL, &rect.y);
		gtk_widget_get_allocation (GTK_WIDGET (tree_view), &allocation);
		rect.width = allocation.width;
		gtk_menu_popup_at_rect (menu, window, &rect,
					GDK_GRAVITY_SOUTH_WEST,
					GDK_GRAVITY_NORTH_WEST,
					NULL);
		g_list_free_full (selected_rows, (GDestroyNotify)gtk_tree_path_free);
	} else {
		gtk_menu_popup_at_widget (menu, GTK_WIDGET (tree_view),
					  GDK_GRAVITY_CENTER,
					  GDK_GRAVITY_CENTER,
					  NULL);
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
 * Replace all occurences of substr in str with repl
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
