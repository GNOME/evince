/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 * Copyright (C) 2005, Jonathan Blandford <jrb@gnome.org>
 * Copyright (C) 2005, Bastien Nocera <hadess@hadess.net>
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

#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "imposter.h"
#include "impress-document.h"

#include "ev-document-misc.h"
#include "ev-document-thumbnails.h"

struct _ImpressDocumentClass
{
  EvDocumentClass parent_class;
};

struct _ImpressDocument
{
  EvDocument parent_instance;

  ImpDoc *imp;
  ImpRenderCtx *ctx;

  GMutex *mutex;
  GdkPixmap *pixmap;
  GdkGC *gc;
  PangoContext *pango_ctx;

  /* Only used while rendering inside the mainloop */
  int pagenum;
  GdkPixbuf *pixbuf;
  GCond *cond;
};

#define PAGE_WIDTH 1024
#define PAGE_HEIGHT 768

typedef struct _ImpressDocumentClass ImpressDocumentClass;

static void impress_document_document_thumbnails_iface_init (EvDocumentThumbnailsInterface *iface);

EV_BACKEND_REGISTER_WITH_CODE (ImpressDocument, impress_document,
		         {
			   EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
							   impress_document_document_thumbnails_iface_init);
			 });

/* Renderer */
static void
imp_render_draw_bezier_real (GdkDrawable *d, GdkGC *gc, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
{
  int x, y, nx, ny;
  int ax, bx, cx, ay, by, cy;
  double t, t2, t3;

  x = x0;
  y = y0;

  cx = 3 * (x1 - x0);
  bx = 3 * (x2 - x1) - cx;
  ax = x3 - x0 - cx - bx;
  cy = 3 * (y1 - y0);
  by = 3 * (y2 - y1) - cy;
  ay = y3 - y0 - cy - by;

  for (t = 0; t < 1; t += 0.01) {
    t2 = t * t;
    t3 = t2 * t;
    nx = ax * t3 + bx * t2 + cx * t + x0;
    ny = ay * t3 + by * t2 + cy * t + y0;
    gdk_draw_line (d, gc, x, y, nx, ny);
    x = nx;
    y = ny;
  }
}

static void
imp_render_get_size(void *drw_data, int *w, int *h)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);

  gdk_drawable_get_size(impress_document->pixmap, w, h);
}

static void
imp_render_set_fg_color(void *drw_data, ImpColor *color)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);
  GdkColor c;

  c.red = color->red;
  c.green = color->green;
  c.blue = color->blue;
  gdk_gc_set_rgb_fg_color(impress_document->gc, &c);
}

static void
imp_render_draw_line(void *drw_data, int x1, int y1, int x2, int y2)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);

  gdk_draw_line(impress_document->pixmap, impress_document->gc, x1, y1, x2, y2);
}

static void
imp_render_draw_rect(void *drw_data, int fill, int x, int y, int w, int h)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);

  gdk_draw_rectangle(impress_document->pixmap, impress_document->gc, fill, x, y, w, h);
}

static void
imp_render_draw_polygon(void *drw_data, int fill, ImpPoint *pts, int nr_pts)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);

  gdk_draw_polygon(impress_document->pixmap, impress_document->gc, fill, (GdkPoint *)pts, nr_pts);
}

static void
imp_render_draw_arc(void *drw_data, int fill, int x, int y, int w, int h, int sa, int ea)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);

  gdk_draw_arc(impress_document->pixmap, impress_document->gc, fill, x, y, w, h, sa * 64, ea * 64);
}

static void
imp_render_draw_bezier(void *drw_data, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);

  imp_render_draw_bezier_real (impress_document->pixmap, impress_document->gc, x0, y0, x1, y1, x2, y2, x3, y3);
}

static void *
imp_render_open_image(void *drw_data, const unsigned char *pix, size_t size)
{
  GdkPixbufLoader *gpl;
  GdkPixbuf *pb;

  gpl = gdk_pixbuf_loader_new();
  gdk_pixbuf_loader_write(gpl, pix, size, NULL);
  gdk_pixbuf_loader_close(gpl, NULL);
  pb = gdk_pixbuf_loader_get_pixbuf(gpl);
  return pb;
}

static void
imp_render_get_image_size(void *drw_data, void *img_data, int *w, int *h)
{
  GdkPixbuf *pb = (GdkPixbuf *) img_data;

  *w = gdk_pixbuf_get_width(pb);
  *h = gdk_pixbuf_get_height(pb);
}

static void *
imp_render_scale_image(void *drw_data, void *img_data, int w, int h)
{
  GdkPixbuf *pb = (GdkPixbuf *) img_data;

  return gdk_pixbuf_scale_simple(pb, w, h, GDK_INTERP_BILINEAR);
}

static void
imp_render_draw_image(void *drw_data, void *img_data, int x, int y, int w, int h)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);
  GdkPixbuf *pb = (GdkPixbuf *) img_data;

  gdk_draw_pixbuf(impress_document->pixmap, impress_document->gc, pb, 0, 0, x, y, w, h, GDK_RGB_DITHER_NONE, 0, 0);
}

