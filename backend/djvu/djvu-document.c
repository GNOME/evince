/*
 * Copyright (C) 2005, Nickolay V. Shmyrev <nshmyrev@yandex.ru>
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

#include "config.h"

#include <config.h>
#include "djvu-document.h"
#include "djvu-text-page.h"
#include "djvu-links.h"
#include "djvu-document-private.h"
#include "ev-file-exporter.h"
#include "ev-document-info.h"
#include "ev-document-misc.h"
#include "ev-document-find.h"
#include "ev-document-links.h"
#include "ev-selection.h"
#include "ev-file-helpers.h"
#include "ev-document-text.h"

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n-lib.h>
#include <string.h>

enum {
	PROP_0,
	PROP_TITLE
};

struct _DjvuDocumentClass
{
	EvDocumentClass parent_class;
};

typedef struct _DjvuDocumentClass DjvuDocumentClass;

static void djvu_document_file_exporter_iface_init (EvFileExporterInterface *iface);
static void djvu_document_find_iface_init (EvDocumentFindInterface *iface);
static void djvu_document_document_links_iface_init  (EvDocumentLinksInterface *iface);
static void djvu_selection_iface_init (EvSelectionInterface *iface);
static void djvu_document_text_iface_init (EvDocumentTextInterface *iface);

G_DEFINE_TYPE_WITH_CODE (DjvuDocument,
			 djvu_document,
			 EV_TYPE_DOCUMENT,
			 G_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER,
						djvu_document_file_exporter_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND,
						djvu_document_find_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LINKS,
						djvu_document_document_links_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_SELECTION,
						djvu_selection_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_TEXT,
						djvu_document_text_iface_init))


#define EV_DJVU_ERROR ev_djvu_error_quark ()

static GQuark
ev_djvu_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_string ("ev-djvu-quark");

	return q;
}

static void
handle_message (const ddjvu_message_t *msg, GError **error)
{
	switch (msg->m_any.tag) {
	        case DDJVU_ERROR: {
			gchar *error_str;

			if (msg->m_error.filename) {
				error_str = g_strdup_printf ("DjvuLibre error: %s:%d",
							     msg->m_error.filename,
							     msg->m_error.lineno);
			} else {
				error_str = g_strdup_printf ("DjvuLibre error: %s",
							     msg->m_error.message);
			}

			if (error) {
				g_set_error_literal (error, EV_DJVU_ERROR, 0, error_str);
			} else {
				g_warning ("%s", error_str);
			}

			g_free (error_str);
			return;
			}
			break;
	        default:
			break;
	}
}

void
djvu_handle_events (DjvuDocument *djvu_document, int wait, GError **error)
{
	ddjvu_context_t *ctx = djvu_document->d_context;
	const ddjvu_message_t *msg;

	if (!ctx)
		return;

	if (wait)
		ddjvu_message_wait (ctx);

	while ((msg = ddjvu_message_peek (ctx))) {
		handle_message (msg, error);
		ddjvu_message_pop (ctx);
		if (error && *error)
			return;
	}
}

static void
djvu_wait_for_message (DjvuDocument *djvu_document, ddjvu_message_tag_t message, GError **error)
{
	ddjvu_context_t *ctx = djvu_document->d_context;
	const ddjvu_message_t *msg;

	ddjvu_message_wait (ctx);
	while ((msg = ddjvu_message_peek (ctx)) && (msg->m_any.tag != message)) {
		handle_message (msg, error);
		ddjvu_message_pop (ctx);
		if (error && *error)
			return;
	}
	if (msg && msg->m_any.tag == message)
		ddjvu_message_pop (ctx);
}

static gboolean
djvu_document_load (EvDocument  *document,
		    const char  *uri,
		    GError     **error)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	ddjvu_document_t *doc;
	gchar *filename;
	gboolean missing_files = FALSE;
	gboolean check_for_missing_files = FALSE;
	GError *djvu_error = NULL;
	gint n_files;
	gint i;
	gchar *base;

	/* FIXME: We could actually load uris  */
	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

#ifdef __APPLE__
	doc = ddjvu_document_create_by_filename_utf8 (djvu_document->d_context, filename, TRUE);
