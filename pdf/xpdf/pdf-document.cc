/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* pdfdocument.h: Implementation of EvDocument for PDF
 * Copyright (C) 2004, Red Hat, Inc.
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

#include "gpdf-g-switch.h"
#include "pdf-document.h"
#include "gpdf-g-switch.h"

#include "GlobalParams.h"
#include "GDKSplashOutputDev.h"
#include "PDFDoc.h"

typedef struct _PdfDocumentClass PdfDocumentClass;

#define PDF_DOCUMENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PDF_TYPE_DOCUMENT, PdfDocumentClass))
#define PDF_IS_DOCUMENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PDF_TYPE_DOCUMENT))
#define PDF_DOCUMENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PDF_TYPE_DOCUMENT, PdfDocumentClass))

struct _PdfDocumentClass
{
	GObjectClass parent_class;
};

struct _PdfDocument
{
	GObject parent_instance;

	int page;
	int page_x_offset;
	int page_y_offset;
	double scale;
	GdkDrawable *target;

	GDKSplashOutputDev *out;
	PDFDoc *doc;

	gboolean page_valid;
};

static void pdf_document_document_iface_init (EvDocumentIface *iface);

G_DEFINE_TYPE_WITH_CODE (PdfDocument, pdf_document, G_TYPE_OBJECT,
                         { G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
						  pdf_document_document_iface_init) });

static gboolean
document_validate_page (PdfDocument *pdf_document)
{
	if (!pdf_document->page_valid) {
		pdf_document->doc->displayPage (pdf_document->out, pdf_document->page,
						72 * pdf_document->scale,
						72 * pdf_document->scale,
						0, gTrue, gTrue);
		
		pdf_document->page_valid = TRUE;
	}
}

static gboolean
pdf_document_load (EvDocument  *document,
		   const char  *uri,
		   GError     **error)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PDFDoc *newDoc;
	int err;
	char *filename;
	GString *filename_g;
	
	if (!globalParams) {
		globalParams = new GlobalParams("/etc/xpdfrc");
		globalParams->setupBaseFontsFc(NULL);
	}

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	filename_g = new GString (filename);
	g_free (filename);

	// open the PDF file, assumes ownership of filename_g
	newDoc = new PDFDoc(filename_g, 0, 0);

	if (!newDoc->isOk()) {
		err = newDoc->getErrorCode();
		delete newDoc;

		/* FIXME: Add a real error enum to EvDocument */
		g_set_error (error, G_FILE_ERROR,
			     G_FILE_ERROR_FAILED,
			     "Failed to load document (error %d) '%s'\n",
			     err,
			     uri);
		
		return FALSE;
	}

	if (pdf_document->doc)
		delete pdf_document->doc;
	pdf_document->doc = newDoc;

	pdf_document->page = 1;

	if (pdf_document->out)
		pdf_document->out->startDoc(pdf_document->doc->getXRef());

	pdf_document->page_valid = FALSE;
	
	return TRUE;
}

static int
pdf_document_get_n_pages (EvDocument  *document)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);

	if (pdf_document->doc)
		return pdf_document->doc->getNumPages();
	else
		return 1;
}

static void
pdf_document_set_page (EvDocument  *document,
		       int          page)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);

	page = CLAMP (page, 1, ev_document_get_n_pages (document));

	if (page != pdf_document->page) {
		pdf_document->page = page;
		pdf_document->page_valid = FALSE;
	}

}

static int
pdf_document_get_page (EvDocument  *document)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);

	return pdf_document->page;
}

static void
redraw_callback (void *data)
{
	/* Need to hook up through a EvDocument callback? */
}

static void
pdf_document_set_target (EvDocument  *document,
			 GdkDrawable *target)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	
	if (pdf_document->target != target) {
		if (pdf_document->target)
			g_object_unref (pdf_document->target);
		
		pdf_document->target = target;

		if (pdf_document->target)
			g_object_ref (pdf_document->target);

		if (pdf_document->out) {
			delete pdf_document->out;
			pdf_document->out = NULL;
		}

		if (pdf_document->target) {
			pdf_document->out = new GDKSplashOutputDev (gdk_drawable_get_screen (pdf_document->target),
							 redraw_callback, (void*) document);

			if (pdf_document->doc)
				pdf_document->out->startDoc(pdf_document->doc->getXRef());

		}

		pdf_document->page_valid = FALSE;
	}
}

static void
pdf_document_set_scale (EvDocument  *document,
			double       scale)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	
	if (pdf_document->scale != scale) {
		pdf_document->scale = scale;
		pdf_document->page_valid = FALSE;
	}
}

static void
pdf_document_set_page_offset (EvDocument  *document,
			      int          x,
			      int          y)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	
	pdf_document->page_x_offset = x;
	pdf_document->page_y_offset = y;
}

