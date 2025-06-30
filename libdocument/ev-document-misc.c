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

static cairo_surface_t *
ev_document_misc_render_thumbnail_frame (GtkWidget       *widget,
                                         int              width,
                                         int              height,
                                         gboolean         inverted_colors,
                                         GdkPixbuf       *source_pixbuf,
                                         cairo_surface_t *source_surface)
{
        GtkStyleContext *context = gtk_widget_get_style_context (widget);
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
                cairo_surface_get_device_scale (source_surface, &device_scale_x, &device_scale_y);
        } else if (source_pixbuf) {
                g_return_val_if_fail (GDK_IS_PIXBUF (source_pixbuf), NULL);

                width_r = gdk_pixbuf_get_width (source_pixbuf);
                height_r = gdk_pixbuf_get_height (source_pixbuf);
                device_scale_x = device_scale_y = gtk_widget_get_scale_factor (widget);
        } else {
                width_r = width;
                height_r = height;
                device_scale_x = device_scale_y = gtk_widget_get_scale_factor (widget);
        }

        width_r /= device_scale_x;
        height_r /= device_scale_y;

        gtk_style_context_save (context);

        gtk_style_context_add_class (context, "page-thumbnail");
        if (inverted_colors)
                gtk_style_context_add_class (context, "inverted");

        gtk_style_context_get_border (context, &border);
        width_f = width_r + border.left + border.right;
        height_f = height_r + border.top + border.bottom;

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                              device_scale_x * width_f,
                                              device_scale_y * height_f);

        cairo_surface_set_device_scale (surface, device_scale_x, device_scale_y);

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

/**
 * ev_document_misc_get_widget_dpi:
 * @widget: a #GtkWidget
 *
 * Returns sensible guess for DPI of monitor on which given widget has been
 * realized. If HiDPI display, use 192, else 96.
 * Returns 96 as fallback value.
 *
 * Returns: DPI as gdouble
 */
gdouble
ev_document_misc_get_widget_dpi (GtkWidget *widget)
{
	GdkDisplay   *display = gtk_widget_get_display (widget);
	GtkNative    *native = gtk_widget_get_native (widget);
	GdkSurface   *surface = NULL;
	GdkMonitor   *monitor = NULL;
	gboolean      is_landscape;
	GdkRectangle  geometry;

	if (native != NULL)
		surface = gtk_native_get_surface (native);

	if (surface != NULL) {
		monitor = gdk_display_get_monitor_at_surface (display, surface);
	}

	/* The only safe assumption you can make, on Unix-like/X11 and
	 * Linux/Wayland, is to always set the DPI to 96, regardless of
	 * physical/logical resolution, because that's the only safe
	 * guarantee we can make.
	 * https://gitlab.gnome.org/GNOME/gtk/-/issues/3115#note_904622 */
	if (monitor == NULL)
		return 96;

	gdk_monitor_get_geometry (monitor, &geometry);
	is_landscape = geometry.width > geometry.height;

	/* DPI is 192 if height ≥ 1080 and the orientation is not portrait,
	 * which is, incidentally, how GTK detects HiDPI displays and set a
	 * scaling factor for the logical output
	 * https://gitlab.gnome.org/GNOME/gtk/-/issues/3115#note_904622 */
	if (is_landscape && geometry.height >= 1080)
		return 192;
	else
		return 96;
}

/**
 * ev_document_misc_format_datetime:
 * @dt: a #GDateTime
 *
 * Determine the preferred date and time representation for the current locale
 * for @dt.
 *
 * Returns: (transfer full): a new allocated string or NULL in the case
 * that there was an error (such as a format specifier not being supported
 * in the current locale). The string should be freed with g_free().
 *
 * Since: 3.38
 */
gchar *
ev_document_misc_format_datetime (GDateTime *dt)
{
	return g_date_time_format (dt, "%c");
}

/**
 * ev_document_misc_get_pointer_position:
 * @widget: a #GtkWidget
 * @x: (out): the pointer's "x" position, or -1 if the position is not
 *   available
 * @y: (out): the pointer's "y" position, or -1 if the position is not
 *   available
 *
 * Get the pointer's x and y position relative to @widget.
 */
void
ev_document_misc_get_pointer_position (GtkWidget *widget,
                                       gint      *x,
                                       gint      *y)
{
	ev_document_misc_get_pointer_position_impl (widget, x, y);
}

gboolean
ev_document_misc_get_pointer_position_impl (GtkWidget *widget,
                                            gint      *x,
                                            gint      *y)
{
	gdouble     dx, dy;
	GdkSeat    *seat;
	GtkNative  *native;
	GdkDevice  *device_pointer;
	GdkSurface *surface;

	if (x)
		*x = -1;
	if (y)
		*y = -1;

	if (!gtk_widget_get_realized (widget))
		return FALSE;

	seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));

	device_pointer = gdk_seat_get_pointer (seat);
	native = gtk_widget_get_native (widget);

	if (!native)
		return FALSE;

	surface = gtk_native_get_surface (native);
	if (!surface)
		return FALSE;

	gdk_surface_get_device_position (surface,
					 device_pointer,
					 &dx, &dy, NULL);

	if (x)
		*x = dx;
	if (y)
		*y = dy;

	gtk_widget_translate_coordinates (widget, GTK_WIDGET (native), 0, 0, &dx, &dy);

	if (x)
		*x -= dx;
	if (y)
		*y -= dy;

	return TRUE;
}
