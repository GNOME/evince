/*
 *  Copyright (C) 2009 Juanjo Marín <juanj.marin@juntadeandalucia.es>
 *  Copyright (c) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 */

#include <config.h>

#include <string.h>
#include <math.h>

#include <gtk/gtk.h>

#include "ev-document-misc.h"

/* Returns a new GdkPixbuf that is suitable for placing in the thumbnail view.
 * It is four pixels wider and taller than the source.  If source_pixbuf is not
 * NULL, then it will fill the return pixbuf with the contents of
 * source_pixbuf.
 */
static GdkPixbuf *
create_thumbnail_frame (int        width,
			int        height,
			GdkPixbuf *source_pixbuf,
			gboolean   fill_bg)
{
	GdkPixbuf *retval;
	guchar *data;
	gint rowstride;
	int i;
	int width_r, height_r;

	if (source_pixbuf)
		g_return_val_if_fail (GDK_IS_PIXBUF (source_pixbuf), NULL);

	if (source_pixbuf) {
		width_r = gdk_pixbuf_get_width (source_pixbuf);
		height_r = gdk_pixbuf_get_height (source_pixbuf);
	} else {
		width_r = width;
		height_r = height;
	}

	/* make sure no one is passing us garbage */
	g_return_val_if_fail (width_r >= 0 && height_r >= 0, NULL);

	retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 TRUE, 8,
				 width_r + 4,
				 height_r + 4);

	/* make it black and fill in the middle */
	data = gdk_pixbuf_get_pixels (retval);
	rowstride = gdk_pixbuf_get_rowstride (retval);

	gdk_pixbuf_fill (retval, 0x000000ff);
	if (fill_bg) {
		for (i = 1; i < height_r + 1; i++)
			memset (data + (rowstride * i) + 4, 0xffffffff, width_r * 4);
	}

	/* copy the source pixbuf */
	if (source_pixbuf)
		gdk_pixbuf_copy_area (source_pixbuf, 0, 0,
				      width_r,
				      height_r,
				      retval,
				      1, 1);
	/* Add the corner */
	data [(width_r + 2) * 4 + 3] = 0;
	data [(width_r + 3) * 4 + 3] = 0;
	data [(width_r + 2) * 4 + (rowstride * 1) + 3] = 0;
	data [(width_r + 3) * 4 + (rowstride * 1) + 3] = 0;

	data [(height_r + 2) * rowstride + 3] = 0;
	data [(height_r + 3) * rowstride + 3] = 0;
	data [(height_r + 2) * rowstride + 4 + 3] = 0;
	data [(height_r + 3) * rowstride + 4 + 3] = 0;

	return retval;
}

/**
 * ev_document_misc_get_thumbnail_frame:
 * @width: the desired width
 * @height: the desired height
 * @source_pixbuf: a #GdkPixbuf
 *
 * Returns: (transfer full): a #GdkPixbuf
 */
GdkPixbuf *
ev_document_misc_get_thumbnail_frame (int        width,
				      int        height,
				      GdkPixbuf *source_pixbuf)
{
	return create_thumbnail_frame (width, height, source_pixbuf, TRUE);
}

/**
 * ev_document_misc_get_loading_thumbnail:
 * @width: the desired width
 * @height: the desired height
 * @inverted_colors: whether to invert colors
 *
 * Returns: (transfer full): a #GdkPixbuf
 */
GdkPixbuf *
ev_document_misc_get_loading_thumbnail (int      width,
					int      height,
					gboolean inverted_colors)
{
	return create_thumbnail_frame (width, height, NULL, !inverted_colors);
}