#else
	doc = ddjvu_document_create_by_filename (djvu_document->d_context, filename, TRUE);
#endif

	if (!doc) {
		g_free (filename);
    		g_set_error_literal (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     _("DjVu document has incorrect format"));
		return FALSE;
	}

	if (djvu_document->d_document)
	    ddjvu_document_release (djvu_document->d_document);

	djvu_document->d_document = doc;

	djvu_wait_for_message (djvu_document, DDJVU_DOCINFO, &djvu_error);
	if (djvu_error) {
		g_set_error_literal (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     djvu_error->message);
		g_error_free (djvu_error);
		g_free (filename);
		ddjvu_document_release (djvu_document->d_document);
		djvu_document->d_document = NULL;

		return FALSE;
	}

	if (ddjvu_document_decoding_error (djvu_document->d_document))
		djvu_handle_events (djvu_document, TRUE, &djvu_error);

	if (djvu_error) {
		g_set_error_literal (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     djvu_error->message);
		g_error_free (djvu_error);
		g_free (filename);
		ddjvu_document_release (djvu_document->d_document);
		djvu_document->d_document = NULL;

		return FALSE;
	}

	g_free (djvu_document->uri);
	djvu_document->uri = g_strdup (uri);

	djvu_document->n_pages = ddjvu_document_get_pagenum (djvu_document->d_document);

	if (djvu_document->n_pages > 0) {
		djvu_document->fileinfo_pages = g_new0 (ddjvu_fileinfo_t, djvu_document->n_pages);
		djvu_document->file_ids = g_hash_table_new (g_str_hash, g_str_equal);
	}
	if (ddjvu_document_get_type (djvu_document->d_document) == DDJVU_DOCTYPE_INDIRECT)
		check_for_missing_files = TRUE;

	base = g_path_get_dirname (filename);

	n_files = ddjvu_document_get_filenum (djvu_document->d_document);
	for (i = 0; i < n_files; i++) {
		ddjvu_fileinfo_t fileinfo;
		gchar *file;

		ddjvu_document_get_fileinfo (djvu_document->d_document,
					     i, &fileinfo);

		if (fileinfo.type != 'P')
			continue;

		if (fileinfo.pageno >= 0 && fileinfo.pageno < djvu_document->n_pages) {
			djvu_document->fileinfo_pages[fileinfo.pageno] = fileinfo;
		}

		g_hash_table_insert (djvu_document->file_ids,
				     (gpointer) djvu_document->fileinfo_pages[fileinfo.pageno].id,
				     GINT_TO_POINTER(fileinfo.pageno));

		if (check_for_missing_files && !missing_files) {
			file = g_build_filename (base, fileinfo.id, NULL);
			if (!g_file_test (file, G_FILE_TEST_EXISTS)) {
				missing_files = TRUE;
			}
			g_free (file);
		}
	}
	g_free (base);
	g_free (filename);

	if (missing_files) {
		g_set_error_literal (error,
                                     G_FILE_ERROR,
                                     G_FILE_ERROR_EXIST,
				     _("The document is composed of several files. "
                                       "One or more of these files cannot be accessed."));

		return FALSE;
	}

	return TRUE;
}


static gboolean
djvu_document_save (EvDocument  *document,
		    const char  *uri,
		    GError     **error)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);

	return ev_xfer_uri_simple (djvu_document->uri, uri, error);
}

int
djvu_document_get_n_pages (EvDocument  *document)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);

	g_return_val_if_fail (djvu_document->d_document, 0);

	return ddjvu_document_get_pagenum (djvu_document->d_document);
}

static void
document_get_page_size (DjvuDocument *djvu_document,
			gint          page,
			double       *width,
			double       *height,
			double	     *dpi)
{
	ddjvu_pageinfo_t info;
	ddjvu_status_t r;

	while ((r = ddjvu_document_get_pageinfo(djvu_document->d_document, page, &info)) < DDJVU_JOB_OK)
		djvu_handle_events(djvu_document, TRUE, NULL);

	if (r >= DDJVU_JOB_FAILED)
		djvu_handle_events(djvu_document, TRUE, NULL);

	if (width)
		*width = info.width * 72.0 / info.dpi;
	if (height)
		*height = info.height * 72.0 / info.dpi;
	if (dpi)
		*dpi = info.dpi;
}

