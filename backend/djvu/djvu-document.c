/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <config.h>
#include "djvu-document.h"
#include "djvu-text.h"
#include "djvu-links.h"
#include "djvu-document-private.h"
#include "ev-document-thumbnails.h"
#include "ev-file-exporter.h"
#include "ev-document-misc.h"
#include "ev-document-find.h"
#include "ev-document-links.h"
#include "ev-selection.h"
#include "ev-file-helpers.h"

#include <gdk-pixbuf/gdk-pixbuf-core.h>
#include <glib/gi18n.h>
#include <glib/gunicode.h>
#include <string.h>

#define SCALE_FACTOR 0.2

enum {
	PROP_0,
	PROP_TITLE
};

struct _DjvuDocumentClass
{
	GObjectClass parent_class;
};

typedef struct _DjvuDocumentClass DjvuDocumentClass;

static void djvu_document_document_iface_init (EvDocumentIface *iface);
static void djvu_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);
static void djvu_document_file_exporter_iface_init (EvFileExporterIface *iface);
static void djvu_document_find_iface_init (EvDocumentFindIface *iface);
static void djvu_document_document_links_iface_init  (EvDocumentLinksIface *iface);
static void djvu_selection_iface_init (EvSelectionIface *iface);

EV_BACKEND_REGISTER_WITH_CODE (DjvuDocument, djvu_document,
    {
      EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS, djvu_document_document_thumbnails_iface_init);
      EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER, djvu_document_file_exporter_iface_init);
      EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND, djvu_document_find_iface_init);
      EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LINKS, djvu_document_document_links_iface_init);
      EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_SELECTION, djvu_selection_iface_init);
     });


void 
djvu_handle_events (DjvuDocument *djvu_document, int wait)
{
	ddjvu_context_t *ctx = djvu_document->d_context;
	const ddjvu_message_t *msg;
	
	if (!ctx)
		return;

	if (wait)
		msg = ddjvu_message_wait (ctx);

	while ((msg = ddjvu_message_peek (ctx))) {
		switch (msg->m_any.tag) {
			case DDJVU_ERROR:
				g_warning ("DjvuLibre error: %s", 
					   msg->m_error.message);
				if (msg->m_error.filename)
					g_warning ("DjvuLibre error: %s:%d", 
						   msg->m_error.filename,
						   msg->m_error.lineno);
				break;
			default:
				break;
		}
		ddjvu_message_pop (ctx);
	}
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

	/* FIXME: We could actually load uris  */
	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;
	
	doc = ddjvu_document_create_by_filename (djvu_document->d_context, filename, TRUE);

	if (!doc) {
		g_free (filename);
		return FALSE;
	}

	if (djvu_document->d_document)
	    ddjvu_document_release (djvu_document->d_document);

	djvu_document->d_document = doc;

	while (!ddjvu_document_decoding_done (djvu_document->d_document)) 
		djvu_handle_events (djvu_document, TRUE);
	g_free (djvu_document->uri);
	djvu_document->uri = g_strdup (uri);

	if (ddjvu_document_get_type (djvu_document->d_document) == DDJVU_DOCTYPE_INDIRECT) {
		gint n_files;
		gint i;
		gchar *base;

		base = g_path_get_dirname (filename);

		n_files = ddjvu_document_get_filenum (djvu_document->d_document);
		for (i = 0; i < n_files; i++) {
			struct ddjvu_fileinfo_s fileinfo;
			gchar *file;
			
			ddjvu_document_get_fileinfo (djvu_document->d_document,
						     i, &fileinfo);

			if (fileinfo.type != 'P')
				continue;

			file = g_build_filename (base, fileinfo.id, NULL);
			if (!g_file_test (file, G_FILE_TEST_EXISTS)) {
				missing_files = TRUE;
				g_free (file);
				
				break;
			}
			g_free (file);
		}
		g_free (base);
	}
	g_free (filename);

	if (missing_files) {
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_EXIST,
			     _("The document is composed by several files. "
			       "One or more of such files cannot be accessed."));

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
			double       *height)
{
	ddjvu_pageinfo_t info;
	ddjvu_status_t r;
	
	while ((r = ddjvu_document_get_pageinfo(djvu_document->d_document, page, &info)) < DDJVU_JOB_OK)
		djvu_handle_events(djvu_document, TRUE);
	
	if (r >= DDJVU_JOB_FAILED)
		djvu_handle_events(djvu_document, TRUE);

        *width = info.width * SCALE_FACTOR; 
        *height = info.height * SCALE_FACTOR;
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
				width, height);
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
	double page_width, page_height, tmp;
	static const cairo_user_data_key_t key;

	d_page = ddjvu_page_create_by_pageno (djvu_document->d_document, rc->page->index);
	
	while (!ddjvu_page_decoding_done (d_page))
		djvu_handle_events(djvu_document, TRUE);

	page_width = ddjvu_page_get_width (d_page) * rc->scale * SCALE_FACTOR + 0.5;
	page_height = ddjvu_page_get_height (d_page) * rc->scale * SCALE_FACTOR + 0.5;
	
	switch (rc->rotation) {
	        case 90:
			rotation = DDJVU_ROTATE_90;
			tmp = page_height;
			page_height = page_width;
			page_width = tmp;
			
			break;
	        case 180:
			rotation = DDJVU_ROTATE_180;
			
			break;
	        case 270:
			rotation = DDJVU_ROTATE_270;
			tmp = page_height;
			page_height = page_width;
			page_width = tmp;
			
			break;
	        default:
			rotation = DDJVU_ROTATE_0;
	}