static cairo_surface_t *
ev_document_misc_render_thumbnail_frame (GtkWidget       *widget,
                                         int              width,
                                         int              height,
                                         gboolean         inverted_colors,
                                         GdkPixbuf       *source_pixbuf,
                                         cairo_surface_t *source_surface)
{
        GtkStyleContext *context = gtk_widget_get_style_context (widget);
        GtkStateFlags    state = gtk_widget_get_state_flags (widget);
        double           width_r, height_r;
        double           width_f, height_f;
        cairo_surface_t *surface;
        cairo_t         *cr;
        double           device_scale_x = 1;
        double           device_scale_y = 1;
        GtkBorder        border = {0, };

        if (source_surface) {
                width_r = cairo_image_surface_get_width (source_surface);
                height_r = cairo_image_surface_get_height (source_surface);
#ifdef HAVE_HIDPI_SUPPORT
                cairo_surface_get_device_scale (source_surface, &device_scale_x, &device_scale_y);
#endif
        } else if (source_pixbuf) {
                g_return_val_if_fail (GDK_IS_PIXBUF (source_pixbuf), NULL);

                width_r = gdk_pixbuf_get_width (source_pixbuf);
                height_r = gdk_pixbuf_get_height (source_pixbuf);
#ifdef HAVE_HIDPI_SUPPORT
                device_scale_x = device_scale_y = gtk_widget_get_scale_factor (widget);
#endif
        } else {
                width_r = width;
                height_r = height;
#ifdef HAVE_HIDPI_SUPPORT
                device_scale_x = device_scale_y = gtk_widget_get_scale_factor (widget);
#endif
        }

        width_r /= device_scale_x;
        height_r /= device_scale_y;

        gtk_style_context_save (context);

        gtk_style_context_add_class (context, "page-thumbnail");
        if (inverted_colors)
                gtk_style_context_add_class (context, "inverted");

        gtk_style_context_get_border (context, state, &border);
        width_f = width_r + border.left + border.right;
        height_f = height_r + border.top + border.bottom;

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                              device_scale_x * width_f,
                                              device_scale_y * height_f);

#ifdef HAVE_HIDPI_SUPPORT
        cairo_surface_set_device_scale (surface, device_scale_x, device_scale_y);
#endif

        cr = cairo_create (surface);
        if (source_surface) {
                cairo_set_source_surface (cr, source_surface, border.left, border.top);
                cairo_paint (cr);
        } else if (source_pixbuf) {
                gdk_cairo_set_source_pixbuf (cr, source_pixbuf, border.left, border.top);
                cairo_paint (cr);
        } else {
                gtk_render_background (context, cr, 0, 0, width_f, height_f);
        }
        gtk_render_frame (context, cr, 0, 0, width_f, height_f);
        cairo_destroy (cr);

        gtk_style_context_restore (context);

        return surface;
}

/**
 * ev_document_misc_render_loading_thumbnail:
 * @widget: a #GtkWidget to use for style information
 * @width: the desired width
 * @height: the desired height
 * @inverted_colors: whether to invert colors
 *
 * Returns: (transfer full): a #GdkPixbuf
 *
 * Since: 3.8
 */
GdkPixbuf *
ev_document_misc_render_loading_thumbnail (GtkWidget *widget,
                                           int        width,
                                           int        height,
                                           gboolean   inverted_colors)
{
        GdkPixbuf *retval;
        cairo_surface_t *surface;

        surface = ev_document_misc_render_thumbnail_frame (widget, width, height, inverted_colors, NULL, NULL);
        retval = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
        cairo_surface_destroy (surface);

        return retval;
}

/**
 * ev_document_misc_render_loading_thumbnail_surface:
 * @widget: a #GtkWidget to use for style information
 * @width: the desired width
 * @height: the desired height
 * @inverted_colors: whether to invert colors
 *
 * Returns: (transfer full): a #cairo_surface_t
 *
 * Since: 3.14
 */
cairo_surface_t *
ev_document_misc_render_loading_thumbnail_surface (GtkWidget *widget,
                                                   int        width,
                                                   int        height,
                                                   gboolean   inverted_colors)
{
        return ev_document_misc_render_thumbnail_frame (widget, width, height, inverted_colors, NULL, NULL);
}

/**
 * ev_document_misc_render_thumbnail_with_frame:
 * @widget: a #GtkWidget to use for style information
 * @source_pixbuf: a #GdkPixbuf
 *
 * Returns: (transfer full): a #GdkPixbuf
 *
 * Since: 3.8
 */
GdkPixbuf *
ev_document_misc_render_thumbnail_with_frame (GtkWidget *widget,
                                              GdkPixbuf *source_pixbuf)
{
        GdkPixbuf *retval;
        cairo_surface_t *surface;

        surface = ev_document_misc_render_thumbnail_frame (widget, -1, -1, FALSE, source_pixbuf, NULL);
        retval = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                              cairo_image_surface_get_width (surface),
                                              cairo_image_surface_get_height (surface));
        cairo_surface_destroy (surface);

        return retval;
}