static void
djvu_document_get_page_size (EvDocument   *document,
			     EvPage       *page,
			     double       *width,
			     double       *height)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);

	g_return_if_fail (djvu_document->d_document);

	document_get_page_size (djvu_document, page->index,
				width, height, NULL);
}

static cairo_surface_t *
djvu_document_render (EvDocument      *document,
		      EvRenderContext *rc)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	cairo_surface_t *surface;
	gchar *pixels;
	gint   rowstride;
    	ddjvu_rect_t rrect;
	ddjvu_rect_t prect;
	ddjvu_page_t *d_page;
	ddjvu_page_rotation_t rotation;
	gint buffer_modified;
	double page_width, page_height;
	gint transformed_width, transformed_height;

	d_page = ddjvu_page_create_by_pageno (djvu_document->d_document, rc->page->index);

	while (!ddjvu_page_decoding_done (d_page))
		djvu_handle_events(djvu_document, TRUE, NULL);

	document_get_page_size (djvu_document, rc->page->index, &page_width, &page_height, NULL);
	rotation = ddjvu_page_get_initial_rotation (d_page);

	ev_render_context_compute_transformed_size (rc, page_width, page_height,
						    &transformed_width, &transformed_height);

	/*
	 * Evince rotates documents in clockwise direction
	 * and djvulibre rotates documents in counter-clockwise direction.
	 */
	switch (rc->rotation) {
	        case 90:
			rotation += DDJVU_ROTATE_270;

			break;
	        case 180:
			rotation += DDJVU_ROTATE_180;

			break;
	        case 270:
			rotation += DDJVU_ROTATE_90;

			break;
	        default:
			rotation += DDJVU_ROTATE_0;
	}
	rotation = rotation % 4;

	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					      transformed_width, transformed_height);

	rowstride = cairo_image_surface_get_stride (surface);
	pixels = (gchar *)cairo_image_surface_get_data (surface);

	prect.x = 0;
	prect.y = 0;
	prect.w = transformed_width;
	prect.h = transformed_height;
	rrect = prect;

	ddjvu_page_set_rotation (d_page, rotation);

	buffer_modified = ddjvu_page_render (d_page, DDJVU_RENDER_COLOR,
					     &prect,
					     &rrect,
					     djvu_document->d_format,
					     rowstride,
					     pixels);

	if (!buffer_modified) {
		cairo_t *cr = cairo_create (surface);

		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_paint (cr);
		cairo_destroy (cr);
	} else {
		cairo_surface_mark_dirty (surface);
	}

	return surface;
}

static char *
djvu_document_get_page_label (EvDocument *document,
                              EvPage     *page)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	const gchar *title = NULL;
	gchar *label = NULL;

	g_assert (page->index >= 0 && page->index < djvu_document->n_pages);

	if (djvu_document->fileinfo_pages == NULL)
		return NULL;

	title = djvu_document->fileinfo_pages[page->index].title;

	if (!g_str_has_suffix (title, ".djvu"))
		label = g_strdup (title);

	return label;
}

static GdkPixbuf *
djvu_document_get_thumbnail (EvDocument      *document,
			     EvRenderContext *rc)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	GdkPixbuf *pixbuf, *rotated_pixbuf;
	gdouble page_width, page_height;
	gint thumb_width, thumb_height;
	guchar *pixels;

	g_return_val_if_fail (djvu_document->d_document, NULL);

	djvu_document_get_page_size (EV_DOCUMENT(djvu_document), rc->page,
				     &page_width, &page_height);

	ev_render_context_compute_scaled_size (rc, page_width, page_height,
					       &thumb_width, &thumb_height);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				 thumb_width, thumb_height);
	gdk_pixbuf_fill (pixbuf, 0xffffffff);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	while (ddjvu_thumbnail_status (djvu_document->d_document, rc->page->index, 1) < DDJVU_JOB_OK)
		djvu_handle_events(djvu_document, TRUE, NULL);

	ddjvu_thumbnail_render (djvu_document->d_document, rc->page->index,
				&thumb_width, &thumb_height,
				djvu_document->thumbs_format,
				gdk_pixbuf_get_rowstride (pixbuf),
				(gchar *)pixels);

	rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf, 360 - rc->rotation);
	g_object_unref (pixbuf);

	return rotated_pixbuf;
}

