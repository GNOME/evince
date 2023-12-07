/*
 * Copyright (C) 2005, Jonathan Blandford <jrb@gnome.org>
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

/* FIXME: Should probably buffer calls to libtiff with TIFFSetWarningHandler
 */

#include "config.h"

#include <config.h>
#include <stdio.h>
#include <stdint.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "tiffio.h"
#include "tiff2ps.h"
#include "tiff-document.h"
#include "ev-document-info.h"
#include "ev-document-misc.h"
#include "ev-file-exporter.h"
#include "ev-file-helpers.h"

struct _TiffDocumentClass
{
  EvDocumentClass parent_class;
};

struct _TiffDocument
{
  EvDocument parent_instance;

  TIFF *tiff;
  gint n_pages;
  TIFF2PSContext *ps_export_ctx;

  gchar *uri;
};

typedef struct _TiffDocumentClass TiffDocumentClass;

static void tiff_document_document_file_exporter_iface_init (EvFileExporterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TiffDocument, tiff_document, EV_TYPE_DOCUMENT,
			 G_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER,
						tiff_document_document_file_exporter_iface_init))

static TIFFErrorHandler orig_error_handler = NULL;
static TIFFErrorHandler orig_warning_handler = NULL;

static void
push_handlers (void)
{
	orig_error_handler = TIFFSetErrorHandler (NULL);
	orig_warning_handler = TIFFSetWarningHandler (NULL);
}

static void
pop_handlers (void)
{
	TIFFSetErrorHandler (orig_error_handler);
	TIFFSetWarningHandler (orig_warning_handler);
}

static gboolean
tiff_document_load (EvDocument  *document,
		    const char  *uri,
		    GError     **error)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);
	gchar *filename;
	TIFF *tiff;

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	push_handlers ();

#ifdef G_OS_WIN32
{
	wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, error);
	if (wfilename == NULL) {
		return FALSE;
	}

	tiff = TIFFOpenW (wfilename, "r");

	g_free (wfilename);
}
#else
	tiff = TIFFOpen (filename, "r");
#endif
	if (tiff) {
		guint32 w, h;

		/* FIXME: unused data? why bother here */
		TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);
	}

	if (!tiff) {
		pop_handlers ();

    		g_set_error_literal (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     _("Invalid document"));

		g_free (filename);
		return FALSE;
	}

	tiff_document->tiff = tiff;
	g_free (tiff_document->uri);
	g_free (filename);
	tiff_document->uri = g_strdup (uri);

	pop_handlers ();
	return TRUE;
}

static gboolean
tiff_document_save (EvDocument  *document,
		    const char  *uri,
		    GError     **error)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);

	return ev_xfer_uri_simple (tiff_document->uri, uri, error);
}

static int
tiff_document_get_n_pages (EvDocument  *document)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);

	g_return_val_if_fail (TIFF_IS_DOCUMENT (document), 0);
	g_return_val_if_fail (tiff_document->tiff != NULL, 0);

	if (tiff_document->n_pages == -1) {
		push_handlers ();
		tiff_document->n_pages = 0;

		do {
			tiff_document->n_pages ++;
		}
		while (TIFFReadDirectory (tiff_document->tiff));
		pop_handlers ();
	}

	return tiff_document->n_pages;
}

static void
tiff_document_get_resolution (TiffDocument *tiff_document,
			      gfloat       *x_res,
			      gfloat       *y_res)
{
	gfloat x = 0.0;
	gfloat y = 0.0;
	gushort unit;

	if (TIFFGetField (tiff_document->tiff, TIFFTAG_XRESOLUTION, &x) &&
	    TIFFGetField (tiff_document->tiff, TIFFTAG_YRESOLUTION, &y)) {
		if (TIFFGetFieldDefaulted (tiff_document->tiff, TIFFTAG_RESOLUTIONUNIT, &unit)) {
			if (unit == RESUNIT_CENTIMETER) {
				x *= 2.54;
				y *= 2.54;
			}
		}
	}

	/* Handle 0 values: some software set TIFF resolution as `0 , 0` see bug #646414 */
	*x_res = x > 0 ? x : 72.0;
	*y_res = y > 0 ? y : 72.0;
}

static void
tiff_document_get_page_size (EvDocument *document,
			     EvPage     *page,
			     double     *width,
			     double     *height)
{
	guint32 w, h;
	gfloat x_res, y_res;
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);

	g_return_if_fail (TIFF_IS_DOCUMENT (document));
	g_return_if_fail (tiff_document->tiff != NULL);

	push_handlers ();
	if (TIFFSetDirectory (tiff_document->tiff, page->index) != 1) {
		pop_handlers ();
		return;
	}

	TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGELENGTH, &h);
	tiff_document_get_resolution (tiff_document, &x_res, &y_res);
	h = h * (x_res / y_res);

	*width = w;
	*height = h;

	pop_handlers ();
}

