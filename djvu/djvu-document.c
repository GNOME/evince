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

#include "djvu-document.h"
#include "ev-document-thumbnails.h"
#include "ev-document-misc.h"

#include <libdjvu/ddjvuapi.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf-core.h>

#define SCALE_FACTOR 0.2

enum {
	PROP_0,
	PROP_TITLE
};

struct _DjvuDocumentClass
{
	GObjectClass parent_class;
};

struct _DjvuDocument
{
	GObject parent_instance;

	ddjvu_context_t  *d_context;
	ddjvu_document_t *d_document;
	ddjvu_format_t   *d_format;
	
	gchar *uri;
};

typedef struct _DjvuDocumentClass DjvuDocumentClass;

static void djvu_document_document_iface_init (EvDocumentIface *iface);
static void djvu_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);

G_DEFINE_TYPE_WITH_CODE 
    (DjvuDocument, djvu_document, G_TYPE_OBJECT, 
    {
      G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT, djvu_document_document_iface_init);    
      G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS, djvu_document_document_thumbnails_iface_init)
     });

static gboolean
djvu_document_load (EvDocument  *document,
		      const char  *uri,
		      GError     **error)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	ddjvu_document_t *doc;
	gchar *filename;

	/* FIXME: We could actually load uris  */
	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;
	
	doc = ddjvu_document_create_by_filename (djvu_document->d_context, filename, TRUE);

	if (!doc) return FALSE;

	if (djvu_document->d_document)
	    ddjvu_document_release (djvu_document->d_document);

	djvu_document->d_document = doc;

	while (!ddjvu_document_decoding_done (djvu_document->d_document)) {
		    ddjvu_message_wait (djvu_document->d_context);
		    ddjvu_message_pop (djvu_document->d_context);	
	}
	g_free (djvu_document->uri);
	djvu_document->uri = g_strdup (uri);

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

static int
djvu_document_get_n_pages (EvDocument  *document)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	
	g_return_val_if_fail (djvu_document->d_document, 0);
	
	return ddjvu_document_get_pagenum (djvu_document->d_document);
}

static void
djvu_document_get_page_size (EvDocument   *document,
			       int           page,
			       double       *width,
			       double       *height)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
        ddjvu_pageinfo_t info;
	
	g_return_if_fail (djvu_document->d_document);
	
	while (ddjvu_document_get_pageinfo(djvu_document->d_document, page, &info) < DDJVU_JOB_OK) {
		    ddjvu_message_wait (djvu_document->d_context);
		    ddjvu_message_pop (djvu_document->d_context);	
	}

        *width = info.width * SCALE_FACTOR; 
        *height = info.height * SCALE_FACTOR;
}

static GdkPixbuf *
djvu_document_render_pixbuf (EvDocument  *document, 
			     EvRenderContext *rc)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	GdkPixbuf *pixbuf;
	GdkPixbuf *rotated_pixbuf;
	
    	ddjvu_rect_t rrect;
	ddjvu_rect_t prect;
	ddjvu_page_t *d_page;
	
	double page_width, page_height;

	d_page = ddjvu_page_create_by_pageno (djvu_document->d_document, rc->page);
	
	while (!ddjvu_page_decoding_done (d_page)) {
		    ddjvu_message_wait (djvu_document->d_context);
		    ddjvu_message_pop (djvu_document->d_context);	
	}
	
	page_width = ddjvu_page_get_width (d_page) * rc->scale * SCALE_FACTOR;
	page_height = ddjvu_page_get_height (d_page) * rc->scale * SCALE_FACTOR;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, page_width, page_height);

	prect.x = 0; prect.y = 0;
	prect.w = page_width; prect.h = page_height;
	rrect = prect;

	ddjvu_page_render(d_page, DDJVU_RENDER_COLOR,
                          &prect,
                          &rrect,
                          djvu_document->d_format,
                	  gdk_pixbuf_get_rowstride (pixbuf),
                          (gchar *)gdk_pixbuf_get_pixels (pixbuf));
	
	rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf, 360 - rc->rotation);
	g_object_unref (pixbuf);
	
	return rotated_pixbuf;
}

static void
djvu_document_finalize (GObject *object)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (object);

	if (djvu_document->d_document)
	    ddjvu_document_release (djvu_document->d_document);

	ddjvu_context_release (djvu_document->d_context);
	ddjvu_format_release (djvu_document->d_format);
	g_free (djvu_document->uri);
	
	G_OBJECT_CLASS (djvu_document_parent_class)->finalize (object);
}

static void
djvu_document_class_init (DjvuDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = djvu_document_finalize;
}

static gboolean
djvu_document_can_get_text (EvDocument *document)
{
	return FALSE;
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
	iface->can_get_text = djvu_document_can_get_text;
	iface->get_n_pages = djvu_document_get_n_pages;
	iface->get_page_size = djvu_document_get_page_size;
	iface->render_pixbuf = djvu_document_render_pixbuf;
	iface->get_info = djvu_document_get_info;
}

static void
djvu_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					   gint                  page,
					   gint                  suggested_width,
					   gint                  *width,
					   gint                  *height)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document); 
	gdouble p_width, p_height;
	gdouble page_ratio;
	
	djvu_document_get_page_size (EV_DOCUMENT(djvu_document), page, &p_width, &p_height);

	page_ratio = p_height / p_width;
	*width = suggested_width;
	*height = (gint) (suggested_width * page_ratio);
	
	return;
}

static GdkPixbuf *
djvu_document_thumbnails_get_thumbnail (EvDocumentThumbnails   *document,
					  gint 			 page,
				  	  gint	                 rotation,
					  gint			 width,
					  gboolean 		 border)
{
	DjvuDocument *djvu_document = DJVU_DOCUMENT (document);
	GdkPixbuf *pixbuf, *rotated_pixbuf;
	gint thumb_width, thumb_height;

	guchar *pixels;
	
	g_return_val_if_fail (djvu_document->d_document, NULL);
	
	djvu_document_thumbnails_get_dimensions (document, page, width, &thumb_width, &thumb_height);
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				 thumb_width, thumb_height);
	gdk_pixbuf_fill (pixbuf, 0xffffffff);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	
	while (ddjvu_thumbnail_status (djvu_document->d_document, page, 1) < DDJVU_JOB_OK) {
		    ddjvu_message_wait (djvu_document->d_context);
		    ddjvu_message_pop (djvu_document->d_context);	
	}
		    
	ddjvu_thumbnail_render (djvu_document->d_document, page, 
				&thumb_width, &thumb_height,
				djvu_document->d_format,
				gdk_pixbuf_get_rowstride (pixbuf), 
				(gchar *)pixels);

	rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf, 360 - rotation);
	g_object_unref (pixbuf);

        if (border) {
	      GdkPixbuf *tmp_pixbuf = rotated_pixbuf;
	      rotated_pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, 0, tmp_pixbuf);
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

static void
djvu_document_init (DjvuDocument *djvu_document)
{
	djvu_document->d_context = ddjvu_context_create ("Evince");
	djvu_document->d_format = ddjvu_format_create (DDJVU_FORMAT_RGB24, 0, 0);
	ddjvu_format_set_row_order (djvu_document->d_format,1);
	
	djvu_document->d_document = NULL;
}