static cairo_surface_t *
djvu_document_get_thumbnail_surface (EvDocument      *document,
				     EvRenderContext *rc)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	cairo_surface_t *surface, *rotated_surface;
	gdouble page_width, page_height;
	gint thumb_width, thumb_height;
	gchar *pixels;
	gint thumbnail_rendered;

	g_return_val_if_fail (djvu_document->d_document, NULL);

	djvu_document_get_page_size (EV_DOCUMENT(djvu_document), rc->page,
				     &page_width, &page_height);

	ev_render_context_compute_scaled_size (rc, page_width, page_height,
					       &thumb_width, &thumb_height);

	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					      thumb_width, thumb_height);
	pixels = (gchar *)cairo_image_surface_get_data (surface);

	while (ddjvu_thumbnail_status (djvu_document->d_document, rc->page->index, 1) < DDJVU_JOB_OK)
		djvu_handle_events(djvu_document, TRUE, NULL);

	thumbnail_rendered = ddjvu_thumbnail_render (djvu_document->d_document,
						     rc->page->index,
						     &thumb_width, &thumb_height,
						     djvu_document->d_format,
						     cairo_image_surface_get_stride (surface),
						     pixels);

	if (!thumbnail_rendered) {
		cairo_surface_destroy (surface);
		surface = djvu_document_render (document, rc);
	} else {
		cairo_surface_mark_dirty (surface);
		rotated_surface = ev_document_misc_surface_rotate_and_scale (surface,
									     thumb_width,
									     thumb_height,
									     rc->rotation);
		cairo_surface_destroy (surface);
		surface = rotated_surface;
	}

	return surface;
}

static EvDocumentInfo *
djvu_document_get_info (EvDocument *document)
{
        DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
        const char *xmp;
        miniexp_t anno;
        EvDocumentInfo *info;

        info = ev_document_info_new ();

        anno = ddjvu_document_get_anno (djvu_document->d_document, 1);
        if (anno == miniexp_nil) {
                ddjvu_miniexp_release (djvu_document->d_document, anno);
                return info;
        }

        xmp = ddjvu_anno_get_xmp (anno);
        if (xmp != NULL) {
                ev_document_info_set_from_xmp (info, xmp, -1);
        }

        ddjvu_miniexp_release (djvu_document->d_document, anno);
        return info;
}

static void
djvu_document_finalize (GObject *object)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (object);

	if (djvu_document->d_document)
	    ddjvu_document_release (djvu_document->d_document);

	if (djvu_document->opts)
	    g_string_free (djvu_document->opts, TRUE);

	if (djvu_document->ps_filename)
	    g_free (djvu_document->ps_filename);

	if (djvu_document->fileinfo_pages)
	    g_free (djvu_document->fileinfo_pages);

	if (djvu_document->file_ids)
	    g_hash_table_destroy (djvu_document->file_ids);

	ddjvu_context_release (djvu_document->d_context);
	ddjvu_format_release (djvu_document->d_format);
	ddjvu_format_release (djvu_document->thumbs_format);
	g_free (djvu_document->uri);

	G_OBJECT_CLASS (djvu_document_parent_class)->finalize (object);
}

static void
djvu_document_class_init (DjvuDocumentClass *klass)
{
	GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
	EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

	gobject_class->finalize = djvu_document_finalize;

	ev_document_class->load = djvu_document_load;
	ev_document_class->save = djvu_document_save;
	ev_document_class->get_n_pages = djvu_document_get_n_pages;
	ev_document_class->get_page_label = djvu_document_get_page_label;
	ev_document_class->get_page_size = djvu_document_get_page_size;
	ev_document_class->render = djvu_document_render;
	ev_document_class->get_thumbnail = djvu_document_get_thumbnail;
	ev_document_class->get_thumbnail_surface = djvu_document_get_thumbnail_surface;
	ev_document_class->get_info = djvu_document_get_info;
}