static cairo_surface_t *
tiff_document_render (EvDocument      *document,
		      EvRenderContext *rc)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);
	int width, height;
	int scaled_width, scaled_height;
	float x_res, y_res;
	gint rowstride, bytes;
	guchar *pixels = NULL;
	guchar *p;
	int orientation;
	cairo_surface_t *surface;
	cairo_surface_t *rotated_surface;
	static const cairo_user_data_key_t key;

	g_return_val_if_fail (TIFF_IS_DOCUMENT (document), NULL);
	g_return_val_if_fail (tiff_document->tiff != NULL, NULL);

	push_handlers ();
	if (TIFFSetDirectory (tiff_document->tiff, rc->page->index) != 1) {
		pop_handlers ();
		g_warning("Failed to select page %d", rc->page->index);
		return NULL;
	}

	if (!TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGEWIDTH, &width)) {
		pop_handlers ();
		g_warning("Failed to read image width");
		return NULL;
	}

	if (! TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGELENGTH, &height)) {
		pop_handlers ();
		g_warning("Failed to read image height");
		return NULL;
	}

	if (! TIFFGetField (tiff_document->tiff, TIFFTAG_ORIENTATION, &orientation)) {
		orientation = ORIENTATION_TOPLEFT;
	}

	tiff_document_get_resolution (tiff_document, &x_res, &y_res);

	pop_handlers ();

	/* Sanity check the doc */
	if (width <= 0 || height <= 0) {
		g_warning("Invalid width or height.");
		return NULL;
	}

	rowstride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width);
	if (rowstride / 4 != width) {
		g_warning("Overflow while rendering document.");
		/* overflow, or cairo was changed in an unsupported way */
		return NULL;
	}

	if (height >= INT_MAX / rowstride) {
		g_warning("Overflow while rendering document.");
		/* overflow */
		return NULL;
	}
	bytes = height * rowstride;

	pixels = g_try_malloc (bytes);
	if (!pixels) {
		g_warning("Failed to allocate memory for rendering.");
		return NULL;
	}

	if (!TIFFReadRGBAImageOriented (tiff_document->tiff,
					width, height,
					(uint32_t *)pixels,
					orientation, 0)) {
		g_warning ("Failed to read TIFF image.");
		g_free (pixels);
		return NULL;
	}

	surface = cairo_image_surface_create_for_data (pixels,
						       CAIRO_FORMAT_RGB24,
						       width, height,
						       rowstride);
	cairo_surface_set_user_data (surface, &key,
				     pixels, (cairo_destroy_func_t)g_free);
	pop_handlers ();

	/* Convert the format returned by libtiff to
	* what cairo expects
	*/
	p = pixels;
	while (p < pixels + bytes) {
		guint32 *pixel = (guint32*)p;
		guint8 r = TIFFGetR(*pixel);
		guint8 g = TIFFGetG(*pixel);
		guint8 b = TIFFGetB(*pixel);
		guint8 a = TIFFGetA(*pixel);

		*pixel = (a << 24) | (r << 16) | (g << 8) | b;

		p += 4;
	}

	ev_render_context_compute_scaled_size (rc, width, height * (x_res / y_res),
					       &scaled_width, &scaled_height);
	rotated_surface = ev_document_misc_surface_rotate_and_scale (surface,
								     scaled_width, scaled_height,
								     rc->rotation);
	cairo_surface_destroy (surface);

	return rotated_surface;
}

static void
free_buffer (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

static GdkPixbuf *
tiff_document_get_thumbnail (EvDocument      *document,
			     EvRenderContext *rc)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);
	int width, height;
	int scaled_width, scaled_height;
	float x_res, y_res;
	gint rowstride, bytes;
	guchar *pixels = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;
	GdkPixbuf *rotated_pixbuf;

	push_handlers ();
	if (TIFFSetDirectory (tiff_document->tiff, rc->page->index) != 1) {
		pop_handlers ();
		return NULL;
	}

	if (!TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGEWIDTH, &width)) {
		pop_handlers ();
		return NULL;
	}

	if (! TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGELENGTH, &height)) {
		pop_handlers ();
		return NULL;
	}

	tiff_document_get_resolution (tiff_document, &x_res, &y_res);

	pop_handlers ();

	/* Sanity check the doc */
	if (width <= 0 || height <= 0)
		return NULL;

	if (width >= INT_MAX / 4)
		/* overflow */
		return NULL;
	rowstride = width * 4;

	if (height >= INT_MAX / rowstride)
		/* overflow */
		return NULL;
	bytes = height * rowstride;

	pixels = g_try_malloc (bytes);
	if (!pixels)
		return NULL;

	if (!TIFFReadRGBAImageOriented (tiff_document->tiff,
					width, height,
					(uint32_t *)pixels,
					ORIENTATION_TOPLEFT, 0)) {
		g_free (pixels);
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, TRUE, 8,
					   width, height, rowstride,
					   free_buffer, NULL);
	pop_handlers ();

	ev_render_context_compute_scaled_size (rc, width, height * (x_res / y_res),
					       &scaled_width, &scaled_height);
	scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
						 scaled_width, scaled_height,
						 GDK_INTERP_BILINEAR);
	g_object_unref (pixbuf);

	rotated_pixbuf = gdk_pixbuf_rotate_simple (scaled_pixbuf, 360 - rc->rotation);
	g_object_unref (scaled_pixbuf);

	return rotated_pixbuf;
}

