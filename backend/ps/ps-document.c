/* Ghostscript widget for GTK/GNOME
 *
 * Copyright (C) 1998 - 2005 the Free Software Foundation
 *
 * Authors: Jonathan Blandford, Jaka Mocnik, Carlos Garcia Campos
 *
 * Based on code by: Federico Mena (Quartic), Szekeres Istvan (Pista)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkpixbuf.h>
#include <string.h>
#include <stdlib.h>

#include "ps-document.h"
#include "ps-interpreter.h"
#include "ps.h"
#include "gstypes.h"
#include "gsdefaults.h"
#include "ev-file-exporter.h"
#include "ev-async-renderer.h"
#include "ev-document-thumbnails.h"
#include "ev-document-misc.h"

struct _PSDocument {
	GObject object;

	gchar *filename;
	struct document *doc;
	gboolean structured_doc;

	PSInterpreter *gs;

	/* Document Thumbnails */
	PSInterpreter   *thumbs_gs;
	GdkPixbuf       *thumbnail;
	EvRenderContext *thumbs_rc;
	GMutex          *thumbs_mutex;
	GCond           *thumbs_cond;
	
	/* File exporter */
	gint  *ps_export_pagelist;
	gchar *ps_export_filename;
};

struct _PSDocumentClass {
	GObjectClass parent_class;
}; 

static void     ps_document_document_iface_init            (EvDocumentIface           *iface);
static void     ps_document_file_exporter_iface_init       (EvFileExporterIface       *iface);
static void     ps_async_renderer_iface_init               (EvAsyncRendererIface      *iface);
static void     ps_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);

static void     ps_interpreter_page_rendered               (PSInterpreter             *gs,
							    GdkPixbuf                 *pixbuf,
							    PSDocument                *ps_document);

EV_BACKEND_REGISTER_WITH_CODE (PSDocument, ps_document,
                         {
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
							ps_document_document_thumbnails_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER,
							ps_document_file_exporter_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_ASYNC_RENDERER,
							ps_async_renderer_iface_init);
			 });

/* PSDocument */
static void
ps_document_init (PSDocument *ps_document)
{
}

static void
ps_document_dispose (GObject *object)
{
	PSDocument *ps_document = PS_DOCUMENT (object);

	if (ps_document->gs) {
		g_object_unref (ps_document->gs);
		ps_document->gs = NULL;
	}

	if (ps_document->thumbs_gs) {
		g_object_unref (ps_document->thumbs_gs);
		ps_document->thumbs_gs = NULL;
	}
	
	if (ps_document->filename) {
		g_free (ps_document->filename);
		ps_document->filename = NULL;
	}

	if (ps_document->doc) {
		psfree (ps_document->doc);
		ps_document->doc = NULL;
	}

	if (ps_document->thumbnail) {
		g_object_unref (ps_document->thumbnail);
		ps_document->thumbnail = NULL;
	}

	if (ps_document->thumbs_mutex) {
		g_mutex_free (ps_document->thumbs_mutex);
		ps_document->thumbs_mutex = NULL;
	}
	
	if (ps_document->thumbs_cond) {
		g_cond_free (ps_document->thumbs_cond);
		ps_document->thumbs_cond = NULL;
	}

	if (ps_document->thumbs_rc) {
		g_object_unref (ps_document->thumbs_rc);
		ps_document->thumbs_rc = NULL;
	}

	G_OBJECT_CLASS (ps_document_parent_class)->dispose (object);
}

static void
ps_document_class_init (PSDocumentClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ps_document_dispose;
}

/* EvDocumentIface */
static gboolean
document_load (PSDocument *ps_document, const gchar *fname, GError **error)
{
	FILE *fd;
	
	ps_document->filename = g_strdup (fname);

	/*
	 * We need to make sure that the file is loadable/exists!
	 * otherwise we want to exit without loading new stuff...
	 */
	if (!g_file_test (fname, G_FILE_TEST_IS_REGULAR)) {
		gchar *filename_dsp;

		filename_dsp = g_filename_display_name (fname);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_NOENT,
			     _("Cannot open file “%s”."),
			     filename_dsp);
		g_free (filename_dsp);
		
		return FALSE;
	}

	if ((fd = fopen (ps_document->filename, "r")) == NULL) {
		gchar *filename_dsp;

		filename_dsp = g_filename_display_name (fname);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_NOENT,
			     _("Cannot open file “%s”."),
			     filename_dsp);
		g_free (filename_dsp);
		
		return FALSE;
	}
	
	/* we grab the vital statistics!!! */
	ps_document->doc = psscan (fd, TRUE, ps_document->filename);
	fclose (fd);
	if (!ps_document->doc)
		return FALSE;

	ps_document->structured_doc =
		((!ps_document->doc->epsf && ps_document->doc->numpages > 0) ||
		 (ps_document->doc->epsf && ps_document->doc->numpages > 1));

	ps_document->gs = ps_interpreter_new (ps_document->filename,
					      ps_document->doc);
	g_signal_connect (G_OBJECT (ps_document->gs), "page_rendered",
			  G_CALLBACK (ps_interpreter_page_rendered),
			  (gpointer) ps_document);
	
	return TRUE;
}