static gchar *
djvu_text_copy (DjvuDocument *djvu_document,
		gint           page_num,
		EvRectangle  *rectangle)
{
	miniexp_t page_text;
	gchar    *text = NULL;

	while ((page_text =
		ddjvu_document_get_pagetext (djvu_document->d_document,
					     page_num, "char")) == miniexp_dummy)
		djvu_handle_events (djvu_document, TRUE, NULL);

	if (page_text != miniexp_nil) {
		DjvuTextPage *page = djvu_text_page_new (page_text);

		text = djvu_text_page_copy (page, rectangle);
		djvu_text_page_free (page);
		ddjvu_miniexp_release (djvu_document->d_document, page_text);
	}

	return text;
}

static void
djvu_convert_to_doc_rect (EvRectangle *dest,
			  EvRectangle *source,
			  gdouble height,
			  gdouble dpi)
{
	dest->x1 = source->x1 * dpi / 72;
	dest->x2 = source->x2 * dpi / 72;
	dest->y1 = (height - source->y2) * dpi / 72;
	dest->y2 = (height - source->y1) * dpi / 72;
}

static GList *
djvu_selection_get_selection_rects (DjvuDocument    *djvu_document,
				    gint             page,
				    EvRectangle     *points,
				    gdouble          height,
				    gdouble          dpi)
{
	miniexp_t   page_text;
	EvRectangle rectangle;
	GList      *rects = NULL;

	djvu_convert_to_doc_rect (&rectangle, points, height, dpi);

	while ((page_text = ddjvu_document_get_pagetext (djvu_document->d_document,
							 page, "char")) == miniexp_dummy)
		djvu_handle_events (djvu_document, TRUE, NULL);

	if (page_text != miniexp_nil) {
		DjvuTextPage *tpage = djvu_text_page_new (page_text);

		rects = djvu_text_page_get_selection_region (tpage, &rectangle);
		djvu_text_page_free (tpage);
		ddjvu_miniexp_release (djvu_document->d_document, page_text);
	}

	return rects;
}

static cairo_region_t *
djvu_get_selection_region (DjvuDocument *djvu_document,
                           gint page,
                           gdouble scale_x,
                           gdouble scale_y,
                           EvRectangle *points)
{
	double          height, dpi;
	GList          *rects = NULL, *l;
	cairo_region_t *region;

	document_get_page_size (djvu_document, page, NULL, &height, &dpi);
	rects = djvu_selection_get_selection_rects (djvu_document, page, points,
						    height, dpi);
	region = cairo_region_create ();
	for (l = rects; l && l->data; l = g_list_next (l)) {
		cairo_rectangle_int_t rect;
		EvRectangle          *r = (EvRectangle *)l->data;
		gdouble               tmp;

		tmp = r->y1;
		r->x1 *= 72 / dpi;
		r->x2 *= 72 / dpi;
		r->y1 = height - r->y2 * 72 / dpi;
		r->y2 = height - tmp * 72 / dpi;

		rect.x = (gint) ((r->x1 * scale_x) + 0.5);
		rect.y = (gint) ((r->y1 * scale_y) + 0.5);
		rect.width = (gint) ((r->x2 * scale_x) + 0.5) - rect.x;
		rect.height = (gint) ((r->y2 * scale_y) + 0.5) - rect.y;
		cairo_region_union_rectangle (region, &rect);
		ev_rectangle_free (r);
	}
	g_list_free (l);

	return region;
}

static cairo_region_t *
djvu_selection_get_selection_region (EvSelection    *selection,
				     EvRenderContext *rc,
				     EvSelectionStyle style,
				     EvRectangle     *points)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (selection);
	gdouble page_width, page_height;
	gdouble scale_x, scale_y;

	document_get_page_size (djvu_document, rc->page->index, &page_width, &page_height, NULL);
	ev_render_context_compute_scales (rc, page_width, page_height, &scale_x, &scale_y);

	return djvu_get_selection_region (djvu_document, rc->page->index,
					  scale_x, scale_y, points);
}

