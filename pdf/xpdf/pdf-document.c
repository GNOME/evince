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

#include "pdf-document.h"

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

	GdkRectangle page_rect;
	GdkDrawable *target;
	
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
	return TRUE;
}

static int
pdf_document_get_n_pages (EvDocument  *document)
{
	return 1;
}

static void
pdf_document_set_page (EvDocument  *document,
		       int          page)
{
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
}

static void
pdf_document_finalize (GObject *object)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (object);

	if (pdf_document->target)
		g_object_unref (pdf_document->target);

}

static void
pdf_document_class_init (PdfDocumentClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  
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

