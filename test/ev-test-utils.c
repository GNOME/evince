/*
 * Copyright (C) 2026 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ev-test-utils.h"

#define PIXEL_TOLERANCE 48

gchar *
ev_test_get_uri (const gchar *filename)
{
	g_autofree gchar *path = NULL;
	gchar *uri;

	path = g_test_build_filename (G_TEST_DIST, "data", filename, NULL);
	uri = g_filename_to_uri (path, NULL, NULL);
	g_assert_nonnull (uri);

	return uri;
}

EvDocument *
ev_test_load_document (const gchar *filename,
		       GError     **error)
{
	g_autofree gchar *uri = ev_test_get_uri (filename);

	return ev_document_factory_get_document (uri, error);
}

EvDocument *
ev_test_load_document_or_fail (const gchar *filename)
{
	g_autoptr(GError) error = NULL;
	EvDocument *document;

	document = ev_test_load_document (filename, &error);
	g_assert_no_error (error);
	g_assert_true (EV_IS_DOCUMENT (document));

	return document;
}

gchar *
ev_test_get_page_text (EvDocument *document,
		       gint        index)
{
	EvPage *page;
	gchar *text;

	g_assert_true (EV_IS_DOCUMENT_TEXT (document));

	page = ev_document_get_page (document, index);
	text = ev_document_text_get_text (EV_DOCUMENT_TEXT (document), page);
	g_object_unref (page);

	return text;
}

cairo_surface_t *
ev_test_render_page (EvDocument *document,
		     gint        index,
		     gint        rotation,
		     gdouble     scale)
{
	EvPage *page;
	EvRenderContext *rc;
	cairo_surface_t *surface;

	page = ev_document_get_page (document, index);
	rc = ev_render_context_new (page, rotation, scale);

	ev_document_doc_mutex_lock ();
	surface = ev_document_render (document, rc);
	ev_document_doc_mutex_unlock ();

	g_object_unref (rc);
	g_object_unref (page);

	g_assert_nonnull (surface);
	g_assert_cmpint (cairo_surface_status (surface), ==, CAIRO_STATUS_SUCCESS);
	g_assert_cmpint (cairo_surface_get_type (surface), ==, CAIRO_SURFACE_TYPE_IMAGE);

	return surface;
}

static guint32
get_pixel (cairo_surface_t *surface,
	   gint             x,
	   gint             y)
{
	guchar *data = cairo_image_surface_get_data (surface);
	gint stride = cairo_image_surface_get_stride (surface);
	guint32 pixel = *(guint32 *) (data + y * stride + x * 4);
	guint alpha;
	guint r, g, b;

	if (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_RGB24)
		return pixel & 0xffffff;

	alpha = pixel >> 24;
	if (alpha == 0)
		return 0xffffff;

	/* Unpremultiply and composite on white, as the views do */
	r = ((pixel >> 16) & 0xff) * 255 / alpha;
	g = ((pixel >> 8) & 0xff) * 255 / alpha;
	b = (pixel & 0xff) * 255 / alpha;
	r = (r * alpha + 255 * (255 - alpha)) / 255;
	g = (g * alpha + 255 * (255 - alpha)) / 255;
	b = (b * alpha + 255 * (255 - alpha)) / 255;

	return (r << 16) | (g << 8) | b;
}

void
ev_test_assert_pixel (cairo_surface_t *surface,
		      gint             x,
		      gint             y,
		      guint32          rgb)
{
	guint32 pixel;
	gint i;

	g_assert_cmpint (cairo_surface_get_type (surface), ==, CAIRO_SURFACE_TYPE_IMAGE);
	g_assert_cmpint (x, >=, 0);
	g_assert_cmpint (y, >=, 0);
	g_assert_cmpint (x, <, cairo_image_surface_get_width (surface));
	g_assert_cmpint (y, <, cairo_image_surface_get_height (surface));

	cairo_surface_flush (surface);
	pixel = get_pixel (surface, x, y);

	for (i = 0; i < 24; i += 8) {
		gint got = (pixel >> i) & 0xff;
		gint expected = (rgb >> i) & 0xff;

		if (ABS (got - expected) > PIXEL_TOLERANCE) {
			g_autofree gchar *msg = NULL;

			msg = g_strdup_printf ("pixel (%d, %d) is #%06x, expected #%06x",
					       x, y, pixel, rgb);
			g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__,
					     G_STRFUNC, msg);
		}
	}
}