static gchar *
djvu_selection_get_selected_text (EvSelection     *selection,
				  EvPage          *page,
				  EvSelectionStyle style,
				  EvRectangle     *points)
{
      	DjvuDocument *djvu_document = DJVU_DOCUMENT (selection);
	double height, dpi;
      	EvRectangle rectangle;
      	gchar *text;

	document_get_page_size (djvu_document, page->index, NULL, &height, &dpi);
	djvu_convert_to_doc_rect (&rectangle, points, height, dpi);
      	text = djvu_text_copy (djvu_document, page->index, &rectangle);

      	if (text == NULL)
		text = g_strdup ("");

    	return text;
}

static void
djvu_selection_iface_init (EvSelectionInterface *iface)
{
	iface->get_selected_text = djvu_selection_get_selected_text;
	iface->get_selection_region = djvu_selection_get_selection_region;
}

static cairo_region_t *
djvu_document_text_get_text_mapping (EvDocumentText *document_text,
				     EvPage         *page)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document_text);
	EvRectangle points;

	points.x1 = 0;
	points.y1 = 0;

	document_get_page_size (djvu_document, page->index,
				&points.x2, &points.y2, NULL);

	return djvu_get_selection_region (djvu_document, page->index,
					  1.0, 1.0, &points);
}

static gchar *
djvu_document_text_get_text (EvDocumentText  *selection,
                             EvPage          *page)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (selection);
	miniexp_t     page_text;
	gchar        *text = NULL;

	while ((page_text = ddjvu_document_get_pagetext (djvu_document->d_document,
							 page->index,
							 "char")) == miniexp_dummy)
		djvu_handle_events (djvu_document, TRUE, NULL);

	if (page_text != miniexp_nil) {
		DjvuTextPage *tpage = djvu_text_page_new (page_text);

		djvu_text_page_index_text (tpage, TRUE);
		text = tpage->text;
		tpage->text = NULL;
		djvu_text_page_free (tpage);
		ddjvu_miniexp_release (djvu_document->d_document, page_text);
	}
	return text;
}

static void
djvu_document_text_iface_init (EvDocumentTextInterface *iface)
{
	iface->get_text_mapping = djvu_document_text_get_text_mapping;
	iface->get_text = djvu_document_text_get_text;
}

/* EvFileExporterIface */
static void
djvu_document_file_exporter_begin (EvFileExporter        *exporter,
				   EvFileExporterContext *fc)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (exporter);

	if (djvu_document->ps_filename)
		g_free (djvu_document->ps_filename);
	djvu_document->ps_filename = g_strdup (fc->filename);

	g_string_assign (djvu_document->opts, "-page=");
}

static void
djvu_document_file_exporter_do_page (EvFileExporter  *exporter,
				     EvRenderContext *rc)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (exporter);

	g_string_append_printf (djvu_document->opts, "%d,", (rc->page->index) + 1);
}

static void
djvu_document_file_exporter_end (EvFileExporter *exporter)
{
	int d_optc = 1;
	const char *d_optv[d_optc];
	ddjvu_job_t *job;

	DjvuDocument *djvu_document = DJVU_DOCUMENT (exporter);

	FILE *fn = fopen (djvu_document->ps_filename, "w");
	if (fn == NULL) {
		g_warning ("Cannot open file “%s”.", djvu_document->ps_filename);
		return;
	}

	d_optv[0] = djvu_document->opts->str;

	job = ddjvu_document_print(djvu_document->d_document, fn, d_optc, d_optv);
	while (!ddjvu_job_done(job)) {
		djvu_handle_events (djvu_document, TRUE, NULL);
	}

	fclose(fn);
}

static EvFileExporterCapabilities
djvu_document_file_exporter_get_capabilities (EvFileExporter *exporter)
{
	return  EV_FILE_EXPORTER_CAN_PAGE_SET |
		EV_FILE_EXPORTER_CAN_COPIES |
		EV_FILE_EXPORTER_CAN_COLLATE |
		EV_FILE_EXPORTER_CAN_REVERSE |
		EV_FILE_EXPORTER_CAN_GENERATE_PS;
}

static void
djvu_document_file_exporter_iface_init (EvFileExporterInterface *iface)
{
        iface->begin = djvu_document_file_exporter_begin;
        iface->do_page = djvu_document_file_exporter_do_page;
        iface->end = djvu_document_file_exporter_end;
	iface->get_capabilities = djvu_document_file_exporter_get_capabilities;
}

