
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* FIXME: Shoudl probably buffer calls to libtiff with TIFFSetWarningHandler
 */
#include "tiffio.h"
#include "tiff-document.h"
#include "ev-document-thumbnails.h"
#include "ev-document-misc.h"

struct _TiffDocumentClass
{
  GObjectClass parent_class;
};

struct _TiffDocument
{
  GObject parent_instance;

  TIFF *tiff;
  gint n_pages;
};

typedef struct _TiffDocumentClass TiffDocumentClass;

static void tiff_document_document_iface_init (EvDocumentIface *iface);
static void tiff_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);

G_DEFINE_TYPE_WITH_CODE (TiffDocument, tiff_document, G_TYPE_OBJECT,
                         { G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
						  tiff_document_document_iface_init);
			   G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
						  tiff_document_document_thumbnails_iface_init);
			 });

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

  push_handlers ();
  /* FIXME: We could actually load uris  */
  filename = g_filename_from_uri (uri, NULL, error);
  if (!filename)
    {
      pop_handlers ();
      return FALSE;
    }

  tiff = TIFFOpen (filename, "r");
  if (tiff)
    {
      guint32 w, h;
      
      TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
      TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);
    }
  if (!tiff)
    {
      pop_handlers ();
      return FALSE;
    }
  tiff_document->tiff = tiff;

  pop_handlers ();
  return TRUE;
}

static gboolean
tiff_document_save (EvDocument  *document,
		      const char  *uri,
		      GError     **error)
{
	return FALSE;
}

static int
tiff_document_get_n_pages (EvDocument  *document)
{
  TiffDocument *tiff_document = TIFF_DOCUMENT (document);

  g_return_val_if_fail (TIFF_IS_DOCUMENT (document), 0);
  g_return_val_if_fail (tiff_document->tiff != NULL, 0);

  if (tiff_document->n_pages == -1)
    {
      push_handlers ();
      tiff_document->n_pages = 0;
      do
	{
	  tiff_document->n_pages ++;
	}
      while (TIFFReadDirectory (tiff_document->tiff));
      pop_handlers ();
    }

  return tiff_document->n_pages;
}

static void
tiff_document_get_page_size (EvDocument   *document,
			     int           page,
			     double       *width,
			     double       *height)
{
  guint32 w, h;
  TiffDocument *tiff_document = TIFF_DOCUMENT (document);

  g_return_if_fail (TIFF_IS_DOCUMENT (document));
  g_return_if_fail (tiff_document->tiff != NULL);

  push_handlers ();
  if (TIFFSetDirectory (tiff_document->tiff, page) != 1)
    {
      pop_handlers ();
      return;
    }

  TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGELENGTH, &h);

  *width = w;
  *height = h;
  pop_handlers ();
}

static GdkPixbuf *
tiff_document_render_pixbuf (EvDocument  *document, int page, double scale)
{
  TiffDocument *tiff_document = TIFF_DOCUMENT (document);
  int width, height;
  gint rowstride, bytes;
  guchar *pixels = NULL;
  GdkPixbuf *pixbuf;
  GdkPixbuf *scaled_pixbuf;

  g_return_val_if_fail (TIFF_IS_DOCUMENT (document), 0);
  g_return_val_if_fail (tiff_document->tiff != NULL, 0);

  push_handlers ();
  if (TIFFSetDirectory (tiff_document->tiff, page) != 1)
    {
      pop_handlers ();
      return NULL;
    }

  if (!TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGEWIDTH, &width))
    {
      pop_handlers ();
      return NULL;
    }

  if (! TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGELENGTH, &height))
    {
      pop_handlers ();
      return NULL;
    }

  pop_handlers ();

  /* Sanity check the doc */
  if (width <= 0 || height <= 0)
    return NULL;                
        
  rowstride = width * 4;
  if (rowstride / 4 != width)
    /* overflow */
    return NULL;                
        
  bytes = height * rowstride;
  if (bytes / rowstride != height)
    /* overflow */
    return NULL;                

  pixels = g_try_malloc (bytes);
  if (!pixels)
    return NULL;

  pixbuf = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, TRUE, 8, 
				     width, height, rowstride,
				     (GdkPixbufDestroyNotify) g_free, NULL);

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
  TIFFReadRGBAImageOriented (tiff_document->tiff, width, height, (uint32 *)gdk_pixbuf_get_pixels (pixbuf), ORIENTATION_TOPLEFT, 1);
  pop_handlers ();

  scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
					   width * scale,
					   height * scale,
					   GDK_INTERP_BILINEAR);
  g_object_unref (pixbuf);

  return scaled_pixbuf;
}

static void
tiff_document_finalize (GObject *object)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (object);

	TIFFClose (tiff_document->tiff);

	G_OBJECT_CLASS (tiff_document_parent_class)->finalize (object);
}

static void
tiff_document_class_init (TiffDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = tiff_document_finalize;
}

static gboolean
tiff_document_can_get_text (EvDocument *document)
{
	return FALSE;
}

static EvDocumentInfo *
tiff_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;

	info = g_new0 (EvDocumentInfo, 1);
	info->fields_mask = 0;

	return info;
}

static void
tiff_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = tiff_document_load;
	iface->save = tiff_document_save;
	iface->can_get_text = tiff_document_can_get_text;
	iface->get_n_pages = tiff_document_get_n_pages;
	iface->get_page_size = tiff_document_get_page_size;
	iface->render_pixbuf = tiff_document_render_pixbuf;
	iface->get_info = tiff_document_get_info;
}

static GdkPixbuf *
tiff_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document,
					gint 		      page,
					gint                  size,
					gboolean              border)
{
  GdkPixbuf *pixbuf;
  gdouble w, h;

  tiff_document_get_page_size (EV_DOCUMENT (document),
			       page,
			       &w, &h);

  pixbuf = tiff_document_render_pixbuf (EV_DOCUMENT (document),
					page,
					size/w);

  if (border)
    {
      GdkPixbuf *tmp_pixbuf = pixbuf;
      pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, tmp_pixbuf);
      g_object_unref (tmp_pixbuf);
    }

  return pixbuf;
}

static void
tiff_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					 gint                  page,
					 gint                  suggested_width,
					 gint                 *width,
					 gint                 *height)
{
  gdouble page_ratio;
  gdouble w, h;

  tiff_document_get_page_size (EV_DOCUMENT (document),
			       page,
			       &w, &h);
  g_return_if_fail (w > 0);
  page_ratio = h/w;
  *width = suggested_width;
  *height = (gint) (suggested_width * page_ratio);
}

static void
tiff_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
  iface->get_thumbnail = tiff_document_thumbnails_get_thumbnail;
  iface->get_dimensions = tiff_document_thumbnails_get_dimensions;
}


static void
tiff_document_init (TiffDocument *tiff_document)
{
  tiff_document->n_pages = -1;
}