static void
imp_render_close_image(void *drw_data, void *img_data)
{
  GdkPixbuf *pb = (GdkPixbuf *) img_data;

  g_object_unref(G_OBJECT(pb));
}

static char *
imp_render_markup(const char *text, size_t len, int styles, int size)
{
  double scr_mm, scr_px, dpi;
  char *esc;
  char *ret;
  int sz;

  scr_mm = gdk_screen_get_height_mm(gdk_screen_get_default());
  scr_px = gdk_screen_get_height(gdk_screen_get_default());
  dpi = (scr_px / scr_mm) * 25.4; 
  sz = (int) ((double) size * 72.0 * PANGO_SCALE / dpi);
  esc = g_markup_escape_text(text, len);
  ret = g_strdup_printf("<span size ='%d'>%s</span>", sz, esc);
  g_free(esc);
  return ret;
}

static void
imp_render_get_text_size(void *drw_data, const char *text, size_t len, int size, int styles, int *w, int *h)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);
  PangoLayout *lay;
  int pw, ph;
  char *m;

  g_return_if_fail (impress_document->pango_ctx != NULL);

  lay = pango_layout_new(impress_document->pango_ctx);
  m = imp_render_markup(text, len, styles, size);
  pango_layout_set_markup(lay, m, strlen(m));
  pango_layout_get_size(lay, &pw, &ph);
  g_object_unref(lay);
  g_free(m);
  *w = pw / PANGO_SCALE;
  *h = ph / PANGO_SCALE;
}

static void
imp_render_draw_text(void *drw_data, int x, int y, const char *text, size_t len, int size, int styles)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (drw_data);
  PangoLayout *lay;
  char *m;

  g_return_if_fail (impress_document->pango_ctx != NULL);

  lay = pango_layout_new(impress_document->pango_ctx);
  m = imp_render_markup(text, len, styles, size);
  pango_layout_set_markup(lay, m, strlen(m));
  gdk_draw_layout(impress_document->pixmap, impress_document->gc, x, y, lay);
  g_object_unref(lay);
  g_free(m);
}

static const ImpDrawer imp_render_functions = {
  imp_render_get_size,
  imp_render_set_fg_color,
  imp_render_draw_line,
  imp_render_draw_rect,
  imp_render_draw_polygon,
  imp_render_draw_arc,
  imp_render_draw_bezier,
  imp_render_open_image,
  imp_render_get_image_size,
  imp_render_scale_image,
  imp_render_draw_image,
  imp_render_close_image,
  imp_render_get_text_size,
  imp_render_draw_text
};

/* Document interface */
static gboolean
impress_document_load (EvDocument  *document,
		    const char  *uri,
		    GError     **error)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (document);
  gchar *filename;
  ImpDoc *imp;
  int err;

  /* FIXME: Could we actually load uris ? */
  filename = g_filename_from_uri (uri, NULL, error);
  if (!filename)
    return FALSE;

  imp = imp_open (filename, &err);
  g_free (filename);

  if (!imp)
    {
      g_set_error_literal (error,
                           EV_DOCUMENT_ERROR,
                           EV_DOCUMENT_ERROR_INVALID,
                           _("Invalid document"));
      return FALSE;
    }
  impress_document->imp = imp;

  return TRUE;
}

static gboolean
impress_document_save (EvDocument  *document,
                       const char  *uri,
                       GError     **error)
{
        g_set_error_literal (error,
                             EV_DOCUMENT_ERROR,
                             EV_DOCUMENT_ERROR_INVALID,
                             "Not supported");
	return FALSE;
}

static int
impress_document_get_n_pages (EvDocument  *document)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (document);

  g_return_val_if_fail (IMPRESS_IS_DOCUMENT (document), 0);
  g_return_val_if_fail (impress_document->imp != NULL, 0);

  return imp_nr_pages (impress_document->imp);
}

static void
impress_document_get_page_size (EvDocument   *document,
				EvPage       *page,
				double       *width,
				double       *height)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (document);

  g_return_if_fail (IMPRESS_IS_DOCUMENT (document));
  g_return_if_fail (impress_document->imp != NULL);

  //FIXME
  *width = PAGE_WIDTH;
  *height = PAGE_HEIGHT;
}

static gboolean
imp_render_get_from_drawable (ImpressDocument *impress_document)
{
  ImpPage *page;

  page = imp_get_page (impress_document->imp, impress_document->pagenum);

  g_return_val_if_fail (page != NULL, FALSE);

  ev_document_doc_mutex_lock ();
  imp_context_set_page (impress_document->ctx, page);
  imp_render (impress_document->ctx, impress_document);
  ev_document_doc_mutex_unlock ();

  impress_document->pixbuf = gdk_pixbuf_get_from_drawable (NULL,
					 GDK_DRAWABLE (impress_document->pixmap),
					 NULL,
					 0, 0,
					 0, 0,
					 PAGE_WIDTH, PAGE_HEIGHT);

  g_cond_broadcast (impress_document->cond);
  return FALSE;
}

