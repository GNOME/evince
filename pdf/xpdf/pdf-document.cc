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
	GdkRectangle page_rect;
	GdkDrawable *target;

	GDKSplashOutputDev *out;
	PDFDoc *doc;
	
};

static void pdf_document_document_iface_init (EvDocumentIface *iface);

G_DEFINE_TYPE_WITH_CODE (PdfDocument, pdf_document, G_TYPE_OBJECT,
                         { G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
						  pdf_document_document_iface_init) });


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
		globalParams->setupBaseFonts(NULL);
	}

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	filename_g = new GString (filename);
	g_free (filename);

	// open the PDF file
	newDoc = new PDFDoc(filename_g, 0, 0);

	delete filename_g;
  
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

	if (pdf_document->out) {
		pdf_document->out->startDoc(pdf_document->doc->getXRef());
		pdf_document->doc->displayPage (pdf_document->out, 1, 72, 72, 0, gTrue, gTrue);
	}
	
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

	pdf_document->page = page;
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

		if (pdf_document->out)
			delete pdf_document->out;

		if (pdf_document->target) {
			pdf_document->out = new GDKSplashOutputDev (gdk_drawable_get_screen (pdf_document->target),
							 redraw_callback, (void*) document);

			if (pdf_document->doc) {
				pdf_document->out->startDoc(pdf_document->doc->getXRef());
				pdf_document->doc->displayPage (pdf_document->out, 1, 72, 72, 0, gTrue, gTrue);
			}
		}
	}
}

static void
pdf_document_set_page_rect (EvDocument  *document,
			    int          x,
			    int          y,
			    int          width,
			    int          height)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	
	pdf_document->page_rect.x = x;
	pdf_document->page_rect.y = y;
	pdf_document->page_rect.width = width;
	pdf_document->page_rect.height = height;
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

	if (!pdf_document->target)
		return;
	
	page.x = 0;
	page.y = 0;
	page.width = pdf_document->out->getBitmapWidth();
	page.height = pdf_document->out->getBitmapHeight();

	draw.x = clip_x;
	draw.y = clip_y;
	draw.width = clip_width;
	draw.height = clip_height;
	
	if (gdk_rectangle_intersect (&page, &draw, &draw))
		pdf_document->out->redraw (draw.x, draw.y,
					   pdf_document->target,
					   draw.x, draw.y,
					   draw.width, draw.height);
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
	iface->set_target = pdf_document_set_target;
	iface->set_page_rect = pdf_document_set_page_rect;
	iface->render = pdf_document_render;
}

static void
pdf_document_init (PdfDocument *document)
{
}