static gboolean
ps_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	char *filename;
	char *gs_path;
	gboolean result;

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	gs_path = g_find_program_in_path ("gs");
	if (!gs_path) {
		gchar *filename_dsp;
		
		filename_dsp = g_filename_display_name (filename);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_NOENT,
			     _("Failed to load document “%s”. Ghostscript interpreter was not found in path"),
			     filename);
		g_free (filename_dsp);
		g_free (filename);
		
		return FALSE;
	}
	g_free (gs_path);

	result = document_load (PS_DOCUMENT (document), filename, error);
	if (!result && !(*error)) {
		gchar *filename_dsp;
		
		filename_dsp = g_filename_display_name (filename);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_FAILED,
			     _("Failed to load document “%s”"),
			     filename_dsp);
		g_free (filename_dsp);
	}
	
	g_free (filename);

	return result;
}

static gboolean
save_document (PSDocument *document, const char *filename)
{
	gboolean result = TRUE;
	GtkGSDocSink *sink = gtk_gs_doc_sink_new ();
	FILE *f, *src_file;
	gchar *buf;

	src_file = fopen (document->filename, "r");
	if (src_file) {
		struct stat stat_rec;

		if (stat (document->filename, &stat_rec) == 0) {
			pscopy (src_file, sink, 0, stat_rec.st_size - 1);
		}

		fclose (src_file);
	}
	
	buf = gtk_gs_doc_sink_get_buffer (sink);
	if (buf == NULL) {
		return FALSE;
	}
	
	f = fopen (filename, "w");
	if (f) {
		fputs (buf, f);
		fclose (f);
	} else {
		result = FALSE;
	}

	g_free (buf);
	gtk_gs_doc_sink_free (sink);
	g_free (sink);

	return result;
}

static gboolean
save_page_list (PSDocument *document, int *page_list, const char *filename)
{
	gboolean result = TRUE;
	GtkGSDocSink *sink = gtk_gs_doc_sink_new ();
	FILE *f;
	gchar *buf;

	pscopydoc (sink, document->filename, 
		   document->doc, page_list);
	
	buf = gtk_gs_doc_sink_get_buffer (sink);
	
	f = fopen (filename, "w");
	if (f) {
		fputs (buf, f);
		fclose (f);
	} else {
		result = FALSE;
	}

	g_free (buf);
	gtk_gs_doc_sink_free (sink);
	g_free (sink);

	return result;
}

static gboolean
ps_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	PSDocument *ps = PS_DOCUMENT (document);
	gboolean result;
	char *filename;

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	result = save_document (ps, filename);

	g_free (filename);

	return result;
}

static int
ps_document_get_n_pages (EvDocument *document)
{
	PSDocument *ps = PS_DOCUMENT (document);

	if (!ps->filename || !ps->doc) {
		return -1;
	}

	return ps->structured_doc ? ps->doc->numpages : 1;
}

static gint
ps_document_get_page_rotation (PSDocument *ps_document,
			       int         page)
{
	gint rotation = GTK_GS_ORIENTATION_NONE;

	g_assert (ps_document->doc != NULL);
	
	if (ps_document->structured_doc) {
		if (ps_document->doc->pages[page].orientation != GTK_GS_ORIENTATION_NONE)
			rotation = ps_document->doc->pages[page].orientation;
		else
			rotation = ps_document->doc->default_page_orientation;
	}

	if (rotation == GTK_GS_ORIENTATION_NONE)
		rotation = ps_document->doc->orientation;

	if (rotation == GTK_GS_ORIENTATION_NONE)
		rotation = GTK_GS_ORIENTATION_PORTRAIT;

	return rotation;
}

