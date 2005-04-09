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

#include <config.h>

#include "dvi-document.h"
#include "ev-document-thumbnails.h"
#include "ev-document-misc.h"

#include "mdvi.h"
#include "fonts.h"
#include "pixbuf-device.h"

#include <gtk/gtk.h>

GMutex *dvi_context_mutex = NULL;

enum {
	PROP_0,
	PROP_TITLE
};

struct _DviDocumentClass
{
	GObjectClass parent_class;
};

struct _DviDocument
{
	GObject parent_instance;

	DviContext *context;
	DviPageSpec *spec;
	DviParams *params;
	
	/* To let document scale we should remember width and height */
	
	double base_width;
	double base_height;
};

typedef struct _DviDocumentClass DviDocumentClass;

static void dvi_document_document_iface_init (EvDocumentIface *iface);
static void dvi_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);
static void dvi_document_get_page_size 			(EvDocument   *document,
					    		 int       page,
							 double    *width,
							 double    *height);

G_DEFINE_TYPE_WITH_CODE 
    (DviDocument, dvi_document, G_TYPE_OBJECT, 
    {
      G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT, dvi_document_document_iface_init);    
      G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS, dvi_document_document_thumbnails_iface_init)
     });

static gboolean
dvi_document_load (EvDocument  *document,
		      const char  *uri,
		      GError     **error)
{
    gchar *filename;
    DviDocument *dvi_document = DVI_DOCUMENT(document);
    
    filename = g_filename_from_uri (uri, NULL, error);
    
    if (!filename)
    	return FALSE;
	
    if (dvi_document->context)
	mdvi_destroy_context (dvi_document->context);

    dvi_document->context = mdvi_init_context(dvi_document->params, dvi_document->spec, filename);

    mdvi_pixbuf_device_init (&dvi_document->context->device);

    dvi_document->base_width = dvi_document->context->dvi_page_w * dvi_document->context->params.conv 
		+ 2 * unit2pix(dvi_document->params->dpi, MDVI_VMARGIN) / dvi_document->params->hshrink;
		
    dvi_document->base_height = dvi_document->context->dvi_page_h * dvi_document->context->params.vconv 
	        + 2 * unit2pix(dvi_document->params->dpi, MDVI_VMARGIN) / dvi_document->params->vshrink;

    dvi_context_mutex = g_mutex_new ();


    return TRUE;
}


static gboolean
dvi_document_save (EvDocument  *document,
		      const char  *uri,
		      GError     **error)
{
	g_warning ("dvi_document_save not implemented"); /* FIXME */
	return TRUE;
}

static int
dvi_document_get_n_pages (EvDocument  *document)
{
    DviDocument *dvi_document = DVI_DOCUMENT (document);
    return dvi_document->context->npages;
}

static void
dvi_document_get_page_size (EvDocument   *document,
			    int       page,
			    double    *width,
			    double    *height)
{
	DviDocument * dvi_document = DVI_DOCUMENT (document);	
	
	if (width != NULL)
    	    *width = dvi_document->base_width;

	if (height != NULL)
	    *height = dvi_document->base_height;
			    
	return;
}

static GdkPixbuf *
dvi_document_render_pixbuf (EvDocument  *document, int page, double scale)
{
	GdkPixbuf *pixbuf;

	DviDocument *dvi_document = DVI_DOCUMENT(document);

	gint required_width, required_height;
	gint proposed_width, proposed_height;
	gint xmargin = 0, ymargin = 0;

	/* We should protect our context since it's not 
	 * thread safe. The work to the future - 
	 * let context render page independently
	 */
	g_mutex_lock (dvi_context_mutex);
	
	mdvi_setpage(dvi_document->context,  page);
	
	mdvi_set_shrink (dvi_document->context, 
			 (int)((dvi_document->params->hshrink - 1) / scale) + 1,
			 (int)((dvi_document->params->vshrink - 1) / scale) + 1);

	required_width = dvi_document->base_width * scale;
	required_height = dvi_document->base_height * scale;
	proposed_width = dvi_document->context->dvi_page_w * dvi_document->context->params.conv;
	proposed_height = dvi_document->context->dvi_page_h * dvi_document->context->params.vconv;
	
	if (required_width >= proposed_width)
	    xmargin = (required_width - proposed_width) / 2;
	if (required_height >= proposed_height)
	    ymargin = (required_height - proposed_height) / 2;
	    
        mdvi_pixbuf_device_set_margins (&dvi_document->context->device, xmargin, ymargin);

        mdvi_pixbuf_device_render (dvi_document->context);
	
	pixbuf = mdvi_pixbuf_device_get_pixbuf (&dvi_document->context->device);

	g_mutex_unlock (dvi_context_mutex);

	return pixbuf;
}

static void
dvi_document_finalize (GObject *object)
{	
	DviDocument *dvi_document = DVI_DOCUMENT(object);

	if (dvi_document->context)
	    {
		mdvi_pixbuf_device_free (&dvi_document->context->device);
		mdvi_destroy_context (dvi_document->context);
	    }

	if (dvi_document->params)
		g_free (dvi_document->params);
		
	G_OBJECT_CLASS (dvi_document_parent_class)->finalize (object);
}

static void
dvi_document_set_property (GObject *object,
		              guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	switch (prop_id)
	{
		case PROP_TITLE:
			/* read only */
			break;
	}
}