/**
 * ev_document_misc_render_thumbnail_surface_with_frame:
 * @widget: a #GtkWidget to use for style information
 * @source_surface: a #cairo_surface_t
 * @width: the desired width
 * @height: the desired height
 *
 * Returns: (transfer full): a #cairo_surface_t
 *
 * Since: 3.14
 */
cairo_surface_t *
ev_document_misc_render_thumbnail_surface_with_frame (GtkWidget       *widget,
                                                      cairo_surface_t *source_surface,
                                                      int              width,
                                                      int              height)
{
        return ev_document_misc_render_thumbnail_frame (widget, width, height, FALSE, NULL, source_surface);
}

void
ev_document_misc_get_page_border_size (gint       page_width,
				       gint       page_height,
				       GtkBorder *border)
{
	g_assert (border);

	border->left = 1;
	border->top = 1;
	if (page_width < 100) {
		border->right = 2;
		border->bottom = 2;
	} else if (page_width < 500) {
		border->right = 3;
		border->bottom = 3;
	} else {
		border->right = 4;
		border->bottom = 4;
	}
}


void
ev_document_misc_paint_one_page (cairo_t      *cr,
				 GtkWidget    *widget,
				 GdkRectangle *area,
				 GtkBorder    *border,
				 gboolean      highlight,
				 gboolean      inverted_colors)
{
	GtkStyleContext *context = gtk_widget_get_style_context (widget);
	GtkStateFlags state = gtk_widget_get_state_flags (widget);
        GdkRGBA fg, bg, shade_bg;

        gtk_style_context_get_background_color (context, state, &bg);
        gtk_style_context_get_color (context, state, &fg);
        gtk_style_context_get_color (context, GTK_STATE_FLAG_INSENSITIVE, &shade_bg);

	gdk_cairo_set_source_rgba (cr, highlight ? &fg : &shade_bg);
	gdk_cairo_rectangle (cr, area);
	cairo_fill (cr);

	if (inverted_colors)
		cairo_set_source_rgb (cr, 0, 0, 0);
	else
		cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle (cr,
			 area->x + border->left,
			 area->y + border->top,
			 area->width - (border->left + border->right),
			 area->height - (border->top + border->bottom));
	cairo_fill (cr);

	gdk_cairo_set_source_rgba (cr, &bg);
	cairo_rectangle (cr,
			 area->x,
			 area->y + area->height - (border->bottom - border->top),
			 border->bottom - border->top,
			 border->bottom - border->top);
	cairo_fill (cr);

	cairo_rectangle (cr,
			 area->x + area->width - (border->right - border->left),
			 area->y,
			 border->right - border->left,
			 border->right - border->left);
	cairo_fill (cr);
}

cairo_surface_t *
ev_document_misc_surface_from_pixbuf (GdkPixbuf *pixbuf)
{
	cairo_surface_t *surface;
	cairo_t         *cr;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	surface = cairo_image_surface_create (gdk_pixbuf_get_has_alpha (pixbuf) ?
					      CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
					      gdk_pixbuf_get_width (pixbuf),
					      gdk_pixbuf_get_height (pixbuf));
	cr = cairo_create (surface);
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);
	
	return surface;
}

/**
 * ev_document_misc_pixbuf_from_surface:
 * @surface: a #cairo_surface_t
 *
 * Returns: (transfer full): a #GdkPixbuf
 */
GdkPixbuf *
ev_document_misc_pixbuf_from_surface (cairo_surface_t *surface)
{
	g_return_val_if_fail (surface, NULL);	

        return gdk_pixbuf_get_from_surface (surface,
                                            0, 0,
                                            cairo_image_surface_get_width (surface),
                                            cairo_image_surface_get_height (surface));
}

