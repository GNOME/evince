/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
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
#include "ev-ps-exporter.h"
#include "ev-document-find.h"
#include "gpdf-g-switch.h"

#include "GlobalParams.h"
#include "GDKSplashOutputDev.h"
#include "PDFDoc.h"
#include "PSOutputDev.h"

typedef struct
{
	PdfDocument *document;
	gunichar *ucs4;
	glong ucs4_len;
	guint idle;
        /* full results are only possible for the rendered current page */
        int current_page;
        GArray *current_page_results;
        guchar *other_page_flags; /* length n_pages + 1, first element ignored */
        int start_page;   /* skip this one as we iterate, since we did it first */
        int search_page;  /* the page we're searching now */
        TextOutputDev *output_dev;
} PdfDocumentSearch;

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
	PSOutputDev *ps_out;
	PDFDoc *doc;

	gboolean page_valid;

	PdfDocumentSearch *search;
};

static void pdf_document_document_iface_init    (EvDocumentIface     *iface);
static void pdf_document_ps_exporter_iface_init (EvPSExporterIface   *iface);
static void pdf_document_find_iface_init        (EvDocumentFindIface *iface);
static void pdf_document_search_free            (PdfDocumentSearch   *search);
static void pdf_document_search_page_changed    (PdfDocumentSearch   *search);

G_DEFINE_TYPE_WITH_CODE (PdfDocument, pdf_document, G_TYPE_OBJECT,
                         {
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
							pdf_document_document_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_PS_EXPORTER,
							pdf_document_ps_exporter_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND,
							pdf_document_find_iface_init);
			 });