static void
ps_document_get_page_size (EvDocument *document,
			   int         page,
			   double     *width,
			   double     *height)
{
	PSDocument *ps_document = PS_DOCUMENT (document);
	int urx, ury, llx, lly;
	gdouble pwidth, pheight;
	gdouble page_width, page_height;
	gint rotate;

	psgetpagebox (ps_document->doc, page, &urx, &ury, &llx, &lly);

	pwidth = (urx - llx) + 0.5;
	pheight = (ury - lly) + 0.5;

	rotate = ps_document_get_page_rotation (ps_document, page);
	if (rotate == 90 || rotate == 270) {
		page_height = pwidth;
		page_width = pheight;
	} else {
		page_width = pwidth;
		page_height = pheight;
	}
	
	if (width) {
		*width = page_width;
	}

	if (height) {
		*height = page_height;
	}
}

static EvDocumentInfo *
ps_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	PSDocument *ps = PS_DOCUMENT (document);
	int urx, ury, llx, lly;

	info = g_new0 (EvDocumentInfo, 1);
	info->fields_mask = EV_DOCUMENT_INFO_TITLE |
	                    EV_DOCUMENT_INFO_FORMAT |
			    EV_DOCUMENT_INFO_CREATOR |
			    EV_DOCUMENT_INFO_N_PAGES |
	                    EV_DOCUMENT_INFO_PAPER_SIZE;

	info->title = g_strdup (ps->doc->title);
	info->format = ps->doc->epsf ? g_strdup (_("Encapsulated PostScript"))
		                     : g_strdup (_("PostScript"));
	info->creator = g_strdup (ps->doc->creator);
	info->n_pages = ev_document_get_n_pages (document);
	
	psgetpagebox (PS_DOCUMENT (document)->doc, 0, &urx, &ury, &llx, &lly);

	info->paper_width  = (urx - llx) / 72.0f * 25.4f;
	info->paper_height = (ury - lly) / 72.0f * 25.4f;

	return info;
}

static void
ps_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = ps_document_load;
	iface->save = ps_document_save;
	iface->get_n_pages = ps_document_get_n_pages;
	iface->get_page_size = ps_document_get_page_size;
	iface->get_info = ps_document_get_info;
}

/* EvAsyncRendererIface */
static void
ps_interpreter_page_rendered (PSInterpreter *gs,
			      GdkPixbuf     *pixbuf,
			      PSDocument    *ps_document)
{
	g_signal_emit_by_name (ps_document, "render_finished", pixbuf);
}

static void
ps_async_renderer_render_pixbuf (EvAsyncRenderer *renderer,
				 gint             page,
				 gdouble          scale,
				 gint             rotation)
{
	PSDocument *ps_document = PS_DOCUMENT (renderer);

	g_return_if_fail (PS_IS_INTERPRETER (ps_document->gs));

	rotation = (rotation + ps_document_get_page_rotation (ps_document, page)) % 360;

	ps_interpreter_render_page (ps_document->gs, page, scale, rotation);
}

static void
ps_async_renderer_iface_init (EvAsyncRendererIface *iface)
{
	iface->render_pixbuf = ps_async_renderer_render_pixbuf;
}

/* EvDocumentThumbnailsIface */
static void
ps_interpreter_thumbnail_rendered (PSInterpreter *gs,
				   GdkPixbuf     *pixbuf,
				   PSDocument    *ps_document)
{
	if (ps_document->thumbnail)
		g_object_unref (ps_document->thumbnail);
	ps_document->thumbnail = g_object_ref (pixbuf);

	g_cond_broadcast (ps_document->thumbs_cond);
}

static gboolean
ps_document_render_thumbnail (PSDocument *ps_document)
{
	ps_interpreter_render_page (ps_document->thumbs_gs,
				    ps_document->thumbs_rc->page,
				    ps_document->thumbs_rc->scale,
				    ps_document->thumbs_rc->rotation);

	return FALSE;
}