cairo_surface_t *
ev_document_misc_surface_rotate_and_scale (cairo_surface_t *surface,
					   gint             dest_width,
					   gint             dest_height,
					   gint             dest_rotation)
{
	cairo_surface_t *new_surface;
	cairo_t         *cr;
	gint             width, height;
	gint             new_width = dest_width;
	gint             new_height = dest_height;

	width = cairo_image_surface_get_width (surface);
	height = cairo_image_surface_get_height (surface);
	
	if (dest_width == width &&
	    dest_height == height &&
	    dest_rotation == 0) {
		return cairo_surface_reference (surface);
	}

	if (dest_rotation == 90 || dest_rotation == 270) {
		new_width = dest_height;
		new_height = dest_width;
	}

	new_surface = cairo_surface_create_similar (surface,
						    cairo_surface_get_content (surface),
						    new_width, new_height);

	cr = cairo_create (new_surface);
	switch (dest_rotation) {
	        case 90:
			cairo_translate (cr, new_width, 0);
			break;
	        case 180:
			cairo_translate (cr, new_width, new_height);
			break;
	        case 270:
			cairo_translate (cr, 0, new_height);
			break;
	        default:
			cairo_translate (cr, 0, 0);
	}
	cairo_rotate (cr, dest_rotation * G_PI / 180.0);
	
	if (dest_width != width || dest_height != height) {
		cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_BILINEAR);
		cairo_scale (cr,
			     (gdouble)dest_width / width,
			     (gdouble)dest_height / height);
	}
	
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);

	return new_surface;
}

void
ev_document_misc_invert_surface (cairo_surface_t *surface) {
	cairo_t *cr;

	cr = cairo_create (surface);

	/* white + DIFFERENCE -> invert */
	cairo_set_operator (cr, CAIRO_OPERATOR_DIFFERENCE);
	cairo_set_source_rgb (cr, 1., 1., 1.);
	cairo_paint(cr);
	cairo_destroy (cr);
}

void
ev_document_misc_invert_pixbuf (GdkPixbuf *pixbuf)
{
	guchar *data, *p;
	guint   width, height, x, y, rowstride, n_channels;

	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
	g_assert (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);

	/* First grab a pointer to the raw pixel data. */
	data = gdk_pixbuf_get_pixels (pixbuf);

	/* Find the number of bytes per row (could be padded). */
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	for (x = 0; x < width; x++) {
		for (y = 0; y < height; y++) {
			/* Calculate pixel's offset into the data array. */
			p = data + x * n_channels + y * rowstride;
			/* Change the RGB values*/
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}
}

gdouble
ev_document_misc_get_screen_dpi (GdkScreen *screen)
{
	gdouble dp, di;

	/*diagonal in pixels*/
	dp = hypot (gdk_screen_get_width (screen), gdk_screen_get_height (screen));

	/*diagonal in inches*/
	di = hypot (gdk_screen_get_width_mm(screen), gdk_screen_get_height_mm (screen)) / 25.4;

	return (dp / di);
}

/* Returns a locale specific date and time representation */
gchar *
ev_document_misc_format_date (GTime utime)
{
	time_t time = (time_t) utime;
	char s[256];
	const char fmt_hack[] = "%c";
	size_t len;
#ifdef HAVE_LOCALTIME_R
	struct tm t;
	if (time == 0 || !localtime_r (&time, &t)) return NULL;
	len = strftime (s, sizeof (s), fmt_hack, &t);
#else
	struct tm *t;
	if (time == 0 || !(t = localtime (&time)) ) return NULL;
	len = strftime (s, sizeof (s), fmt_hack, t);
#endif

	if (len == 0 || s[0] == '\0') return NULL;

	return g_locale_to_utf8 (s, -1, NULL, NULL, NULL);
}

void
ev_document_misc_get_pointer_position (GtkWidget *widget,
                                       gint      *x,
                                       gint      *y)
{
        GdkDeviceManager *device_manager;
        GdkDevice        *device_pointer;
        GdkRectangle      allocation;

        if (x)
                *x = -1;
        if (y)
                *y = -1;

        if (!gtk_widget_get_realized (widget))
                return;

        device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
        device_pointer = gdk_device_manager_get_client_pointer (device_manager);
        gdk_window_get_device_position (gtk_widget_get_window (widget),
                                        device_pointer,
                                        x, y, NULL);

        if (gtk_widget_get_has_window (widget))
                return;

        gtk_widget_get_allocation (widget, &allocation);
        if (x)
                *x -= allocation.x;
        if (y)
                *y -= allocation.y;
}
