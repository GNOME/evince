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

/* FIXME: Should probably buffer calls to libtiff with TIFFSetWarningHandler
 */

#include <stdio.h>
#include <glib.h>

#include "tiffio.h"
#include "tiff2ps.h"
#include "tiff-document.h"
#include "ev-document-misc.h"
#include "ev-document-thumbnails.h"
#include "ev-ps-exporter.h"

#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-xfer.h>

struct _TiffDocumentClass
{
  GObjectClass parent_class;
};

struct _TiffDocument
{
  GObject parent_instance;

  TIFF *tiff;
  gint n_pages;
  TIFF2PSContext *ps_export_ctx;
  
  gchar *uri;
};

typedef struct _TiffDocumentClass TiffDocumentClass;

static void tiff_document_document_iface_init (EvDocumentIface *iface);
static void tiff_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);
static void tiff_document_document_ps_exporter_iface_init (EvPSExporterIface *iface);

G_DEFINE_TYPE_WITH_CODE (TiffDocument, tiff_document, G_TYPE_OBJECT,
                         { G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
						  tiff_document_document_iface_init);
			   G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
						  tiff_document_document_thumbnails_iface_init);
			   G_IMPLEMENT_INTERFACE (EV_TYPE_PS_EXPORTER,
						  tiff_document_document_ps_exporter_iface_init);
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
      /* FIXME: unused data? why bother here */
      TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
      TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);
    }
  if (!tiff)
    {
      pop_handlers ();
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
	GnomeVFSResult result;
	GnomeVFSURI *source_uri;
	GnomeVFSURI *target_uri;
	
	if (!tiff_document->uri)
		return FALSE;
	
	source_uri = gnome_vfs_uri_new (tiff_document->uri);
	target_uri = gnome_vfs_uri_new (uri);

	result = gnome_vfs_xfer_uri (source_uri, target_uri, 
				     GNOME_VFS_XFER_DEFAULT | GNOME_VFS_XFER_FOLLOW_LINKS,
				     GNOME_VFS_XFER_ERROR_MODE_ABORT,
				     GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				     NULL,
				     NULL);
	gnome_vfs_uri_unref (target_uri);
	gnome_vfs_uri_unref (source_uri);
    
	if (result != GNOME_VFS_OK)
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     0,
			     gnome_vfs_result_to_string (result));			
	return (result == GNOME_VFS_OK);
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
  gfloat x_res, y_res;
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
  TIFFGetField (tiff_document->tiff, TIFFTAG_XRESOLUTION, &x_res);
  TIFFGetField (tiff_document->tiff, TIFFTAG_YRESOLUTION, &y_res);
  h = h * (x_res / y_res);

  *width = w;
  *height = h;

  pop_handlers ();
}

static GdkPixbuf *
tiff_document_render_pixbuf (EvDocument      *document,
			     EvRenderContext *rc)
{
  TiffDocument *tiff_document = TIFF_DOCUMENT (document);
  int width, height;
  float x_res, y_res;
  gint rowstride, bytes;
  guchar *pixels = NULL;
  GdkPixbuf *pixbuf;
  GdkPixbuf *scaled_pixbuf;
  GdkPixbuf *rotated_pixbuf;

  g_return_val_if_fail (TIFF_IS_DOCUMENT (document), 0);
  g_return_val_if_fail (tiff_document->tiff != NULL, 0);

  push_handlers ();
  if (TIFFSetDirectory (tiff_document->tiff, rc->page) != 1)
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

  if (!TIFFGetField (tiff_document->tiff, TIFFTAG_XRESOLUTION, &x_res))
    {
      pop_handlers ();
      return NULL;
    }

  if (! TIFFGetField (tiff_document->tiff, TIFFTAG_YRESOLUTION, &y_res))
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
					   width * rc->scale,
					   height * rc->scale * (x_res/y_res),
					   GDK_INTERP_BILINEAR);
  g_object_unref (pixbuf);

  rotated_pixbuf = gdk_pixbuf_rotate_simple (scaled_pixbuf, 360 - rc->rotation);
  g_object_unref (scaled_pixbuf);

  return rotated_pixbuf;
}

static void
tiff_document_finalize (GObject *object)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (object);

	TIFFClose (tiff_document->tiff);
	g_free (tiff_document->uri);

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
					gint	              rotation,
					gint                  size,
					gboolean              border)
{
  EvRenderContext *rc;
  GdkPixbuf *pixbuf;
  gdouble w, h;

  tiff_document_get_page_size (EV_DOCUMENT (document),
			       page,
			       &w, &h);

  rc = ev_render_context_new (rotation, page, size/w);
  pixbuf = tiff_document_render_pixbuf (EV_DOCUMENT (document), rc);
  g_object_unref (G_OBJECT (rc));

  if (border)
    {
      GdkPixbuf *tmp_pixbuf = pixbuf;
      pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, 0, tmp_pixbuf);
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

/* postscript exporter implementation */

static void
tiff_document_ps_export_begin (EvPSExporter *exporter, const char *filename,
			       int first_page, int last_page,
			       double width, double height, gboolean duplex)
{
	TiffDocument *document = TIFF_DOCUMENT (exporter);

	document->ps_export_ctx = tiff2ps_context_new(filename);
}

static void
tiff_document_ps_export_do_page (EvPSExporter *exporter, EvRenderContext *rc)
{
	TiffDocument *document = TIFF_DOCUMENT (exporter);

	if (document->ps_export_ctx == NULL)
		return;
	if (TIFFSetDirectory (document->tiff, rc->page) != 1)
		return;
	tiff2ps_process_page (document->ps_export_ctx, document->tiff,
			      0, 0, 0, 0, 0);
}

static void
tiff_document_ps_export_end (EvPSExporter *exporter)
{
	TiffDocument *document = TIFF_DOCUMENT (exporter);

	if (document->ps_export_ctx == NULL)
		return;
	tiff2ps_context_finalize(document->ps_export_ctx);
}

static void
tiff_document_document_ps_exporter_iface_init (EvPSExporterIface *iface)
{
	iface->begin = tiff_document_ps_export_begin;
	iface->do_page = tiff_document_ps_export_do_page;
	iface->end = tiff_document_ps_export_end;
}

static void
tiff_document_init (TiffDocument *tiff_document)
{
  tiff_document->n_pages = -1;
}
