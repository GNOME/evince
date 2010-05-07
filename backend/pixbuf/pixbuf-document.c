/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 * Copyright (C) 2004, Anders Carlsson <andersca@gnome.org>
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

#include <config.h>
#include <glib/gi18n-lib.h>

#include "pixbuf-document.h"
#include "ev-document-thumbnails.h"
#include "ev-document-misc.h"
#include "ev-file-helpers.h"

struct _PixbufDocumentClass
{
	EvDocumentClass parent_class;
};

struct _PixbufDocument
{
	EvDocument parent_instance;

	GdkPixbuf *pixbuf;
	
	gchar *uri;
};

typedef struct _PixbufDocumentClass PixbufDocumentClass;

static void pixbuf_document_document_thumbnails_iface_init (EvDocumentThumbnailsInterface *iface);

EV_BACKEND_REGISTER_WITH_CODE (PixbufDocument, pixbuf_document,
                   {
			 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
							 pixbuf_document_document_thumbnails_iface_init)				   
		   });

static gboolean
pixbuf_document_load (EvDocument  *document,
		      const char  *uri,
		      GError     **error)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);
	
	gchar *filename;
	GdkPixbuf *pixbuf;

	/* FIXME: We could actually load uris  */
	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;
	
	pixbuf = gdk_pixbuf_new_from_file (filename, error);

	if (!pixbuf)
		return FALSE;

	pixbuf_document->pixbuf = pixbuf;
	g_free (pixbuf_document->uri);
	pixbuf_document->uri = g_strdup (uri);
	
	return TRUE;
}

static gboolean
pixbuf_document_save (EvDocument  *document,
		      const char  *uri,
		      GError     **error)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);

	return ev_xfer_uri_simple (pixbuf_document->uri, uri, error); 
}

static int
pixbuf_document_get_n_pages (EvDocument  *document)
{
	return 1;
}

static void
pixbuf_document_get_page_size (EvDocument   *document,
			       EvPage       *page,
			       double       *width,
			       double       *height)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);

	*width = gdk_pixbuf_get_width (pixbuf_document->pixbuf);
	*height = gdk_pixbuf_get_height (pixbuf_document->pixbuf);
}

static cairo_surface_t *
pixbuf_document_render (EvDocument      *document,
			EvRenderContext *rc)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);
	GdkPixbuf *scaled_pixbuf, *rotated_pixbuf;
	cairo_surface_t *surface;

	scaled_pixbuf = gdk_pixbuf_scale_simple (
		pixbuf_document->pixbuf,
		(gdk_pixbuf_get_width (pixbuf_document->pixbuf) * rc->scale) + 0.5,
		(gdk_pixbuf_get_height (pixbuf_document->pixbuf) * rc->scale) + 0.5,
		GDK_INTERP_BILINEAR);
	
        rotated_pixbuf = gdk_pixbuf_rotate_simple (scaled_pixbuf, 360 - rc->rotation);
        g_object_unref (scaled_pixbuf);

	surface = ev_document_misc_surface_from_pixbuf (rotated_pixbuf);
	g_object_unref (rotated_pixbuf);

	return surface;
}

static void
pixbuf_document_finalize (GObject *object)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (object);

	g_object_unref (pixbuf_document->pixbuf);
	g_free (pixbuf_document->uri);
	
	G_OBJECT_CLASS (pixbuf_document_parent_class)->finalize (object);
}

static void
pixbuf_document_class_init (PixbufDocumentClass *klass)
{
	GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
	EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

	gobject_class->finalize = pixbuf_document_finalize;

	ev_document_class->load = pixbuf_document_load;
	ev_document_class->save = pixbuf_document_save;
	ev_document_class->get_n_pages = pixbuf_document_get_n_pages;
	ev_document_class->get_page_size = pixbuf_document_get_page_size;
	ev_document_class->render = pixbuf_document_render;
}

static GdkPixbuf *
pixbuf_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document,
					  EvRenderContext      *rc,
					  gboolean              border)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);
	GdkPixbuf *pixbuf, *rotated_pixbuf;
	gint width, height;
	
	width = (gint) (gdk_pixbuf_get_width (pixbuf_document->pixbuf) * rc->scale);
	height = (gint) (gdk_pixbuf_get_height (pixbuf_document->pixbuf) * rc->scale);
	
	pixbuf = gdk_pixbuf_scale_simple (pixbuf_document->pixbuf,
					  width, height,
					  GDK_INTERP_BILINEAR);

	rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf, 360 - rc->rotation);
        g_object_unref (pixbuf);

        return rotated_pixbuf;
}

static void
pixbuf_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					   EvRenderContext      *rc, 
					   gint                 *width,
					   gint                 *height)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);
	gint p_width = gdk_pixbuf_get_width (pixbuf_document->pixbuf);
	gint p_height = gdk_pixbuf_get_height (pixbuf_document->pixbuf);

	if (rc->rotation == 90 || rc->rotation == 270) {
		*width = (gint) (p_height * rc->scale);
		*height = (gint) (p_width * rc->scale);
	} else {
		*width = (gint) (p_width * rc->scale);
		*height = (gint) (p_height * rc->scale);
	}
}

static void
pixbuf_document_document_thumbnails_iface_init (EvDocumentThumbnailsInterface *iface)
{
	iface->get_thumbnail = pixbuf_document_thumbnails_get_thumbnail;
	iface->get_dimensions = pixbuf_document_thumbnails_get_dimensions;
}


static void
pixbuf_document_init (PixbufDocument *pixbuf_document)
{
}