static void
djvu_document_init (DjvuDocument *djvu_document)
{
	guint masks[4] = { 0xff0000, 0xff00, 0xff, 0xff000000 };

	djvu_document->d_context = ddjvu_context_create ("Evince");
	djvu_document->d_format = ddjvu_format_create (DDJVU_FORMAT_RGBMASK32, 4, masks);
	ddjvu_format_set_row_order (djvu_document->d_format, 1);

	djvu_document->thumbs_format = ddjvu_format_create (DDJVU_FORMAT_RGB24, 0, 0);
	ddjvu_format_set_row_order (djvu_document->thumbs_format, 1);

	djvu_document->ps_filename = NULL;
	djvu_document->opts = g_string_new ("");

	djvu_document->d_document = NULL;
}

static GList *
djvu_document_find_find_text (EvDocumentFind *document,
			      EvPage *page,
			      const char *text,
			      EvFindOptions options)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	miniexp_t page_text;
	gdouble width, height, dpi;
	GList *matches = NULL, *l;
	char *search_text = NULL;
	gboolean case_sensitive = (options & EV_FIND_CASE_SENSITIVE);

	g_return_val_if_fail (text != NULL, NULL);

	while ((page_text = ddjvu_document_get_pagetext (djvu_document->d_document,
							 page->index,
							 "char")) == miniexp_dummy)
		djvu_handle_events (djvu_document, TRUE, NULL);

	if (page_text != miniexp_nil) {
		DjvuTextPage *tpage = djvu_text_page_new (page_text);

		djvu_text_page_index_text (tpage, case_sensitive);
		if (tpage->links->len > 0) {
			if (!case_sensitive) {
				search_text = g_utf8_casefold (text, -1);
				djvu_text_page_search (tpage, search_text);
				g_free (search_text);
			} else {
				djvu_text_page_search (tpage, text);
			}
			matches = tpage->results;
		}
		djvu_text_page_free (tpage);
		ddjvu_miniexp_release (djvu_document->d_document, page_text);
	}
	if (!matches)
		return NULL;

	document_get_page_size (djvu_document, page->index, &width, &height, &dpi);
	for (l = matches; l && l->data; l = g_list_next (l)) {
		EvRectangle *r = (EvRectangle *)l->data;
		gdouble tmp = r->y1;

		r->x1 *= 72.0 / dpi;
		r->x2 *= 72.0 / dpi;

		r->y1 = height - r->y2 * 72.0 / dpi;
		r->y2 = height - tmp * 72.0 / dpi;

		EvFindRectangle *ev_rect = ev_find_rectangle_new ();
		ev_rect->x1 = r->x1;
		ev_rect->x2 = r->x2;
		ev_rect->y1 = r->y1;
		ev_rect->y2 = r->y2;

		ev_rectangle_free (r);
		l->data = ev_rect;
	}


	return matches;
}

static EvFindOptions
djvu_document_find_get_supported_options (EvDocumentFind *document)
{
	return EV_FIND_CASE_SENSITIVE;
}

static void
djvu_document_find_iface_init (EvDocumentFindInterface *iface)
{
        iface->find_text = djvu_document_find_find_text;
	iface->get_supported_options = djvu_document_find_get_supported_options;
}

static EvMappingList *
djvu_document_links_get_links (EvDocumentLinks *document_links,
			       EvPage          *page)
{
	gdouble dpi;

	document_get_page_size (DJVU_DOCUMENT (document_links), page->index, NULL, NULL, &dpi);
	return djvu_links_get_links (document_links, page->index, 72.0 / dpi);
}

static void
djvu_document_document_links_iface_init  (EvDocumentLinksInterface *iface)
{
	iface->has_document_links = djvu_links_has_document_links;
	iface->get_links_model = djvu_links_get_links_model;
	iface->get_links = djvu_document_links_get_links;
	iface->find_link_dest = djvu_links_find_link_dest;
	iface->find_link_page = djvu_links_find_link_page;
}

GType
ev_backend_query_type (void)
{
	return DJVU_TYPE_DOCUMENT;
}
