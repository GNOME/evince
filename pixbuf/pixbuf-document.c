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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "pixbuf-document.h"
#include "ev-document-thumbnails.h"

struct _PixbufDocumentClass
{
	GObjectClass parent_class;
};

struct _PixbufDocument
{
	GObject parent_instance;

	GdkPixbuf *pixbuf;
	GdkDrawable *target;
	gdouble scale;

	gint x_offset, y_offset;
};

typedef struct _PixbufDocumentClass PixbufDocumentClass;

static void pixbuf_document_document_iface_init (EvDocumentIface *iface);
static void pixbuf_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);

G_DEFINE_TYPE_WITH_CODE (PixbufDocument, pixbuf_document, G_TYPE_OBJECT,
                         { G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
						  pixbuf_document_document_iface_init);
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
						pixbuf_document_document_thumbnails_iface_init)				   
				   });

static GObjectClass *parent_class;

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
	
	return TRUE;
}

static int
pixbuf_document_get_n_pages (EvDocument  *document)
{
	return 1;
}

static void
pixbuf_document_set_page (EvDocument  *document,
			  int          page)
{
	/* Do nothing */
}

static int
pixbuf_document_get_page (EvDocument  *document)
{
	return 1;
}

static void
pixbuf_document_set_target (EvDocument  *document,
			    GdkDrawable *target)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);

	pixbuf_document->target = target;
}

static void
pixbuf_document_set_scale (EvDocument  *document,
			   double       scale)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);

	pixbuf_document->scale = scale;
}

static void
pixbuf_document_set_page_offset (EvDocument  *document,
			      int          x,
			      int          y)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);

	pixbuf_document->x_offset = x;
	pixbuf_document->y_offset = y;
}

static void
pixbuf_document_get_page_size (EvDocument   *document,
			       int          *width,
			       int          *height)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);

	*width = gdk_pixbuf_get_width (pixbuf_document->pixbuf);
	*height = gdk_pixbuf_get_height (pixbuf_document->pixbuf);
}

static void
pixbuf_document_render (EvDocument  *document,
			int          clip_x,
			int          clip_y,
			int          clip_width,
			int          clip_height)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);

	if (pixbuf_document->scale == 1.0) {
 		gdk_draw_pixbuf (pixbuf_document->target, NULL, pixbuf_document->pixbuf,
				 clip_x, clip_y,
				 clip_x, clip_y,
				 clip_width, clip_height, GDK_RGB_DITHER_NORMAL,
				 0, 0);
	}
	else {
		GdkPixbuf *tmp_pixbuf;

		tmp_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf_document->pixbuf),
					     gdk_pixbuf_get_has_alpha (pixbuf_document->pixbuf),
					     gdk_pixbuf_get_bits_per_sample (pixbuf_document->pixbuf),
					     clip_width, clip_height);

		gdk_pixbuf_scale (pixbuf_document->pixbuf, tmp_pixbuf, 0, 0, clip_width, clip_height,
				  clip_x * pixbuf_document->scale,
				  clip_y * pixbuf_document->scale,
				  pixbuf_document->scale, pixbuf_document->scale,
				  GDK_INTERP_BILINEAR);
		
 		gdk_draw_pixbuf (pixbuf_document->target, NULL, tmp_pixbuf,
				 0, 0,
				 clip_x + pixbuf_document->x_offset,
				 clip_y + pixbuf_document->y_offset,
				 clip_width, clip_height, GDK_RGB_DITHER_NORMAL,
				 0, 0);

		g_object_unref (tmp_pixbuf);
	}
}

static void
pixbuf_document_begin_find (EvDocument   *document,
			    const char   *search_string,
			    gboolean      case_sensitive)
{
	
}

static void
pixbuf_document_end_find (EvDocument   *document)
{
}

static void
pixbuf_document_finalize (GObject *object)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (object);

	g_object_unref (pixbuf_document->pixbuf);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
pixbuf_document_class_init (PixbufDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	
	gobject_class->finalize = pixbuf_document_finalize;
}

static void
pixbuf_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = pixbuf_document_load;
	iface->get_n_pages = pixbuf_document_get_n_pages;
	iface->set_page = pixbuf_document_set_page;
	iface->get_page = pixbuf_document_get_page;
	iface->set_scale = pixbuf_document_set_scale;
	iface->set_target = pixbuf_document_set_target;
	iface->set_page_offset = pixbuf_document_set_page_offset;
	iface->get_page_size = pixbuf_document_get_page_size;
	iface->render = pixbuf_document_render;
        iface->begin_find = pixbuf_document_begin_find;
        iface->end_find = pixbuf_document_end_find;
}

static GdkPixbuf *
pixbuf_document_thumbnails_get_thumbnail (EvDocumentThumbnails   *document,
					  gint 			 page,
					  gint			 width)
{
	PixbufDocument *pixbuf_document = PIXBUF_DOCUMENT (document);
	GdkPixbuf *pixbuf;
	gdouble scale_factor;
	gint height;
	
	scale_factor = (gdouble)width / gdk_pixbuf_get_width (pixbuf_document->pixbuf);

	height = gdk_pixbuf_get_height (pixbuf_document->pixbuf) * scale_factor;
	
	pixbuf = gdk_pixbuf_scale_simple (pixbuf_document->pixbuf, width, height,
					  GDK_INTERP_BILINEAR);
	
	return pixbuf;
}


static void
pixbuf_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = pixbuf_document_thumbnails_get_thumbnail;
}


static void
pixbuf_document_init (PixbufDocument *pixbuf_document)
{
	pixbuf_document->scale = 1.0;

	pixbuf_document->x_offset = 10;
	pixbuf_document->y_offset = 10;
}