#ifdef HAVE_CAIRO_FORMAT_STRIDE_FOR_WIDTH
	rowstride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, page_width);
#else
	rowstride = page_width * 4;
#endif
	pixels = (gchar *) g_malloc (page_height * rowstride);
	surface = cairo_image_surface_create_for_data ((guchar *)pixels,
						       CAIRO_FORMAT_RGB24,
						       page_width,
						       page_height,
						       rowstride);
	cairo_surface_set_user_data (surface, &key,
				     pixels, (cairo_destroy_func_t)g_free);
	prect.x = 0;
	prect.y = 0;
	prect.w = page_width;
	prect.h = page_height;
	rrect = prect;

	ddjvu_page_set_rotation (d_page, rotation);
	
	ddjvu_page_render (d_page, DDJVU_RENDER_COLOR,
			   &prect,
			   &rrect,
			   djvu_document->d_format,
			   rowstride,
			   pixels);

	return surface;
}

static void
djvu_document_finalize (GObject *object)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (object);

	if (djvu_document->search)
	    djvu_text_free (djvu_document->search);

	if (djvu_document->d_document)
	    ddjvu_document_release (djvu_document->d_document);
	    
	if (djvu_document->opts)
	    g_string_free (djvu_document->opts, TRUE);

	if (djvu_document->ps_filename)
	    g_free (djvu_document->ps_filename);
	    
	ddjvu_context_release (djvu_document->d_context);
	ddjvu_format_release (djvu_document->d_format);
	ddjvu_format_release (djvu_document->thumbs_format);
	g_free (djvu_document->uri);
	
	G_OBJECT_CLASS (djvu_document_parent_class)->finalize (object);
}

static void
djvu_document_class_init (DjvuDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = djvu_document_finalize;
}

static EvDocumentInfo *
djvu_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;

	info = g_new0 (EvDocumentInfo, 1);

	return info;
}

static void
djvu_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = djvu_document_load;
	iface->save = djvu_document_save;
	iface->get_n_pages = djvu_document_get_n_pages;
	iface->get_page_size = djvu_document_get_page_size;
	iface->render = djvu_document_render;
	iface->get_info = djvu_document_get_info;
}

static gchar *
djvu_selection_get_selected_text (EvSelection     *selection,
				  EvRenderContext *rc,
				  EvSelectionStyle style,
				  EvRectangle     *points)
{
      	DjvuDocument *djvu_document = DJVU_DOCUMENT (selection);
      	double width, height;
      	EvRectangle rectangle;
      	gchar *text;
	     
     	djvu_document_get_page_size (EV_DOCUMENT (djvu_document),
				     rc->page, &width, &height); 		
      	rectangle.x1 = points->x1 / SCALE_FACTOR;
	rectangle.y1 = (height - points->y2) / SCALE_FACTOR;
	rectangle.x2 = points->x2 / SCALE_FACTOR;
	rectangle.y2 = (height - points->y1) / SCALE_FACTOR;
		
      	text = djvu_text_copy (djvu_document, rc->page->index, &rectangle);
      
      	if (text == NULL)
		text = g_strdup ("");
		
    	return text;
}

static void
djvu_selection_iface_init (EvSelectionIface *iface)
{
	iface->get_selected_text = djvu_selection_get_selected_text;
}

static void
djvu_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					 EvRenderContext      *rc, 
					 gint                 *width,
					 gint                 *height)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document); 
	gdouble page_width, page_height;
	
	djvu_document_get_page_size (EV_DOCUMENT(djvu_document), rc->page,
				     &page_width, &page_height);

	if (rc->rotation == 90 || rc->rotation == 270) {
		*width = (gint) (page_height * rc->scale);
		*height = (gint) (page_width * rc->scale);
	} else {
		*width = (gint) (page_width * rc->scale);
		*height = (gint) (page_height * rc->scale);
	}
}

