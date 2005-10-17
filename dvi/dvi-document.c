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
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-xfer.h>

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
	
	gchar *uri;
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
    
    if (!filename) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("File not available"));
        	return FALSE;
    }
	
    if (dvi_document->context)
	mdvi_destroy_context (dvi_document->context);

    dvi_document->context = mdvi_init_context(dvi_document->params, dvi_document->spec, filename);

    if (!dvi_document->context) {
    		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("DVI document has incorrect format"));
        	return FALSE;
    }

    mdvi_pixbuf_device_init (&dvi_document->context->device);

    dvi_document->base_width = dvi_document->context->dvi_page_w * dvi_document->context->params.conv 
		+ 2 * unit2pix(dvi_document->params->dpi, MDVI_HMARGIN) / dvi_document->params->hshrink;
		
    dvi_document->base_height = dvi_document->context->dvi_page_h * dvi_document->context->params.vconv 
	        + 2 * unit2pix(dvi_document->params->vdpi, MDVI_VMARGIN) / dvi_document->params->vshrink;

    dvi_context_mutex = g_mutex_new ();

    g_free (dvi_document->uri);
    dvi_document->uri = g_strdup (uri);

    return TRUE;
}


static gboolean
dvi_document_save (EvDocument  *document,
		      const char  *uri,
		      GError     **error)
{
	DviDocument *dvi_document = DVI_DOCUMENT (document);
	GnomeVFSResult result;
	GnomeVFSURI *source_uri;
	GnomeVFSURI *target_uri;
	
	if (!dvi_document->uri)
		return FALSE;
	
	source_uri = gnome_vfs_uri_new (dvi_document->uri);
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

        *width = dvi_document->base_width;
        *height = dvi_document->base_height;;
				    
	return;
}

static GdkPixbuf *
dvi_document_render_pixbuf (EvDocument  *document,
			    EvRenderContext *rc)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *rotated_pixbuf;

	DviDocument *dvi_document = DVI_DOCUMENT(document);

	gint required_width, required_height;
	gint proposed_width, proposed_height;
	gint xmargin = 0, ymargin = 0;

	/* We should protect our context since it's not 
	 * thread safe. The work to the future - 
	 * let context render page independently
	 */
	g_mutex_lock (dvi_context_mutex);
	
	mdvi_setpage(dvi_document->context,  rc->page);
	
	mdvi_set_shrink (dvi_document->context, 
			 (int)((dvi_document->params->hshrink - 1) / rc->scale) + 1,
			 (int)((dvi_document->params->vshrink - 1) / rc->scale) + 1);

	required_width = dvi_document->base_width * rc->scale;
	required_height = dvi_document->base_height * rc->scale;
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

	rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf, 360 - rc->rotation);
	g_object_unref (pixbuf);

	return rotated_pixbuf;
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

        g_free (dvi_document->uri);
		
	G_OBJECT_CLASS (dvi_document_parent_class)->finalize (object);
}


static void
dvi_document_class_init (DviDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = dvi_document_finalize;
}

static gboolean
dvi_document_can_get_text (EvDocument *document)
{
	return FALSE;
}

static EvDocumentInfo *
dvi_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;

	info = g_new0 (EvDocumentInfo, 1);

	return info;
}

static void
dvi_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = dvi_document_load;
	iface->save = dvi_document_save;
	iface->can_get_text = dvi_document_can_get_text;
	iface->get_n_pages = dvi_document_get_n_pages;
	iface->get_page_size = dvi_document_get_page_size;
	iface->render_pixbuf = dvi_document_render_pixbuf;
	iface->get_info = dvi_document_get_info;
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
				       gint 		         rotation,
				       gint			 width,
				       gboolean 		 border)
{
	DviDocument *dvi_document = DVI_DOCUMENT (document);
	GdkPixbuf *pixbuf;
	GdkPixbuf *border_pixbuf;
	GdkPixbuf *rotated_pixbuf;
	gint thumb_width, thumb_height;
	gint proposed_width, proposed_height;
	
	dvi_document_thumbnails_get_dimensions (document, page, width, &thumb_width, &thumb_height);

	g_mutex_lock (dvi_context_mutex);

	mdvi_setpage(dvi_document->context,  page);

	mdvi_set_shrink (dvi_document->context, 
			  (int)dvi_document->base_width * dvi_document->params->hshrink / thumb_width,
			  (int)dvi_document->base_height * dvi_document->params->vshrink / thumb_height);

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
	dvi_document->params->vshrink  =  MDVI_SHRINK_FROM_DPI(dvi_document->params->vdpi);
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