static void
dvi_document_get_property (GObject *object,
		              guint prop_id,
		              GValue *value,
		              GParamSpec *pspec)
{
	switch (prop_id)
	{
		case PROP_TITLE:
			g_value_set_string (value, NULL);
			break;
	}
}

static void
dvi_document_class_init (DviDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = dvi_document_finalize;
	gobject_class->get_property = dvi_document_get_property;
	gobject_class->set_property = dvi_document_set_property;

	g_object_class_override_property (gobject_class, PROP_TITLE, "title");
}

static char *
dvi_document_get_text (EvDocument *document, gint page, EvRectangle *rect)
{
	/* FIXME this method should not be in EvDocument */
	g_warning ("dvi_document_get_text not implemented");
	return NULL;
}

static void
dvi_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = dvi_document_load;
	iface->save = dvi_document_save;
	iface->get_text = dvi_document_get_text;
	iface->get_n_pages = dvi_document_get_n_pages;
	iface->get_page_size = dvi_document_get_page_size;
	iface->render_pixbuf = dvi_document_render_pixbuf;
}

static void
dvi_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					   gint                  page,
					   gint                  suggested_width,
					   gint                  *width,
					   gint                  *height)
{	
	DviDocument *dvi_document = DVI_DOCUMENT (document); 
	gdouble page_ratio;
	
	page_ratio = dvi_document->base_height / dvi_document->base_width;
	*width = suggested_width;
	*height = (gint) (suggested_width * page_ratio);

	return;
}

static GdkPixbuf *
dvi_document_thumbnails_get_thumbnail (EvDocumentThumbnails   *document,
				       gint 			 page,
				       gint			 width,
				       gboolean 		 border)
{
	DviDocument *dvi_document = DVI_DOCUMENT (document);
	GdkPixbuf *pixbuf;
	GdkPixbuf *border_pixbuf;
	gint thumb_width, thumb_height;
	gint proposed_width, proposed_height;
	
	dvi_document_thumbnails_get_dimensions (document, page, width, &thumb_width, &thumb_height);

	g_mutex_lock (dvi_context_mutex);

	mdvi_setpage(dvi_document->context,  page);

	mdvi_set_shrink (dvi_document->context, 
			  (int)dvi_document->base_width * dvi_document->params->hshrink / thumb_width,
			  (int)dvi_document->base_width * dvi_document->params->vshrink / thumb_height);

	proposed_width = dvi_document->context->dvi_page_w * dvi_document->context->params.conv;
	proposed_height = dvi_document->context->dvi_page_h * dvi_document->context->params.vconv;
			  
	if (border) {
	 	mdvi_pixbuf_device_set_margins	(&dvi_document->context->device, 
						 MAX (thumb_width - proposed_width, 0) / 2,
						 MAX (thumb_height - proposed_height, 0) / 2); 	
	} else {
	 	mdvi_pixbuf_device_set_margins	(&dvi_document->context->device, 
						 MAX (thumb_width - proposed_width - 2, 0) / 2,
						 MAX (thumb_height - proposed_height - 2, 0) / 2); 	
	}
	

        mdvi_pixbuf_device_render (dvi_document->context);
	pixbuf = mdvi_pixbuf_device_get_pixbuf (&dvi_document->context->device);

	g_mutex_unlock (dvi_context_mutex);

	if (border) {
	        border_pixbuf = ev_document_misc_get_thumbnail_frame (thumb_width, thumb_height, NULL);
		gdk_pixbuf_copy_area (pixbuf, 0, 0, 
				      thumb_width - 2, thumb_height - 2,
				      border_pixbuf, 2, 2);
		g_object_unref (pixbuf);
		pixbuf = border_pixbuf;
	}
	
	
	return pixbuf;
}

static void
dvi_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = dvi_document_thumbnails_get_thumbnail;
	iface->get_dimensions = dvi_document_thumbnails_get_dimensions;
}


static void
dvi_document_init_params (DviDocument *dvi_document)
{	
	dvi_document->params = g_new0 (DviParams, 1);	

	dvi_document->params->dpi      = MDVI_DPI;
	dvi_document->params->vdpi     = MDVI_VDPI;
	dvi_document->params->mag      = MDVI_MAGNIFICATION;
	dvi_document->params->density  = MDVI_DEFAULT_DENSITY;
	dvi_document->params->gamma    = MDVI_DEFAULT_GAMMA;
	dvi_document->params->flags    = MDVI_PARAM_ANTIALIASED;
	dvi_document->params->hdrift   = 0;
	dvi_document->params->vdrift   = 0;
	dvi_document->params->hshrink  =  MDVI_SHRINK_FROM_DPI(dvi_document->params->dpi);
	dvi_document->params->vshrink  =  MDVI_SHRINK_FROM_DPI(dvi_document->params->dpi);
	dvi_document->params->orientation = MDVI_ORIENT_TBLR;

	dvi_document->spec = NULL;
	
        dvi_document->params->bg = 0xffffffff;
        dvi_document->params->fg = 0xff000000;

	mdvi_init_kpathsea("evince", MDVI_MFMODE, MDVI_FALLBACK_FONT, MDVI_DPI);
	
	mdvi_register_fonts ();
}

static void
dvi_document_init (DviDocument *dvi_document)
{
	dvi_document->context = NULL;
	dvi_document_init_params (dvi_document);
}