static GdkPixbuf *
impress_document_render_pixbuf (EvDocument      *document,
				EvRenderContext *rc)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (document);
  GdkPixbuf       *pixbuf;

  g_return_val_if_fail (IMPRESS_IS_DOCUMENT (document), NULL);
  g_return_val_if_fail (impress_document->imp != NULL, NULL);
  
  impress_document->pagenum = rc->page->index;

  g_mutex_lock (impress_document->mutex);
  impress_document->cond = g_cond_new ();

  ev_document_fc_mutex_unlock ();
  ev_document_doc_mutex_unlock ();
  g_idle_add ((GSourceFunc) imp_render_get_from_drawable, impress_document);
  g_cond_wait (impress_document->cond, impress_document->mutex);
  g_cond_free (impress_document->cond);
  ev_document_doc_mutex_lock ();
  ev_document_fc_mutex_lock ();
  
  g_mutex_unlock (impress_document->mutex);

  pixbuf = impress_document->pixbuf;
  impress_document->pixbuf = NULL;

  return pixbuf;
}

static cairo_surface_t *
impress_document_render (EvDocument      *document,
			 EvRenderContext *rc)
{
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface, *scaled_surface;

  pixbuf = impress_document_render_pixbuf (document, rc);
  
  /* FIXME: impress backend should be ported to cairo */
  surface = ev_document_misc_surface_from_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  scaled_surface = ev_document_misc_surface_rotate_and_scale (surface,
							      (PAGE_WIDTH * rc->scale) + 0.5,
							      (PAGE_HEIGHT * rc->scale) + 0.5,
							      rc->rotation);
  cairo_surface_destroy (surface);

  return scaled_surface;
}

static void
impress_document_finalize (GObject *object)
{
  ImpressDocument *impress_document = IMPRESS_DOCUMENT (object);

  if (impress_document->mutex)
    g_mutex_free (impress_document->mutex);

  if (impress_document->imp)
    imp_close (impress_document->imp);

  if (impress_document->ctx)
    imp_delete_context (impress_document->ctx);

  if (impress_document->pango_ctx)
    g_object_unref (impress_document->pango_ctx);

  if (impress_document->pixmap)
    g_object_unref (G_OBJECT (impress_document->pixmap));

  if (impress_document->gc)
    g_object_unref (impress_document->gc);

  G_OBJECT_CLASS (impress_document_parent_class)->finalize (object);
}

static void
impress_document_class_init (ImpressDocumentClass *klass)
{
  GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
  EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

  gobject_class->finalize = impress_document_finalize;

  ev_document_class->load = impress_document_load;
  ev_document_class->save = impress_document_save;
  ev_document_class->get_n_pages = impress_document_get_n_pages;
  ev_document_class->get_page_size = impress_document_get_page_size;
  ev_document_class->render = impress_document_render;
}

static GdkPixbuf *
impress_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document,
					   EvRenderContext      *rc, 
					   gboolean              border)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *scaled_pixbuf;

  pixbuf = impress_document_render_pixbuf (EV_DOCUMENT (document), rc);
  scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
					   (PAGE_WIDTH * rc->scale),
					   (PAGE_HEIGHT * rc->scale),
					   GDK_INTERP_BILINEAR);
  g_object_unref (pixbuf);

  if (border)
    {
      GdkPixbuf *tmp_pixbuf = scaled_pixbuf;
      
      scaled_pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, tmp_pixbuf);
      g_object_unref (tmp_pixbuf);
    }

  return scaled_pixbuf;
}

static void
impress_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					    EvRenderContext      *rc,
					    gint                 *width,
					    gint                 *height)
{
  gdouble page_width, page_height;

  impress_document_get_page_size (EV_DOCUMENT (document),
				  rc->page,
				  &page_width, &page_height);
  
  if (rc->rotation == 90 || rc->rotation == 270)
    {
      *width = (gint) (page_height * rc->scale);
      *height = (gint) (page_width * rc->scale);
    }
  else
    {
      *width = (gint) (page_width * rc->scale);
      *height = (gint) (page_height * rc->scale);
    }
}

static void
impress_document_document_thumbnails_iface_init (EvDocumentThumbnailsInterface *iface)
{
  iface->get_thumbnail = impress_document_thumbnails_get_thumbnail;
  iface->get_dimensions = impress_document_thumbnails_get_dimensions;
}

static void
impress_document_init (ImpressDocument *impress_document)
{
  GdkWindow *window;

  impress_document->mutex = g_mutex_new ();
  impress_document->ctx = imp_create_context(&imp_render_functions);

  window = gdk_screen_get_root_window (gdk_screen_get_default ());

  impress_document->pixmap = gdk_pixmap_new (window,
					     PAGE_WIDTH, PAGE_HEIGHT, -1);
  impress_document->gc = gdk_gc_new (impress_document->pixmap);
  impress_document->pango_ctx = gdk_pango_context_get_for_screen (gdk_screen_get_default ());
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