static GdkPixbuf *
ps_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document_thumbnails,
				      EvRenderContext      *rc, 
				      gboolean              border)
{
	PSDocument *ps_document;
	GdkPixbuf  *pixbuf = NULL;

	ps_document = PS_DOCUMENT (document_thumbnails);
	
	g_return_val_if_fail (ps_document->filename != NULL, NULL);
	g_return_val_if_fail (ps_document->doc != NULL, NULL);
		
	if (!ps_document->thumbs_gs) {
		ps_document->thumbs_gs = ps_interpreter_new (ps_document->filename,
							     ps_document->doc);
		g_signal_connect (G_OBJECT (ps_document->thumbs_gs), "page_rendered",
				  G_CALLBACK (ps_interpreter_thumbnail_rendered),
				  (gpointer) ps_document);
	}

	if (!ps_document->thumbs_mutex)
		ps_document->thumbs_mutex = g_mutex_new ();
	ps_document->thumbs_cond = g_cond_new ();

	if (ps_document->thumbs_rc)
		g_object_unref (ps_document->thumbs_rc);
	ps_document->thumbs_rc = g_object_ref (rc);

	ev_document_doc_mutex_unlock ();
	g_mutex_lock (ps_document->thumbs_mutex);
	g_idle_add ((GSourceFunc)ps_document_render_thumbnail, ps_document);
	g_cond_wait (ps_document->thumbs_cond, ps_document->thumbs_mutex);
	g_cond_free (ps_document->thumbs_cond);
	ps_document->thumbs_cond = NULL;
	g_mutex_unlock (ps_document->thumbs_mutex);
	ev_document_doc_mutex_lock ();
	
	pixbuf = ps_document->thumbnail;
	ps_document->thumbnail = NULL;
	
	if (border) {
		GdkPixbuf *border_pixbuf;
		
		border_pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, pixbuf);
		g_object_unref (pixbuf);
		pixbuf = border_pixbuf;
	}

	return pixbuf;
}

static void
ps_document_thumbnails_get_dimensions (EvDocumentThumbnails *document_thumbnails,
				       EvRenderContext      *rc, 
				       gint                 *width,
				       gint                 *height)
{
	PSDocument *ps_document;
	gdouble     page_width, page_height;

	ps_document = PS_DOCUMENT (document_thumbnails);
	
	ps_document_get_page_size (EV_DOCUMENT (ps_document),
				   rc->page,
				   &page_width, &page_height);
	
	if (rc->rotation == 90 || rc->rotation == 270) {
		*width = (gint) (page_height * rc->scale);
		*height = (gint) (page_width * rc->scale);
	} else {
		*width = (gint) (page_width * rc->scale);
		*height = (gint) (page_height * rc->scale);
	}
}

static void
ps_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = ps_document_thumbnails_get_thumbnail;
	iface->get_dimensions = ps_document_thumbnails_get_dimensions;
}

/* EvFileExporterIface */
static void
ps_document_file_exporter_begin (EvFileExporter        *exporter,
				 EvFileExporterContext *fc)
{
	PSDocument *document = PS_DOCUMENT (exporter);

	if (document->structured_doc) {
		g_free (document->ps_export_pagelist);
	
		document->ps_export_pagelist = g_new0 (int, document->doc->numpages);
	}

	document->ps_export_filename = g_strdup (fc->filename);
}

static void
ps_document_file_exporter_do_page (EvFileExporter  *exporter,
				   EvRenderContext *rc)
{
	PSDocument *document = PS_DOCUMENT (exporter);
	
	if (document->structured_doc) {
		document->ps_export_pagelist[rc->page] = 1;
	}
}

static void
ps_document_file_exporter_end (EvFileExporter *exporter)
{
	PSDocument *document = PS_DOCUMENT (exporter);

	if (!document->structured_doc) {
		save_document (document, document->ps_export_filename);
	} else {
		save_page_list (document, document->ps_export_pagelist,
				document->ps_export_filename);
		g_free (document->ps_export_pagelist);
		g_free (document->ps_export_filename);	
		document->ps_export_pagelist = NULL;
		document->ps_export_filename = NULL;
	}
}

static EvFileExporterCapabilities
ps_document_file_exporter_get_capabilities (EvFileExporter *exporter)
{
	return  EV_FILE_EXPORTER_CAN_PAGE_SET |
		EV_FILE_EXPORTER_CAN_GENERATE_PS;
}

static void
ps_document_file_exporter_iface_init (EvFileExporterIface *iface)
{
	iface->begin = ps_document_file_exporter_begin;
	iface->do_page = ps_document_file_exporter_do_page;
	iface->end = ps_document_file_exporter_end;
	iface->get_capabilities = ps_document_file_exporter_get_capabilities;
}