static GdkPixbuf *
djvu_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document,
					EvRenderContext      *rc,
					gboolean 	      border)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	GdkPixbuf *pixbuf, *rotated_pixbuf;
	gdouble page_width, page_height;
	gint thumb_width, thumb_height;
	guchar *pixels;
	
	g_return_val_if_fail (djvu_document->d_document, NULL);

	djvu_document_get_page_size (EV_DOCUMENT(djvu_document), rc->page,
				     &page_width, &page_height);
	
	thumb_width = (gint) (page_width * rc->scale);
	thumb_height = (gint) (page_height * rc->scale);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				 thumb_width, thumb_height);
	gdk_pixbuf_fill (pixbuf, 0xffffffff);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	
	while (ddjvu_thumbnail_status (djvu_document->d_document, rc->page->index, 1) < DDJVU_JOB_OK)
		djvu_handle_events(djvu_document, TRUE);
		    
	ddjvu_thumbnail_render (djvu_document->d_document, rc->page->index, 
				&thumb_width, &thumb_height,
				djvu_document->thumbs_format,
				gdk_pixbuf_get_rowstride (pixbuf), 
				(gchar *)pixels);

	rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf, 360 - rc->rotation);
	g_object_unref (pixbuf);

        if (border) {
	      GdkPixbuf *tmp_pixbuf = rotated_pixbuf;
	      
	      rotated_pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, tmp_pixbuf);
	      g_object_unref (tmp_pixbuf);
	}
	
	return rotated_pixbuf;
}

static void
djvu_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = djvu_document_thumbnails_get_thumbnail;
	iface->get_dimensions = djvu_document_thumbnails_get_dimensions;
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

	DjvuDocument *djvu_document = DJVU_DOCUMENT (exporter);

	FILE *fn = fopen (djvu_document->ps_filename, "w");
	if (fn == NULL) {
		g_warning ("Cannot open file “%s”.", djvu_document->ps_filename);
		return;
	}
	
	d_optv[0] = djvu_document->opts->str; 

	ddjvu_job_t * job = ddjvu_document_print(djvu_document->d_document, fn, d_optc, d_optv);
	while (!ddjvu_job_done(job)) {	
		djvu_handle_events (djvu_document, TRUE);
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
djvu_document_file_exporter_iface_init (EvFileExporterIface *iface)
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

static void
djvu_document_find_begin (EvDocumentFind   *document,
			  int               page,
                          const char       *search_string,
                          gboolean          case_sensitive)
{
        DjvuDocument *djvu_document = DJVU_DOCUMENT (document);

	if (djvu_document->search && 
	    strcmp (search_string, djvu_text_get_text (djvu_document->search)) == 0)
                return;

        if (djvu_document->search)
                djvu_text_free (djvu_document->search);

        djvu_document->search = djvu_text_new (djvu_document,
							  page,
							  case_sensitive,
							  search_string);
}

static int
djvu_document_find_get_n_results (EvDocumentFind *document_find, int page)
{
	DjvuText *search = DJVU_DOCUMENT (document_find)->search;

	if (search) {
		return djvu_text_n_results (search, page);
	} else {
		return 0;
	}
}

static gboolean
djvu_document_find_get_result (EvDocumentFind *document_find,
			       int             page,
			       int             n_result,
			       EvRectangle    *rectangle)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document_find);
	DjvuText *search = djvu_document->search;
	EvRectangle *r;
	double width, height;

	if (search == NULL)
		return FALSE;

	r = djvu_text_get_result (search, page, n_result);
	if (r == NULL)
		return FALSE;

	document_get_page_size (djvu_document, page, &width, &height);
	rectangle->x1 = r->x1 * SCALE_FACTOR;
	rectangle->y1 = height - r->y2 * SCALE_FACTOR;
	rectangle->x2 = r->x2 * SCALE_FACTOR;
	rectangle->y2 = height - r->y1 * SCALE_FACTOR;
		
	return TRUE;
}

static int
djvu_document_find_page_has_results (EvDocumentFind *document_find,
				    int             page)
{
	DjvuText *search = DJVU_DOCUMENT (document_find)->search;

	return search && djvu_text_has_results (search, page);
}

static double
djvu_document_find_get_progress (EvDocumentFind *document_find)
{
	DjvuText *search = DJVU_DOCUMENT (document_find)->search;
	
	if (search == NULL) {
		return 0;
	}

	return djvu_text_get_progress (search);
}

static void
djvu_document_find_cancel (EvDocumentFind *document)
{
        DjvuDocument *djvu_document = DJVU_DOCUMENT (document);

	if (djvu_document->search) {
		djvu_text_free (djvu_document->search);
		djvu_document->search = NULL;
	}
}

static void
djvu_document_find_iface_init (EvDocumentFindIface *iface)
{
        iface->begin = djvu_document_find_begin;
	iface->get_n_results = djvu_document_find_get_n_results;
	iface->get_result = djvu_document_find_get_result;
	iface->page_has_results = djvu_document_find_page_has_results;
	iface->get_progress = djvu_document_find_get_progress;
        iface->cancel = djvu_document_find_cancel;
}

static GList *
djvu_document_links_get_links (EvDocumentLinks *document_links,
			       gint             page)
{
	return djvu_links_get_links (document_links, page, SCALE_FACTOR);
}

static void
djvu_document_document_links_iface_init  (EvDocumentLinksIface *iface)
{
	iface->has_document_links = djvu_links_has_document_links;
	iface->get_links_model = djvu_links_get_links_model;
	iface->get_links = djvu_document_links_get_links;
	iface->find_link_dest = djvu_links_find_link_dest;
}