static gboolean
document_validate_page (PdfDocument *pdf_document)
{
	if (!pdf_document->page_valid) {
		pdf_document->doc->displayPage (pdf_document->out, pdf_document->page,
						72 * pdf_document->scale,
						72 * pdf_document->scale,
						0, gTrue, gTrue);
		
		pdf_document->page_valid = TRUE;

                /* Update the search results available to the app since
                 * we only provide full results on the current page
                 */
                if (pdf_document->search)
                        pdf_document_search_page_changed (pdf_document->search);
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
pdf_document_search_emit_found (PdfDocumentSearch *search)
{
        PdfDocument *pdf_document = search->document;
        int n_pages;
        int pages_done;
        GArray *tmp_results;
        int i;

        n_pages = ev_document_get_n_pages (EV_DOCUMENT (search->document));
        if (search->search_page > search->start_page) {
                pages_done = search->search_page - search->start_page;
        } else if (search->search_page == search->start_page) {
                pages_done = n_pages;
        } else {
                pages_done = n_pages - search->start_page + search->search_page;
        }

        tmp_results = g_array_new (FALSE, FALSE, sizeof (EvFindResult));
        g_array_append_vals (tmp_results,
                             search->current_page_results->data,
                             search->current_page_results->len);

        /* Now append a bogus element for each page that has a result in it,
         * that is not the current page
         */
        i = 1;
        while (i <= n_pages) {
                if (i != pdf_document->page &&
                    search->other_page_flags[i]) {
                        EvFindResult result;
                        
                        result.page_num = i;
                        
                        /* Use bogus coordinates, again we can't get coordinates
                         * until this is the current page because TextOutputDev
                         * isn't good enough
                         */
                        result.highlight_area.x = -1;
                        result.highlight_area.y = -1;
                        result.highlight_area.width = 1;
                        result.highlight_area.height = 1;
                        
                        g_array_append_val (tmp_results, result);
                }

                ++i;
        }
        
        ev_document_find_found (EV_DOCUMENT_FIND (pdf_document),
                                (EvFindResult*) tmp_results->data,
                                tmp_results->len,
                                pages_done / (double) n_pages);
        
        g_array_free (tmp_results, TRUE);
}

static void
pdf_document_search_page_changed (PdfDocumentSearch   *search)
{
        PdfDocument *pdf_document = search->document;
        int current_page;
        EvFindResult result;
        int xMin, yMin, xMax, yMax;

        current_page = pdf_document->page;

        if (!pdf_document->page_valid) {
                /* we can't do anything until displayPage() */
                search->current_page = -1;
                return;
        }
        
        if (search->current_page == current_page)
                return;
        
        /* We need to create current_page_results for the new current page */
        g_array_set_size (search->current_page_results, 0);
        
        if (pdf_document->out->findText (search->ucs4, search->ucs4_len,
                                         gTrue, gTrue, // startAtTop, stopAtBottom
                                         gFalse, gFalse, // startAtLast, stopAtLast
                                         &xMin, &yMin, &xMax, &yMax)) {
                result.page_num = pdf_document->page;

                result.highlight_area.x = xMin;
                result.highlight_area.y = yMin;
                result.highlight_area.width = xMax - xMin;
                result.highlight_area.height = yMax - yMin;

                g_array_append_val (search->current_page_results, result);
        
                /* Now find further results */

                while (pdf_document->out->findText (search->ucs4, search->ucs4_len,
                                                    gFalse, gTrue,
                                                    gTrue, gFalse,
                                                    &xMin, &yMin, &xMax, &yMax)) {
                        
                        result.page_num = pdf_document->page;
                        
                        result.highlight_area.x = xMin;
                        result.highlight_area.y = yMin;
                        result.highlight_area.width = xMax - xMin;
                        result.highlight_area.height = yMax - yMin;
                        
                        g_array_append_val (search->current_page_results, result);
                }
        }

        /* needed for the initial current page since we don't search
         * it in the idle
         */
        search->other_page_flags[current_page] =
                search->current_page_results->len > 0;
        
        pdf_document_search_emit_found (search);
}

static gboolean
pdf_document_search_idle_callback (void *data)
{
        PdfDocumentSearch *search = (PdfDocumentSearch*) data;
        PdfDocument *pdf_document = search->document;
        int n_pages;
        double xMin, yMin, xMax, yMax;
        gboolean found;

        /* Note that PDF page count is 1 through n_pages INCLUSIVE
         * like a real book. We are looking to add one result for each
         * page with a match, because the coordinates are meaningless
         * with TextOutputDev, so we just want to flag matching pages
         * and then when the user switches to the current page, we
         * will emit "found" again with the real results.
         */
        n_pages = ev_document_get_n_pages (EV_DOCUMENT (search->document));

        if (search->search_page == search->start_page) {
                goto end_search;
        }

        if (search->output_dev == 0) {
                /* First time through here... */
                search->output_dev = new TextOutputDev (NULL, gTrue, gFalse, gFalse);
                if (!search->output_dev->isOk()) {
                        goto end_search;
                }
        }
                                                  
        pdf_document->doc->displayPage (search->output_dev,
                                        search->search_page,
                                        72, 72, 0, gTrue, gFalse);

        if (search->output_dev->findText (search->ucs4,
                                          search->ucs4_len,
                                          gTrue, gTrue, // startAtTop, stopAtBottom
                                          gFalse, gFalse, // startAtLast, stopAtLast
                                          &xMin, &yMin, &xMax, &yMax)) {
                /* This page has results */
                search->other_page_flags[search->search_page] = TRUE;
        }
        
        search->search_page += 1;
        if (search->search_page > n_pages) {
                /* wrap around */
                search->search_page = 1;
        }

        /* We do this even if nothing was found, to update the percent complete */
        pdf_document_search_emit_found (search);
        
        return TRUE;

 end_search:
        /* We're done. */
        search->idle = 0; /* will return FALSE to remove */
        return FALSE;
}

static void
pdf_document_find_begin (EvDocumentFind   *document,
                         const char       *search_string,
                         gboolean          case_sensitive)
{
        PdfDocument *pdf_document = PDF_DOCUMENT (document);
        PdfDocumentSearch *search;
        int n_pages;
        gunichar *ucs4;
        glong ucs4_len;

        /* FIXME handle case_sensitive (right now XPDF
         * code is always case insensitive for ASCII
         * and case sensitive for all other languaages)
         */
        
        g_assert (sizeof (gunichar) == sizeof (Unicode));
        ucs4 = g_utf8_to_ucs4_fast (search_string, -1,
                                    &ucs4_len);

        if (pdf_document->search &&
            pdf_document->search->ucs4_len == ucs4_len &&
            memcmp (pdf_document->search->ucs4,
                    ucs4,
                    sizeof (gunichar) * ucs4_len) == 0) {
                /* Search is unchanged */
                g_free (ucs4);
                return;
        }

        if (pdf_document->search) {
                pdf_document_search_free (pdf_document->search);
                pdf_document->search = NULL;
        }
        
        search = g_new0 (PdfDocumentSearch, 1);

        search->ucs4 = ucs4;
        search->ucs4_len = ucs4_len;
        
        search->current_page_results = g_array_new (FALSE,
                                                    FALSE,
                                                    sizeof (EvFindResult));
        n_pages = ev_document_get_n_pages (EV_DOCUMENT (document)); 

        /* This is an array of bool; with the first value ignored
         * so we can index by the based-at-1 page numbers
         */
        search->other_page_flags = g_new0 (guchar, n_pages + 1);
        
        search->document = pdf_document;

        /* We add at low priority so the progress bar repaints */
        search->idle = g_idle_add_full (G_PRIORITY_LOW,
                                        pdf_document_search_idle_callback,
                                        search,
                                        NULL);

        search->output_dev = 0;

        search->start_page = pdf_document->page;
        search->search_page = search->start_page + 1;
        if (search->search_page > n_pages)
                search->search_page = 1;

        search->current_page = -1;

        pdf_document->search = search;
        
        /* Update for the current page right away */
        pdf_document_search_page_changed (search);
}

static void
pdf_document_find_cancel (EvDocumentFind   *document)
{
        PdfDocument *pdf_document = PDF_DOCUMENT (document);

	if (pdf_document->search) {
		pdf_document_search_free (pdf_document->search);
		pdf_document->search = NULL;
	}
}

static void
pdf_document_search_free (PdfDocumentSearch   *search)
{
        if (search->idle != 0)
                g_source_remove (search->idle);

	if (search->output_dev)
		delete search->output_dev;
	
        g_array_free (search->current_page_results, TRUE);
        g_free (search->other_page_flags);
        
        g_free (search->ucs4);
	g_free (search);
}

static void
pdf_document_ps_export_begin (EvPSExporter *exporter, const char *filename)
{
	PdfDocument *document = PDF_DOCUMENT (exporter);

	if (document->ps_out)
		delete document->ps_out;

	document->ps_out = new PSOutputDev ((char *)filename, document->doc->getXRef(),
					    document->doc->getCatalog(), 1,
					    ev_document_get_n_pages (EV_DOCUMENT (document)),
					    psModePS);	
}

static void
pdf_document_ps_export_do_page (EvPSExporter *exporter, int page)
{
	PdfDocument *document = PDF_DOCUMENT (exporter);

	document->doc->displayPage (document->ps_out, page,
				    72.0, 72.0, 0, gTrue, gFalse);
}

static void
pdf_document_ps_export_end (EvPSExporter *exporter)
{
	PdfDocument *document = PDF_DOCUMENT (exporter);

	delete document->ps_out;
	document->ps_out = NULL;
}

static void
pdf_document_finalize (GObject *object)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (object);

	if (pdf_document->search)
		pdf_document_search_free (pdf_document->search);
	
	if (pdf_document->target)
		g_object_unref (pdf_document->target);

	if (pdf_document->out)
		delete pdf_document->out;
	if (pdf_document->ps_out)
		delete pdf_document->ps_out;
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
}

static void
pdf_document_ps_exporter_iface_init (EvPSExporterIface *iface)
{
	iface->begin = pdf_document_ps_export_begin;
	iface->do_page = pdf_document_ps_export_do_page;
	iface->end = pdf_document_ps_export_end;
}


static void
pdf_document_find_iface_init (EvDocumentFindIface *iface)
{
        iface->begin = pdf_document_find_begin;
        iface->cancel = pdf_document_find_cancel;
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