static gchar *
tiff_document_get_page_label (EvDocument *document,
			      EvPage     *page)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);
	static gchar *label;

	if (TIFFGetField (tiff_document->tiff, TIFFTAG_PAGENAME, &label) &&
	    g_utf8_validate (label, -1, NULL)) {
		return g_strdup (label);
	}

	return NULL;
}

static EvDocumentInfo *
tiff_document_get_info (EvDocument *document)
{
        TiffDocument *tiff_document = TIFF_DOCUMENT (document);
        EvDocumentInfo *info;
        const void *data;
        uint32_t size;

        info = ev_document_info_new ();

        if (TIFFGetField (tiff_document->tiff, TIFFTAG_XMLPACKET, &size, &data) == 1) {
                ev_document_info_set_from_xmp (info, (const char*)data, size);
        }

        return info;
}

static void
tiff_document_finalize (GObject *object)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (object);

	if (tiff_document->tiff)
		TIFFClose (tiff_document->tiff);
	if (tiff_document->uri)
		g_free (tiff_document->uri);

	G_OBJECT_CLASS (tiff_document_parent_class)->finalize (object);
}

static void
tiff_document_class_init (TiffDocumentClass *klass)
{
	GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
	EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

	gobject_class->finalize = tiff_document_finalize;

	ev_document_class->load = tiff_document_load;
	ev_document_class->save = tiff_document_save;
	ev_document_class->get_n_pages = tiff_document_get_n_pages;
	ev_document_class->get_page_size = tiff_document_get_page_size;
	ev_document_class->render = tiff_document_render;
	ev_document_class->get_thumbnail = tiff_document_get_thumbnail;
	ev_document_class->get_page_label = tiff_document_get_page_label;
	ev_document_class->get_info = tiff_document_get_info;
}

/* postscript exporter implementation */
static void
tiff_document_file_exporter_begin (EvFileExporter        *exporter,
				   EvFileExporterContext *fc)
{
	TiffDocument *document = TIFF_DOCUMENT (exporter);

	document->ps_export_ctx = tiff2ps_context_new(fc->filename);
}

static void
tiff_document_file_exporter_do_page (EvFileExporter *exporter, EvRenderContext *rc)
{
	TiffDocument *document = TIFF_DOCUMENT (exporter);

	if (document->ps_export_ctx == NULL)
		return;
	if (TIFFSetDirectory (document->tiff, rc->page->index) != 1)
		return;
	tiff2ps_process_page (document->ps_export_ctx, document->tiff,
			      0, 0, 0, 0, 0);
}

static void
tiff_document_file_exporter_end (EvFileExporter *exporter)
{
	TiffDocument *document = TIFF_DOCUMENT (exporter);

	if (document->ps_export_ctx == NULL)
		return;
	tiff2ps_context_finalize(document->ps_export_ctx);
}

static EvFileExporterCapabilities
tiff_document_file_exporter_get_capabilities (EvFileExporter *exporter)
{
	return  EV_FILE_EXPORTER_CAN_PAGE_SET |
		EV_FILE_EXPORTER_CAN_COPIES |
		EV_FILE_EXPORTER_CAN_COLLATE |
		EV_FILE_EXPORTER_CAN_REVERSE |
		EV_FILE_EXPORTER_CAN_GENERATE_PS;
}

static void
tiff_document_document_file_exporter_iface_init (EvFileExporterInterface *iface)
{
	iface->begin = tiff_document_file_exporter_begin;
	iface->do_page = tiff_document_file_exporter_do_page;
	iface->end = tiff_document_file_exporter_end;
	iface->get_capabilities = tiff_document_file_exporter_get_capabilities;
}

static void
tiff_document_init (TiffDocument *tiff_document)
{
	tiff_document->n_pages = -1;
}

GType
ev_backend_query_type (void)
{
	return TIFF_TYPE_DOCUMENT;
}