static void
pdf_document_get_page_size (EvDocument   *document,
			    int          *width,
			    int          *height)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);

	if (document_validate_page (pdf_document)) {
		if (width)
			*width = pdf_document->out->getBitmapWidth();
		if (height)
			*height = pdf_document->out->getBitmapHeight();
	} else {
		if (width)
			*width = 1;
		if (height)
			*height = 1;
	}
}

static void
pdf_document_render (EvDocument  *document,
		     int          clip_x,
		     int          clip_y,
		     int          clip_width,
		     int          clip_height)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	GdkRectangle page;
	GdkRectangle draw;

	if (!document_validate_page (pdf_document) || !pdf_document->target)
		return;
	
	page.x = pdf_document->page_x_offset;
	page.y = pdf_document->page_y_offset;
	page.width = pdf_document->out->getBitmapWidth();
	page.height = pdf_document->out->getBitmapHeight();

	draw.x = clip_x;
	draw.y = clip_y;
	draw.width = clip_width;
	draw.height = clip_height;
	
	if (gdk_rectangle_intersect (&page, &draw, &draw))
		pdf_document->out->redraw (draw.x - page.x, draw.y - page.y,
					   pdf_document->target,
					   draw.x, draw.y,
					   draw.width, draw.height);
}

static void
pdf_document_begin_find (EvDocument   *document,
                         const char   *search_string,
                         gboolean      case_sensitive)
{
        /* FIXME make this incremental (idle handler) and multi-page */
        /* Right now it's fully synchronous plus only does the current page */
        
        PdfDocument *pdf_document = PDF_DOCUMENT (document);
        gunichar *ucs4;
        glong ucs4_len;
        int xMin, yMin, xMax, yMax;
        GArray *results;
        EvFindResult result;

        /* FIXME case_sensitive (right now XPDF
         * code is always case insensitive for ASCII
         * and case sensitive for all other languaages)
         */
        
        g_assert (sizeof (gunichar) == sizeof (Unicode));
        ucs4 = g_utf8_to_ucs4_fast (search_string, -1,
                                    &ucs4_len);

        results = g_array_new (FALSE,
                               FALSE,
                               sizeof (EvFindResult));

        if (pdf_document->out->findText (ucs4, ucs4_len,
                                         gTrue, gTrue, // startAtTop, stopAtBottom
                                         gFalse, gFalse, // startAtLast, stopAtLast
                                         &xMin, &yMin, &xMax, &yMax)) {

                result.page_num = pdf_document->page;

                result.highlight_area.x = xMin;
                result.highlight_area.y = yMin;
                result.highlight_area.width = xMax - xMin;
                result.highlight_area.height = yMax - yMin;

                g_array_append_val (results, result);
        
                /* Now find further results */

                while (pdf_document->out->findText (ucs4, ucs4_len,
                                                    gFalse, gTrue,
                                                    gTrue, gFalse,
                                                    &xMin, &yMin, &xMax, &yMax)) {
                        
                        result.page_num = pdf_document->page;
                        
                        result.highlight_area.x = xMin;
                        result.highlight_area.y = yMin;
                        result.highlight_area.width = xMax - xMin;
                        result.highlight_area.height = yMax - yMin;
                        
                        g_array_append_val (results, result);
                }
        }

        ev_document_found (document,
                           (EvFindResult*) results->data,
                           results->len,
                           1.0);

        g_array_free (results, TRUE);
}

static void
pdf_document_end_find (EvDocument   *document)
{
        PdfDocument *pdf_document = PDF_DOCUMENT (document);

        /* FIXME this will do something once begin_find queues
         * an incremental find
         */

        // this should probably be shared among EvDocument
        // implementations somehow?
        ev_document_found (document, NULL, 0, 1.0);
}

static void
pdf_document_finalize (GObject *object)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (object);

	if (pdf_document->target)
		g_object_unref (pdf_document->target);

	if (pdf_document->out)
		delete pdf_document->out;
	if (pdf_document->doc)
		delete pdf_document->doc;

}

static void
pdf_document_class_init (PdfDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
	gobject_class->finalize = pdf_document_finalize;
}

static void
pdf_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = pdf_document_load;
	iface->get_n_pages = pdf_document_get_n_pages;
	iface->set_page = pdf_document_set_page;
	iface->get_page = pdf_document_get_page;
	iface->set_scale = pdf_document_set_scale;
	iface->set_target = pdf_document_set_target;
	iface->set_page_offset = pdf_document_set_page_offset;
	iface->get_page_size = pdf_document_get_page_size;
	iface->render = pdf_document_render;
        iface->begin_find = pdf_document_begin_find;
        iface->end_find = pdf_document_end_find;
}

static void
pdf_document_init (PdfDocument *pdf_document)
{
	pdf_document->page = 1;
	pdf_document->page_x_offset = 0;
	pdf_document->page_y_offset = 0;
	pdf_document->scale = 1.;
	
	pdf_document->page_valid = FALSE;
}

