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

#include "config.h"

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <poppler.h>
#include <poppler-document.h>
#include <poppler-page.h>
#ifdef HAVE_CAIRO_PDF
#include <cairo-pdf.h>
#endif
#ifdef HAVE_CAIRO_PS
#include <cairo-ps.h>
#endif
#include <glib/gi18n-lib.h>

#include "ev-poppler.h"
#include "ev-file-exporter.h"
#include "ev-mapping.h"
#include "ev-document-find.h"
#include "ev-document-misc.h"
#include "ev-document-links.h"
#include "ev-document-images.h"
#include "ev-document-fonts.h"
#include "ev-document-security.h"
#include "ev-document-thumbnails.h"
#include "ev-document-transition.h"
#include "ev-document-forms.h"
#include "ev-document-layers.h"
#include "ev-document-print.h"
#include "ev-document-annotations.h"
#include "ev-selection.h"
#include "ev-transition-effect.h"
#include "ev-attachment.h"
#include "ev-image.h"

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#if (defined (HAVE_POPPLER_PAGE_RENDER)) && (defined (HAVE_CAIRO_PDF) || defined (HAVE_CAIRO_PS))
#define HAVE_CAIRO_PRINT
#endif

typedef struct {
	PdfDocument *document;
	char *text;
	GList **pages;
	guint idle;
	int start_page;
	int search_page;
} PdfDocumentSearch;

typedef struct {
	EvFileExporterFormat format;

	/* Pages per sheet */
	gint pages_per_sheet;
	gint pages_printed;
	gint pages_x;
	gint pages_y;
	gdouble paper_width;
	gdouble paper_height;
	
#ifdef HAVE_CAIRO_PRINT
	cairo_t *cr;
#else
	PopplerPSFile *ps_file;
#endif
} PdfPrintContext;

struct _PdfDocumentClass
{
	GObjectClass parent_class;
};

struct _PdfDocument
{
	GObject parent_instance;

	PopplerDocument *document;
	gchar *password;
	gboolean modified;

	PopplerFontInfo *font_info;
	PopplerFontsIter *fonts_iter;
	int fonts_scanned_pages;

	PdfDocumentSearch *search;
	PdfPrintContext *print_ctx;

	GList *layers;
};

static void pdf_document_document_iface_init             (EvDocumentIface            *iface);
static void pdf_document_security_iface_init             (EvDocumentSecurityIface    *iface);
static void pdf_document_document_thumbnails_iface_init  (EvDocumentThumbnailsIface  *iface);
static void pdf_document_document_links_iface_init       (EvDocumentLinksIface       *iface);
static void pdf_document_document_images_iface_init      (EvDocumentImagesIface      *iface);
static void pdf_document_document_forms_iface_init       (EvDocumentFormsIface       *iface);
static void pdf_document_document_fonts_iface_init       (EvDocumentFontsIface       *iface);
static void pdf_document_document_layers_iface_init      (EvDocumentLayersIface      *iface);
#ifdef HAVE_POPPLER_PAGE_RENDER
static void pdf_document_document_print_iface_init       (EvDocumentPrintIface       *iface);
#endif
static void pdf_document_document_annotations_iface_init (EvDocumentAnnotationsIface *iface);
static void pdf_document_find_iface_init                 (EvDocumentFindIface        *iface);
static void pdf_document_file_exporter_iface_init        (EvFileExporterIface        *iface);
static void pdf_selection_iface_init                     (EvSelectionIface           *iface);
static void pdf_document_page_transition_iface_init      (EvDocumentTransitionIface  *iface);
static void pdf_document_thumbnails_get_dimensions       (EvDocumentThumbnails       *document_thumbnails,
							  EvRenderContext            *rc,
							  gint                       *width,
							  gint                       *height);
static int  pdf_document_get_n_pages			 (EvDocument                 *document);

static EvLinkDest *ev_link_dest_from_dest   (PdfDocument       *pdf_document,
					     PopplerDest       *dest);
static EvLink     *ev_link_from_action      (PdfDocument       *pdf_document,
					     PopplerAction     *action);
static void        pdf_document_search_free (PdfDocumentSearch *search);
static void        pdf_print_context_free   (PdfPrintContext   *ctx);

EV_BACKEND_REGISTER_WITH_CODE (PdfDocument, pdf_document,
			 {
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_SECURITY,
								 pdf_document_security_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
								 pdf_document_document_thumbnails_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LINKS,
								 pdf_document_document_links_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_IMAGES,
								 pdf_document_document_images_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FORMS,
								 pdf_document_document_forms_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FONTS,
								 pdf_document_document_fonts_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LAYERS,
								 pdf_document_document_layers_iface_init);
#ifdef HAVE_POPPLER_PAGE_RENDER
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_PRINT,
								 pdf_document_document_print_iface_init);
#endif
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_ANNOTATIONS,
								 pdf_document_document_annotations_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND,
								 pdf_document_find_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER,
								 pdf_document_file_exporter_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_SELECTION,
								 pdf_selection_iface_init);
				 EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_TRANSITION,
								 pdf_document_page_transition_iface_init);
			 });

static void
pdf_document_search_free (PdfDocumentSearch   *search)
{
        PdfDocument *pdf_document = search->document;
	int n_pages;
	int i;

        if (search->idle != 0)
                g_source_remove (search->idle);

	n_pages = pdf_document_get_n_pages (EV_DOCUMENT (pdf_document));
	for (i = 0; i < n_pages; i++) {
		g_list_foreach (search->pages[i], (GFunc) g_free, NULL);
		g_list_free (search->pages[i]);
	}
	g_free (search->pages);
	
	g_free (search->text);
	g_free (search);
}

static void
pdf_document_dispose (GObject *object)
{
	PdfDocument *pdf_document = PDF_DOCUMENT(object);

	if (pdf_document->print_ctx) {
		pdf_print_context_free (pdf_document->print_ctx);
		pdf_document->print_ctx = NULL;
	}
	
	if (pdf_document->search) {
		pdf_document_search_free (pdf_document->search);
		pdf_document->search = NULL;
	}

	if (pdf_document->document) {
		g_object_unref (pdf_document->document);
	}

	if (pdf_document->font_info) { 
		poppler_font_info_free (pdf_document->font_info);
	}

	if (pdf_document->fonts_iter) {
		poppler_fonts_iter_free (pdf_document->fonts_iter);
	}

	if (pdf_document->layers) {
		g_list_foreach (pdf_document->layers, (GFunc)g_object_unref, NULL);
		g_list_free (pdf_document->layers);
	}

	G_OBJECT_CLASS (pdf_document_parent_class)->dispose (object);
}

static void
pdf_document_class_init (PdfDocumentClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->dispose = pdf_document_dispose;
}

static void
pdf_document_init (PdfDocument *pdf_document)
{
	pdf_document->password = NULL;
}

static void
convert_error (GError  *poppler_error,
	       GError **error)
{
	if (poppler_error == NULL)
		return;

	if (poppler_error->domain == POPPLER_ERROR) {
		/* convert poppler errors into EvDocument errors */
		gint code = EV_DOCUMENT_ERROR_INVALID;
		if (poppler_error->code == POPPLER_ERROR_INVALID)
			code = EV_DOCUMENT_ERROR_INVALID;
		else if (poppler_error->code == POPPLER_ERROR_ENCRYPTED)
			code = EV_DOCUMENT_ERROR_ENCRYPTED;
			
		g_set_error_literal (error,
                                     EV_DOCUMENT_ERROR,
                                     code,
                                     poppler_error->message);

		g_error_free (poppler_error);
	} else {
		g_propagate_error (error, poppler_error);
	}
}


/* EvDocument */
static gboolean
pdf_document_save (EvDocument  *document,
		   const char  *uri,
		   GError     **error)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	gboolean retval;
	GError *poppler_error = NULL;

	if (pdf_document->modified) {
		retval = poppler_document_save (pdf_document->document,
						uri, &poppler_error);
	} else {
		retval = poppler_document_save_a_copy (pdf_document->document,
						       uri, &poppler_error);
	}
						       
	if (! retval)
		convert_error (poppler_error, error);

	return retval;
}

static gboolean
pdf_document_load (EvDocument   *document,
		   const char   *uri,
		   GError      **error)
{
	GError *poppler_error = NULL;
	PdfDocument *pdf_document = PDF_DOCUMENT (document);

	pdf_document->document =
		poppler_document_new_from_file (uri, pdf_document->password, &poppler_error);

	if (pdf_document->document == NULL) {
		convert_error (poppler_error, error);
		return FALSE;
	}

	return TRUE;
}

static int
pdf_document_get_n_pages (EvDocument *document)
{
	return poppler_document_get_n_pages (PDF_DOCUMENT (document)->document);
}

static EvPage *
pdf_document_get_page (EvDocument *document,
		       gint        index)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerPage *poppler_page;
	EvPage      *page;

	poppler_page = poppler_document_get_page (pdf_document->document, index);
	page = ev_page_new (index);
	page->backend_page = (EvBackendPage)g_object_ref (poppler_page);
	page->backend_destroy_func = (EvBackendPageDestroyFunc)g_object_unref;
	g_object_unref (poppler_page);

	return page;
}

static void
pdf_document_get_page_size (EvDocument *document,
			    EvPage     *page,
			    double     *width,
			    double     *height)
{
	g_return_if_fail (POPPLER_IS_PAGE (page->backend_page));
	
	poppler_page_get_size (POPPLER_PAGE (page->backend_page), width, height);
}

static char *
pdf_document_get_page_label (EvDocument *document,
			     EvPage     *page)
{
	char *label = NULL;

	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);

	g_object_get (G_OBJECT (page->backend_page),
		      "label", &label,
		      NULL);
	return label;
}

static gboolean
pdf_document_has_attachments (EvDocument *document)
{
	PdfDocument *pdf_document;

	pdf_document = PDF_DOCUMENT (document);

	return poppler_document_has_attachments (pdf_document->document);
}

struct SaveToBufferData {
	gchar *buffer;
	gsize len, max;
};

static gboolean
attachment_save_to_buffer_callback (const gchar  *buf,
				    gsize         count,
				    gpointer      user_data,
				    GError      **error)
{
	struct SaveToBufferData *sdata = (SaveToBufferData *)user_data;
	gchar *new_buffer;
	gsize new_max;

	if (sdata->len + count > sdata->max) {
		new_max = MAX (sdata->max * 2, sdata->len + count);
		new_buffer = (gchar *)g_realloc (sdata->buffer, new_max);

		sdata->buffer = new_buffer;
		sdata->max = new_max;
	}
	
	memcpy (sdata->buffer + sdata->len, buf, count);
	sdata->len += count;
	
	return TRUE;
}

static gboolean
attachment_save_to_buffer (PopplerAttachment  *attachment,
			   gchar             **buffer,
			   gsize              *buffer_size,
			   GError            **error)
{
	static const gint initial_max = 1024;
	struct SaveToBufferData sdata;

	*buffer = NULL;
	*buffer_size = 0;

	sdata.buffer = (gchar *) g_malloc (initial_max);
	sdata.max = initial_max;
	sdata.len = 0;

	if (! poppler_attachment_save_to_callback (attachment,
						   attachment_save_to_buffer_callback,
						   &sdata,
						   error)) {
		g_free (sdata.buffer);
		return FALSE;
	}

	*buffer = sdata.buffer;
	*buffer_size = sdata.len;
	
	return TRUE;
}

static GList *
pdf_document_get_attachments (EvDocument *document)
{
	PdfDocument *pdf_document;
	GList *attachments;
	GList *list;
	GList *retval = NULL;

	pdf_document = PDF_DOCUMENT (document);

	if (!pdf_document_has_attachments (document))
		return NULL;

	attachments = poppler_document_get_attachments (pdf_document->document);
	
	for (list = attachments; list; list = list->next) {
		PopplerAttachment *attachment;
		EvAttachment *ev_attachment;
		gchar *data = NULL;
		gsize size;
		GError *error = NULL;

		attachment = (PopplerAttachment *) list->data;

		if (attachment_save_to_buffer (attachment, &data, &size, &error)) {
			ev_attachment = ev_attachment_new (attachment->name,
							   attachment->description,
							   attachment->mtime,
							   attachment->ctime,
							   size, data);
			
			retval = g_list_prepend (retval, ev_attachment);
		} else {
			if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);

				g_free (data);
			}
		}

		g_object_unref (attachment);
	}

	return g_list_reverse (retval);
}

static cairo_surface_t *
pdf_page_render (PopplerPage     *page,
		 gint             width,
		 gint             height,
		 EvRenderContext *rc)
{
	cairo_surface_t *surface;

#ifdef HAVE_POPPLER_PAGE_RENDER
	cairo_t *cr;

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					      width, height);
	cr = cairo_create (surface);

	switch (rc->rotation) {
	        case 90:
			cairo_translate (cr, width, 0);
			break;
	        case 180:
			cairo_translate (cr, width, height);
			break;
	        case 270:
			cairo_translate (cr, 0, height);
			break;
	        default:
			cairo_translate (cr, 0, 0);
	}
	cairo_scale (cr, rc->scale, rc->scale);
	cairo_rotate (cr, rc->rotation * G_PI / 180.0);
	poppler_page_render (page, cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
	cairo_set_source_rgb (cr, 1., 1., 1.);
	cairo_paint (cr);

	cairo_destroy (cr);
#else /* HAVE_POPPLER_PAGE_RENDER */
	GdkPixbuf *pixbuf;
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 FALSE, 8,
				 width, height);

	poppler_page_render_to_pixbuf (page,
				       0, 0,
				       width, height,
				       rc->scale,
				       rc->rotation,
				       pixbuf);
	surface = ev_document_misc_surface_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
#endif /* HAVE_POPPLER_PAGE_RENDER */

	return surface;	
}

static cairo_surface_t *
pdf_document_render (EvDocument      *document,
		     EvRenderContext *rc)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	double width_points, height_points;
	gint width, height;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	poppler_page_get_size (poppler_page,
			       &width_points, &height_points);
	
	if (rc->rotation == 90 || rc->rotation == 270) {
		width = (int) ((height_points * rc->scale) + 0.5);
		height = (int) ((width_points * rc->scale) + 0.5);
	} else {
		width = (int) ((width_points * rc->scale) + 0.5);
		height = (int) ((height_points * rc->scale) + 0.5);
	}
	
	return pdf_page_render (poppler_page,
				width, height, rc);
}

/* EvDocumentSecurity */

static gboolean
pdf_document_has_document_security (EvDocumentSecurity *document_security)
{
	/* FIXME: do we really need to have this? */
	return FALSE;
}

static void
pdf_document_set_password (EvDocumentSecurity *document_security,
			   const char         *password)
{
	PdfDocument *document = PDF_DOCUMENT (document_security);

	if (document->password)
		g_free (document->password);

	document->password = g_strdup (password);
}


/* reference:
http://www.pdfa.org/lib/exe/fetch.php?id=pdfa%3Aen%3Atechdoc&cache=cache&media=pdfa:techdoc:tn0001_pdfa-1_and_namespaces_2008-03-18.pdf */
static char *
pdf_document_get_format_from_metadata (const char *metadata)
{
	xmlDocPtr doc;
	xmlXPathContextPtr xpathCtx;
	xmlXPathObjectPtr xpathObj;
	xmlChar *part = NULL;
	xmlChar *conf = NULL;
	char *result = NULL;
	int i;

	doc = xmlParseMemory (metadata, strlen (metadata));
	if (doc == NULL)
		return NULL;      /* invalid xml metadata */

	xpathCtx = xmlXPathNewContext (doc);
	if (xpathCtx == NULL) {
		xmlFreeDoc (doc);
		return NULL;      /* invalid xpath context */
	}

	/* add pdf/a namespaces */
	xmlXPathRegisterNs (xpathCtx, BAD_CAST "x", BAD_CAST "adobe:ns:meta/");
	xmlXPathRegisterNs (xpathCtx, BAD_CAST "rdf", BAD_CAST "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	xmlXPathRegisterNs (xpathCtx, BAD_CAST "pdfaid", BAD_CAST "http://www.aiim.org/pdfa/ns/id/");

	/* reads pdf/a part */
	/* first syntax: child node */
	xpathObj = xmlXPathEvalExpression (BAD_CAST "/x:xmpmeta/rdf:RDF/rdf:Description/pdfaid:part", xpathCtx);
	if (xpathObj != NULL) {
		if (xpathObj->nodesetval != NULL && xpathObj->nodesetval->nodeNr != 0)
			part = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[0]);

		xmlXPathFreeObject (xpathObj);
	}
	if (part == NULL) {
		/* second syntax: attribute */
		xpathObj = xmlXPathEvalExpression (BAD_CAST "/x:xmpmeta/rdf:RDF/rdf:Description/@pdfaid:part", xpathCtx);
		if (xpathObj != NULL) {
			if (xpathObj->nodesetval != NULL && xpathObj->nodesetval->nodeNr != 0)
				part = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[0]);

			xmlXPathFreeObject (xpathObj);
		}
	}

	/* reads pdf/a conformance */
	/* first syntax: child node */
	xpathObj = xmlXPathEvalExpression (BAD_CAST "/x:xmpmeta/rdf:RDF/rdf:Description/pdfaid:conformance", xpathCtx);
	if (xpathObj != NULL) {
		if (xpathObj->nodesetval != NULL && xpathObj->nodesetval->nodeNr != 0)
			conf = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[0]);

		xmlXPathFreeObject (xpathObj);
	}
	if (conf == NULL) {
		/* second syntax: attribute */
		xpathObj = xmlXPathEvalExpression (BAD_CAST "/x:xmpmeta/rdf:RDF/rdf:Description/@pdfaid:conformance", xpathCtx);
		if (xpathObj != NULL) {
			if (xpathObj->nodesetval != NULL && xpathObj->nodesetval->nodeNr != 0)
				conf = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[0]);

			xmlXPathFreeObject (xpathObj);
		}
	}

	if (part != NULL && conf != NULL) {
		/* makes conf lowercase */
		for (i = 0; conf[i]; i++)
			conf[i] = g_ascii_tolower (conf[i]);

		/* return buffer */
		result = g_strdup_printf ("PDF/A - %s%s", part, conf);
	}

	/* Cleanup */
	xmlFree (part);
	xmlFree (conf);
	xmlXPathFreeContext (xpathCtx);
	xmlFreeDoc (doc);

	return result;
}


static EvDocumentInfo *
pdf_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	PopplerPageLayout layout;
	PopplerPageMode mode;
	PopplerViewerPreferences view_prefs;
	PopplerPermissions permissions;
	EvPage *page;
	char *metadata;
	char *fmt;

	info = g_new0 (EvDocumentInfo, 1);

	info->fields_mask = EV_DOCUMENT_INFO_TITLE |
			    EV_DOCUMENT_INFO_FORMAT |
			    EV_DOCUMENT_INFO_AUTHOR |
			    EV_DOCUMENT_INFO_SUBJECT |
			    EV_DOCUMENT_INFO_KEYWORDS |
			    EV_DOCUMENT_INFO_LAYOUT |
			    EV_DOCUMENT_INFO_START_MODE |
		            EV_DOCUMENT_INFO_PERMISSIONS |
			    EV_DOCUMENT_INFO_UI_HINTS |
			    EV_DOCUMENT_INFO_CREATOR |
			    EV_DOCUMENT_INFO_PRODUCER |
			    EV_DOCUMENT_INFO_CREATION_DATE |
			    EV_DOCUMENT_INFO_MOD_DATE |
			    EV_DOCUMENT_INFO_LINEARIZED |
			    EV_DOCUMENT_INFO_N_PAGES |
			    EV_DOCUMENT_INFO_SECURITY | 
		            EV_DOCUMENT_INFO_PAPER_SIZE;

	g_object_get (PDF_DOCUMENT (document)->document,
		      "title", &(info->title),
		      "format", &(info->format),
		      "author", &(info->author),
		      "subject", &(info->subject),
		      "keywords", &(info->keywords),
		      "page-mode", &mode,
		      "page-layout", &layout,
		      "viewer-preferences", &view_prefs,
		      "permissions", &permissions,
		      "creator", &(info->creator),
		      "producer", &(info->producer),
		      "creation-date", &(info->creation_date),
		      "mod-date", &(info->modified_date),
		      "linearized", &(info->linearized),
		      "metadata", &metadata,
		      NULL);

	if (metadata != NULL) {
		fmt = pdf_document_get_format_from_metadata(metadata);
		if (fmt != NULL) {
			g_free (info->format);
			info->format = fmt;
		}
		g_free (metadata);
	}

	info->n_pages = ev_document_get_n_pages (document);

	if (info->n_pages > 0) {
		page = ev_document_get_page (document, 0);
		ev_document_get_page_size (document, page,
					   &(info->paper_width),
					   &(info->paper_height));
		g_object_unref (page);
		

		// Convert to mm.
		info->paper_width = info->paper_width / 72.0f * 25.4f;
		info->paper_height = info->paper_height / 72.0f * 25.4f;
	}

	switch (layout) {
		case POPPLER_PAGE_LAYOUT_SINGLE_PAGE:
			info->layout = EV_DOCUMENT_LAYOUT_SINGLE_PAGE;
			break;
		case POPPLER_PAGE_LAYOUT_ONE_COLUMN:
			info->layout = EV_DOCUMENT_LAYOUT_ONE_COLUMN;
			break;
		case POPPLER_PAGE_LAYOUT_TWO_COLUMN_LEFT:
			info->layout = EV_DOCUMENT_LAYOUT_TWO_COLUMN_LEFT;
			break;
		case POPPLER_PAGE_LAYOUT_TWO_COLUMN_RIGHT:
			info->layout = EV_DOCUMENT_LAYOUT_TWO_COLUMN_RIGHT;
		case POPPLER_PAGE_LAYOUT_TWO_PAGE_LEFT:
			info->layout = EV_DOCUMENT_LAYOUT_TWO_PAGE_LEFT;
			break;
		case POPPLER_PAGE_LAYOUT_TWO_PAGE_RIGHT:
			info->layout = EV_DOCUMENT_LAYOUT_TWO_PAGE_RIGHT;
			break;
	        default:
			break;
	}

	switch (mode) {
		case POPPLER_PAGE_MODE_NONE:
			info->mode = EV_DOCUMENT_MODE_NONE;
			break;
		case POPPLER_PAGE_MODE_USE_THUMBS:
			info->mode = EV_DOCUMENT_MODE_USE_THUMBS;
			break;
		case POPPLER_PAGE_MODE_USE_OC:
			info->mode = EV_DOCUMENT_MODE_USE_OC;
			break;
		case POPPLER_PAGE_MODE_FULL_SCREEN:
			info->mode = EV_DOCUMENT_MODE_FULL_SCREEN;
			break;
		case POPPLER_PAGE_MODE_USE_ATTACHMENTS:
			info->mode = EV_DOCUMENT_MODE_USE_ATTACHMENTS;
	        default:
			break;
	}

	info->ui_hints = 0;
	if (view_prefs & POPPLER_VIEWER_PREFERENCES_HIDE_TOOLBAR) {
		info->ui_hints |= EV_DOCUMENT_UI_HINT_HIDE_TOOLBAR;
	}
	if (view_prefs & POPPLER_VIEWER_PREFERENCES_HIDE_MENUBAR) {
		info->ui_hints |= EV_DOCUMENT_UI_HINT_HIDE_MENUBAR;
	}
	if (view_prefs & POPPLER_VIEWER_PREFERENCES_HIDE_WINDOWUI) {
		info->ui_hints |= EV_DOCUMENT_UI_HINT_HIDE_WINDOWUI;
	}
	if (view_prefs & POPPLER_VIEWER_PREFERENCES_FIT_WINDOW) {
		info->ui_hints |= EV_DOCUMENT_UI_HINT_FIT_WINDOW;
	}
	if (view_prefs & POPPLER_VIEWER_PREFERENCES_CENTER_WINDOW) {
		info->ui_hints |= EV_DOCUMENT_UI_HINT_CENTER_WINDOW;
	}
	if (view_prefs & POPPLER_VIEWER_PREFERENCES_DISPLAY_DOC_TITLE) {
		info->ui_hints |= EV_DOCUMENT_UI_HINT_DISPLAY_DOC_TITLE;
	}
	if (view_prefs & POPPLER_VIEWER_PREFERENCES_DIRECTION_RTL) {
		info->ui_hints |=  EV_DOCUMENT_UI_HINT_DIRECTION_RTL;
	}

	info->permissions = 0;
	if (permissions & POPPLER_PERMISSIONS_OK_TO_PRINT) {
		info->permissions |= EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT;
	}
	if (permissions & POPPLER_PERMISSIONS_OK_TO_MODIFY) {
		info->permissions |= EV_DOCUMENT_PERMISSIONS_OK_TO_MODIFY;
	}
	if (permissions & POPPLER_PERMISSIONS_OK_TO_COPY) {
		info->permissions |= EV_DOCUMENT_PERMISSIONS_OK_TO_COPY;
	}
	if (permissions & POPPLER_PERMISSIONS_OK_TO_ADD_NOTES) {
		info->permissions |= EV_DOCUMENT_PERMISSIONS_OK_TO_ADD_NOTES;
	}

	if (ev_document_security_has_document_security (EV_DOCUMENT_SECURITY (document))) {
		/* translators: this is the document security state */
		info->security = g_strdup (_("Yes"));
	} else {
		/* translators: this is the document security state */
		info->security = g_strdup (_("No"));
	}

	return info;
}

static void
pdf_document_document_iface_init (EvDocumentIface *iface)
{
	iface->save = pdf_document_save;
	iface->load = pdf_document_load;
	iface->get_n_pages = pdf_document_get_n_pages;
	iface->get_page = pdf_document_get_page;
	iface->get_page_size = pdf_document_get_page_size;
	iface->get_page_label = pdf_document_get_page_label;
	iface->has_attachments = pdf_document_has_attachments;
	iface->get_attachments = pdf_document_get_attachments;
	iface->render = pdf_document_render;
	iface->get_info = pdf_document_get_info;
};

static void
pdf_document_security_iface_init (EvDocumentSecurityIface *iface)
{
	iface->has_document_security = pdf_document_has_document_security;
	iface->set_password = pdf_document_set_password;
}

static gdouble
pdf_document_fonts_get_progress (EvDocumentFonts *document_fonts)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_fonts);
	int n_pages;

        n_pages = pdf_document_get_n_pages (EV_DOCUMENT (pdf_document));

	return (double)pdf_document->fonts_scanned_pages / (double)n_pages;
}

static gboolean
pdf_document_fonts_scan (EvDocumentFonts *document_fonts,
			 int              n_pages)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_fonts);
	gboolean result;

	g_return_val_if_fail (PDF_IS_DOCUMENT (document_fonts), FALSE);

	if (pdf_document->font_info == NULL) { 
		pdf_document->font_info = poppler_font_info_new (pdf_document->document);
	}

	if (pdf_document->fonts_iter) {
		poppler_fonts_iter_free (pdf_document->fonts_iter);
	}

	pdf_document->fonts_scanned_pages += n_pages;

	result = poppler_font_info_scan (pdf_document->font_info, n_pages,
				         &pdf_document->fonts_iter);
	if (!result) {
		pdf_document->fonts_scanned_pages = 0;
		poppler_font_info_free (pdf_document->font_info);
		pdf_document->font_info = NULL;	
	}

	return result;
}

static const char *
font_type_to_string (PopplerFontType type)
{
	switch (type) {
	        case POPPLER_FONT_TYPE_TYPE1:
			return _("Type 1");
	        case POPPLER_FONT_TYPE_TYPE1C:
			return _("Type 1C");
	        case POPPLER_FONT_TYPE_TYPE3:
			return _("Type 3");
	        case POPPLER_FONT_TYPE_TRUETYPE:
			return _("TrueType");
	        case POPPLER_FONT_TYPE_CID_TYPE0:
			return _("Type 1 (CID)");
	        case POPPLER_FONT_TYPE_CID_TYPE0C:
			return _("Type 1C (CID)");
	        case POPPLER_FONT_TYPE_CID_TYPE2:
			return _("TrueType (CID)");
	        default:
			return _("Unknown font type");
	}
}

static void
pdf_document_fonts_fill_model (EvDocumentFonts *document_fonts,
			       GtkTreeModel    *model)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_fonts);
	PopplerFontsIter *iter = pdf_document->fonts_iter;

	g_return_if_fail (PDF_IS_DOCUMENT (document_fonts));

	if (!iter)
		return;

	do {
		GtkTreeIter list_iter;
		const char *name;
		const char *type;
		const char *embedded;
		char *details;
		
		name = poppler_fonts_iter_get_name (iter);

		if (name == NULL) {
			name = _("No name");
		}

		type = font_type_to_string (
			poppler_fonts_iter_get_font_type (iter));

		if (poppler_fonts_iter_is_embedded (iter)) {
			if (poppler_fonts_iter_is_subset (iter))
				embedded = _("Embedded subset");
			else
				embedded = _("Embedded");
		} else {
			embedded = _("Not embedded");
		}

		details = g_markup_printf_escaped ("%s\n%s", type, embedded);

		gtk_list_store_append (GTK_LIST_STORE (model), &list_iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &list_iter,
				    EV_DOCUMENT_FONTS_COLUMN_NAME, name,
				    EV_DOCUMENT_FONTS_COLUMN_DETAILS, details,
				    -1);

		g_free (details);
	} while (poppler_fonts_iter_next (iter));
}

static void
pdf_document_document_fonts_iface_init (EvDocumentFontsIface *iface)
{
	iface->fill_model = pdf_document_fonts_fill_model;
	iface->scan = pdf_document_fonts_scan;
	iface->get_progress = pdf_document_fonts_get_progress;
}

static gboolean
pdf_document_links_has_document_links (EvDocumentLinks *document_links)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_links);
	PopplerIndexIter *iter;

	g_return_val_if_fail (PDF_IS_DOCUMENT (document_links), FALSE);

	iter = poppler_index_iter_new (pdf_document->document);
	if (iter == NULL)
		return FALSE;
	poppler_index_iter_free (iter);

	return TRUE;
}

static EvLinkDest *
ev_link_dest_from_dest (PdfDocument *pdf_document,
			PopplerDest *dest)
{
	EvLinkDest *ev_dest = NULL;
	const char *unimplemented_dest = NULL;

	g_assert (dest != NULL);

	switch (dest->type) {
	        case POPPLER_DEST_XYZ: {
			PopplerPage *poppler_page;
			double height;

			poppler_page = poppler_document_get_page (pdf_document->document,
								  MAX (0, dest->page_num - 1));
			poppler_page_get_size (poppler_page, NULL, &height);
			ev_dest = ev_link_dest_new_xyz (dest->page_num - 1,
							dest->left,
							height - MIN (height, dest->top),
							dest->zoom,
							dest->change_left,
							dest->change_top,
							dest->change_zoom);
			g_object_unref (poppler_page);
		}
			break;
	        case POPPLER_DEST_FITB:
		case POPPLER_DEST_FIT:
			ev_dest = ev_link_dest_new_fit (dest->page_num - 1);
			break;
		case POPPLER_DEST_FITBH:
	        case POPPLER_DEST_FITH: {
			PopplerPage *poppler_page;
			double height;

			poppler_page = poppler_document_get_page (pdf_document->document,
								  MAX (0, dest->page_num - 1));
			poppler_page_get_size (poppler_page, NULL, &height);
			ev_dest = ev_link_dest_new_fith (dest->page_num - 1,
							 height - MIN (height, dest->top),
							 dest->change_top);
			g_object_unref (poppler_page);
		}
			break;
		case POPPLER_DEST_FITBV:
	        case POPPLER_DEST_FITV:
			ev_dest = ev_link_dest_new_fitv (dest->page_num - 1,
							 dest->left,
							 dest->change_left);
			break;
	        case POPPLER_DEST_FITR: {
			PopplerPage *poppler_page;
			double height;

			poppler_page = poppler_document_get_page (pdf_document->document,
								  MAX (0, dest->page_num - 1));
			poppler_page_get_size (poppler_page, NULL, &height);
			ev_dest = ev_link_dest_new_fitr (dest->page_num - 1,
							 dest->left,
							 height - MIN (height, dest->bottom),
							 dest->right,
							 height - MIN (height, dest->top));
			g_object_unref (poppler_page);
		}
			break;
	        case POPPLER_DEST_NAMED:
			ev_dest = ev_link_dest_new_named (dest->named_dest);
			break;
	        case POPPLER_DEST_UNKNOWN:
			unimplemented_dest = "POPPLER_DEST_UNKNOWN";
			break;
	}

	if (unimplemented_dest) {
		g_warning ("Unimplemented destination: %s, please post a "
		           "bug report in Evince bugzilla "
		           "(http://bugzilla.gnome.org) with a testcase.",
			   unimplemented_dest);
	}

	if (!ev_dest)
		ev_dest = ev_link_dest_new_page (dest->page_num - 1);
	
	return ev_dest;
}

static EvLink *
ev_link_from_action (PdfDocument   *pdf_document,
		     PopplerAction *action)
{
	EvLink       *link = NULL;
	EvLinkAction *ev_action = NULL;
	const char   *unimplemented_action = NULL;

	switch (action->type) {
	        case POPPLER_ACTION_NONE:
			break;
	        case POPPLER_ACTION_GOTO_DEST: {
			EvLinkDest *dest;
			
			dest = ev_link_dest_from_dest (pdf_document, action->goto_dest.dest);
			ev_action = ev_link_action_new_dest (dest);
		}
			break;
	        case POPPLER_ACTION_GOTO_REMOTE: {
			EvLinkDest *dest;
			
			dest = ev_link_dest_from_dest (pdf_document, action->goto_remote.dest);
			ev_action = ev_link_action_new_remote (dest, 
							       action->goto_remote.file_name);
			
		}
			break;
	        case POPPLER_ACTION_LAUNCH:
			ev_action = ev_link_action_new_launch (action->launch.file_name,
							       action->launch.params);
			break;
	        case POPPLER_ACTION_URI:
			ev_action = ev_link_action_new_external_uri (action->uri.uri);
			break;
	        case POPPLER_ACTION_NAMED:
			ev_action = ev_link_action_new_named (action->named.named_dest);
			break;
	        case POPPLER_ACTION_MOVIE:
			unimplemented_action = "POPPLER_ACTION_MOVIE";
			break;
	        case POPPLER_ACTION_UNKNOWN:
			unimplemented_action = "POPPLER_ACTION_UNKNOWN";
	}
	
	if (unimplemented_action) {
		g_warning ("Unimplemented action: %s, please post a bug report "
			   "in Evince bugzilla (http://bugzilla.gnome.org) "
			   "with a testcase.", unimplemented_action);
	}
	
	link = ev_link_new (action->any.title, ev_action);
	
	return link;	
}

static void
build_tree (PdfDocument      *pdf_document,
	    GtkTreeModel     *model,
	    GtkTreeIter      *parent,
	    PopplerIndexIter *iter)
{
	
	do {
		GtkTreeIter tree_iter;
		PopplerIndexIter *child;
		PopplerAction *action;
		EvLink *link = NULL;
		gboolean expand;
		char *title_markup;
		
		action = poppler_index_iter_get_action (iter);
		expand = poppler_index_iter_is_open (iter);

		if (!action)
			continue;

		switch (action->type) {
		        case POPPLER_ACTION_GOTO_DEST: {
				/* For bookmarks, solve named destinations */
				if (action->goto_dest.dest->type == POPPLER_DEST_NAMED) {
					PopplerDest *dest;
					EvLinkDest *ev_dest = NULL;
					EvLinkAction *ev_action;
					
					dest = poppler_document_find_dest (pdf_document->document,
									   action->goto_dest.dest->named_dest);
					if (!dest) {
						link = ev_link_from_action (pdf_document, action);
						break;
					}
					
					ev_dest = ev_link_dest_from_dest (pdf_document, dest);
					poppler_dest_free (dest);
					
					ev_action = ev_link_action_new_dest (ev_dest);
					link = ev_link_new (action->any.title, ev_action);
				} else {
					link = ev_link_from_action (pdf_document, action);
				}
			}
				break;
		        default:
				link = ev_link_from_action (pdf_document, action);
				break;
		}
		
		if (!link || strlen (ev_link_get_title (link)) <= 0) {
			poppler_action_free (action);
			if (link)
				g_object_unref (link);
			
			continue;
		}

		gtk_tree_store_append (GTK_TREE_STORE (model), &tree_iter, parent);
		title_markup = g_markup_escape_text (ev_link_get_title (link), -1);
		
		gtk_tree_store_set (GTK_TREE_STORE (model), &tree_iter,
				    EV_DOCUMENT_LINKS_COLUMN_MARKUP, title_markup,
				    EV_DOCUMENT_LINKS_COLUMN_LINK, link,
				    EV_DOCUMENT_LINKS_COLUMN_EXPAND, expand,
				    -1);
		
		g_free (title_markup);
		g_object_unref (link);
		
		child = poppler_index_iter_get_child (iter);
		if (child)
			build_tree (pdf_document, model, &tree_iter, child);
		poppler_index_iter_free (child);
		poppler_action_free (action);
		
	} while (poppler_index_iter_next (iter));
}

static GtkTreeModel *
pdf_document_links_get_links_model (EvDocumentLinks *document_links)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_links);
	GtkTreeModel *model = NULL;
	PopplerIndexIter *iter;
	
	g_return_val_if_fail (PDF_IS_DOCUMENT (document_links), NULL);

	iter = poppler_index_iter_new (pdf_document->document);
	/* Create the model if we have items*/
	if (iter != NULL) {
		model = (GtkTreeModel *) gtk_tree_store_new (EV_DOCUMENT_LINKS_COLUMN_NUM_COLUMNS,
							     G_TYPE_STRING,
							     G_TYPE_OBJECT,
							     G_TYPE_BOOLEAN,
							     G_TYPE_STRING);
		build_tree (pdf_document, model, NULL, iter);
		poppler_index_iter_free (iter);
	}
	
	return model;
}

static GList *
pdf_document_links_get_links (EvDocumentLinks *document_links,
			      gint             page)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GList *retval = NULL;
	GList *mapping_list;
	GList *list;
	double height;

	pdf_document = PDF_DOCUMENT (document_links);
	poppler_page = poppler_document_get_page (pdf_document->document,
						  page);
	mapping_list = poppler_page_get_link_mapping (poppler_page);
	poppler_page_get_size (poppler_page, NULL, &height);

	for (list = mapping_list; list; list = list->next) {
		PopplerLinkMapping *link_mapping;
		EvMapping *ev_link_mapping;

		link_mapping = (PopplerLinkMapping *)list->data;
		ev_link_mapping = g_new (EvMapping, 1);
		ev_link_mapping->data = ev_link_from_action (pdf_document,
							     link_mapping->action);
		ev_link_mapping->area.x1 = link_mapping->area.x1;
		ev_link_mapping->area.x2 = link_mapping->area.x2;
		/* Invert this for X-style coordinates */
		ev_link_mapping->area.y1 = height - link_mapping->area.y2;
		ev_link_mapping->area.y2 = height - link_mapping->area.y1;

		retval = g_list_prepend (retval, ev_link_mapping);
	}

	poppler_page_free_link_mapping (mapping_list);
	g_object_unref (poppler_page);

	return g_list_reverse (retval);
}

static EvLinkDest *
pdf_document_links_find_link_dest (EvDocumentLinks  *document_links,
				   const gchar      *link_name)
{
	PdfDocument *pdf_document;
	PopplerDest *dest;
	EvLinkDest *ev_dest = NULL;

	pdf_document = PDF_DOCUMENT (document_links);
	dest = poppler_document_find_dest (pdf_document->document,
					   link_name);
	if (dest) {
		ev_dest = ev_link_dest_from_dest (pdf_document, dest);
		poppler_dest_free (dest);
	}

	return ev_dest;
}

static void
pdf_document_document_links_iface_init (EvDocumentLinksIface *iface)
{
	iface->has_document_links = pdf_document_links_has_document_links;
	iface->get_links_model = pdf_document_links_get_links_model;
	iface->get_links = pdf_document_links_get_links;
	iface->find_link_dest = pdf_document_links_find_link_dest;
}

static GList *
pdf_document_images_get_image_mapping (EvDocumentImages *document_images,
				       gint              page)
{
	GList *retval = NULL;
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GList *mapping_list;
	GList *list;

	pdf_document = PDF_DOCUMENT (document_images);
	poppler_page = poppler_document_get_page (pdf_document->document, page);
	mapping_list = poppler_page_get_image_mapping (poppler_page);

	for (list = mapping_list; list; list = list->next) {
		PopplerImageMapping *image_mapping;
		EvMapping *ev_image_mapping;

		image_mapping = (PopplerImageMapping *)list->data;

		ev_image_mapping = g_new (EvMapping, 1);
		
		ev_image_mapping->data = ev_image_new (page, image_mapping->image_id);
		ev_image_mapping->area.x1 = image_mapping->area.x1;
		ev_image_mapping->area.y1 = image_mapping->area.y1;
		ev_image_mapping->area.x2 = image_mapping->area.x2;
		ev_image_mapping->area.y2 = image_mapping->area.y2;

		retval = g_list_prepend (retval, ev_image_mapping);
	}

	poppler_page_free_image_mapping (mapping_list);
	g_object_unref (poppler_page);

	return g_list_reverse (retval);
}

GdkPixbuf *
pdf_document_images_get_image (EvDocumentImages *document_images,
			       EvImage          *image)
{
	GdkPixbuf       *retval = NULL;
#ifdef HAVE_POPPLER_PAGE_GET_IMAGE
	PdfDocument     *pdf_document;
	PopplerPage     *poppler_page;
	cairo_surface_t *surface;

	pdf_document = PDF_DOCUMENT (document_images);
	poppler_page = poppler_document_get_page (pdf_document->document,
						  ev_image_get_page (image));

	surface = poppler_page_get_image (poppler_page, ev_image_get_id (image));
	if (surface) {
		retval = ev_document_misc_pixbuf_from_surface (surface);
		cairo_surface_destroy (surface);
	}

	g_object_unref (poppler_page);
#endif
	return retval;
}

static void
pdf_document_document_images_iface_init (EvDocumentImagesIface *iface)
{
	iface->get_image_mapping = pdf_document_images_get_image_mapping;
	iface->get_image = pdf_document_images_get_image;
}

static GdkPixbuf *
make_thumbnail_for_page (PopplerPage     *poppler_page,
			 EvRenderContext *rc,
			 gint             width,
			 gint             height)
{
	GdkPixbuf *pixbuf;

#ifdef POPPLER_WITH_GDK
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				 width, height);
	gdk_pixbuf_fill (pixbuf, 0xffffffff);

	ev_document_fc_mutex_lock ();
	poppler_page_render_to_pixbuf (poppler_page, 0, 0,
				       width, height,
				       rc->scale, rc->rotation, pixbuf);
	ev_document_fc_mutex_unlock ();
#else
	cairo_surface_t *surface;

	ev_document_fc_mutex_lock ();
	surface = pdf_page_render (poppler_page, width, height, rc);
	ev_document_fc_mutex_unlock ();
	
	pixbuf = ev_document_misc_pixbuf_from_surface (surface);
	cairo_surface_destroy (surface);
#endif /* POPPLER_WITH_GDK */

	return pixbuf;
}

static GdkPixbuf *
pdf_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document_thumbnails,
				       EvRenderContext      *rc, 
				       gboolean              border)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_thumbnails);
	PopplerPage *poppler_page;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *border_pixbuf;
	gint width, height;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	pdf_document_thumbnails_get_dimensions (EV_DOCUMENT_THUMBNAILS (pdf_document),
						rc, &width, &height);
	
#ifdef POPPLER_WITH_GDK
	pixbuf = poppler_page_get_thumbnail_pixbuf (poppler_page);
#else
	cairo_surface_t *surface;
	
	surface = poppler_page_get_thumbnail (poppler_page);
	if (surface) {
		pixbuf = ev_document_misc_pixbuf_from_surface (surface);
		cairo_surface_destroy (surface);
	}
#endif /* POPPLER_WITH_GDK */

	if (pixbuf != NULL) {
		int thumb_width = (rc->rotation == 90 || rc->rotation == 270) ?
			gdk_pixbuf_get_height (pixbuf) :
			gdk_pixbuf_get_width (pixbuf);

		if (thumb_width == width) {
			GdkPixbuf *rotated_pixbuf;

			rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf,
								   (GdkPixbufRotation) (360 - rc->rotation));
			g_object_unref (pixbuf);
			pixbuf = rotated_pixbuf;
		} else {
			/* The provided thumbnail has a different size */
			g_object_unref (pixbuf);
			pixbuf = make_thumbnail_for_page (poppler_page, rc, width, height);
		}
	} else {
		/* There is no provided thumbnail. We need to make one. */
		pixbuf = make_thumbnail_for_page (poppler_page, rc, width, height);
	}

        if (border && pixbuf) {
		border_pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, pixbuf);
		g_object_unref (pixbuf);
		pixbuf = border_pixbuf;
	}		

	return pixbuf;
}

static void
pdf_document_thumbnails_get_dimensions (EvDocumentThumbnails *document_thumbnails,
					EvRenderContext      *rc,
					gint                 *width,
					gint                 *height)
{
	double page_width, page_height;
	
	poppler_page_get_size (POPPLER_PAGE (rc->page->backend_page),
			       &page_width, &page_height);
	
	*width = MAX ((gint)(page_width * rc->scale + 0.5), 1);
	*height = MAX ((gint)(page_height * rc->scale + 0.5), 1);

	if (rc->rotation == 90 || rc->rotation == 270) {
		gint  temp;

		temp = *width;
		*width = *height;
		*height = temp;
	}
}

static void
pdf_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = pdf_document_thumbnails_get_thumbnail;
	iface->get_dimensions = pdf_document_thumbnails_get_dimensions;
}


static GList *
pdf_document_find_find_text (EvDocumentFind *document_find,
			     EvPage         *page,
			     const gchar    *text,
			     gboolean        case_sensitive)
{
	GList *matches, *l;
	PopplerPage *poppler_page;
	gdouble height;
	
	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);
	g_return_val_if_fail (text != NULL, NULL);

	poppler_page = POPPLER_PAGE (page->backend_page);
	
	matches = poppler_page_find_text (poppler_page, text);
	if (!matches)
		return NULL;

	poppler_page_get_size (poppler_page, NULL, &height);
	for (l = matches; l && l->data; l = g_list_next (l)) {
		PopplerRectangle *rect = (PopplerRectangle *)l->data;
		gdouble           tmp;

		tmp = rect->y1;
		rect->y1 = height - rect->y2;
		rect->y2 = height - tmp;
	}
	
	return matches;
}

static void
pdf_document_find_iface_init (EvDocumentFindIface *iface)
{
        iface->find_text = pdf_document_find_find_text;
}

static void
pdf_print_context_free (PdfPrintContext *ctx)
{
	if (!ctx)
		return;

#ifdef HAVE_CAIRO_PRINT
	if (ctx->cr) {
		cairo_destroy (ctx->cr);
		ctx->cr = NULL;
	}
#else
	if (ctx->ps_file) {
		poppler_ps_file_free (ctx->ps_file);
		ctx->ps_file = NULL;
	}
#endif
	g_free (ctx);
}

static void
pdf_document_file_exporter_begin (EvFileExporter        *exporter,
				  EvFileExporterContext *fc)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);
	PdfPrintContext *ctx;
#ifdef HAVE_CAIRO_PRINT
	gdouble width, height;
	cairo_surface_t *surface = NULL;
#endif
	
	if (pdf_document->print_ctx)
		pdf_print_context_free (pdf_document->print_ctx);
	pdf_document->print_ctx = g_new0 (PdfPrintContext, 1);
	ctx = pdf_document->print_ctx;
	ctx->format = fc->format;
	
#ifdef HAVE_CAIRO_PRINT
	ctx->pages_per_sheet = CLAMP (fc->pages_per_sheet, 1, 16);

	ctx->paper_width = fc->paper_width;
	ctx->paper_height = fc->paper_height;
	
	switch (fc->pages_per_sheet) {
	        default:
	        case 1:
			ctx->pages_x = 1;
			ctx->pages_y = 1;
			break;
	        case 2:
			ctx->pages_x = 1;
			ctx->pages_y = 2;
			break;
	        case 4:
			ctx->pages_x = 2;
			ctx->pages_y = 2;
			break;
	        case 6:
			ctx->pages_x = 2;
			ctx->pages_y = 3;
			break;
	        case 9:
			ctx->pages_x = 3;
			ctx->pages_y = 3;
			break;
	        case 16:
			ctx->pages_x = 4;
			ctx->pages_y = 4;
			break;
	}

	ctx->pages_printed = 0;
	
	switch (fc->format) {
	        case EV_FILE_FORMAT_PS:
#ifdef HAVE_CAIRO_PS
			surface = cairo_ps_surface_create (fc->filename, fc->paper_width, fc->paper_height);
#endif
			break;
	        case EV_FILE_FORMAT_PDF:
#ifdef HAVE_CAIRO_PDF
			surface = cairo_pdf_surface_create (fc->filename, fc->paper_width, fc->paper_height);
#endif
			break;
	        default:
			g_assert_not_reached ();
	}

	ctx->cr = cairo_create (surface);
	cairo_surface_destroy (surface);

#else /* HAVE_CAIRO_PRINT */
	if (ctx->format == EV_FILE_FORMAT_PS) {
		ctx->ps_file = poppler_ps_file_new (pdf_document->document,
						    fc->filename, fc->first_page,
						    fc->last_page - fc->first_page + 1);
		poppler_ps_file_set_paper_size (ctx->ps_file, fc->paper_width, fc->paper_height);
		poppler_ps_file_set_duplex (ctx->ps_file, fc->duplex);
	}
#endif /* HAVE_CAIRO_PRINT */
}

static void
pdf_document_file_exporter_begin_page (EvFileExporter *exporter)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);
	PdfPrintContext *ctx = pdf_document->print_ctx;
	
	g_return_if_fail (pdf_document->print_ctx != NULL);

	ctx->pages_printed = 0;
	
#ifdef HAVE_CAIRO_PRINT
	if (ctx->paper_width > ctx->paper_height) {
		if (ctx->format == EV_FILE_FORMAT_PS) {
			cairo_ps_surface_set_size (cairo_get_target (ctx->cr),
						   ctx->paper_height,
						   ctx->paper_width);
		} else if (ctx->format == EV_FILE_FORMAT_PDF) {
			cairo_pdf_surface_set_size (cairo_get_target (ctx->cr),
						    ctx->paper_height,
						    ctx->paper_width);
		}
	}
#endif /* HAVE_CAIRO_PRINT */
}

static void
pdf_document_file_exporter_do_page (EvFileExporter  *exporter,
				    EvRenderContext *rc)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);
	PdfPrintContext *ctx = pdf_document->print_ctx;
	PopplerPage *poppler_page;
#ifdef HAVE_CAIRO_PRINT
	gdouble  page_width, page_height;
	gint     x, y;
	gboolean rotate;
	gdouble  width, height;
	gdouble  pwidth, pheight;
	gdouble  xscale, yscale;
#endif

	g_return_if_fail (pdf_document->print_ctx != NULL);

	poppler_page = POPPLER_PAGE (rc->page->backend_page);
	
#ifdef HAVE_CAIRO_PRINT
	x = (ctx->pages_printed % ctx->pages_per_sheet) % ctx->pages_x;
	y = (ctx->pages_printed % ctx->pages_per_sheet) / ctx->pages_x;
	poppler_page_get_size (poppler_page, &page_width, &page_height);

	if (page_width > page_height && page_width > ctx->paper_width) {
		rotate = TRUE;
	} else {
		rotate = FALSE;
	}

	/* Use always portrait mode and rotate when necessary */
	if (ctx->paper_width > ctx->paper_height) {
		width = ctx->paper_height;
		height = ctx->paper_width;
		rotate = !rotate;
	} else {
		width = ctx->paper_width;
		height = ctx->paper_height;
	}

	if (ctx->pages_per_sheet == 2 || ctx->pages_per_sheet == 6) {
		rotate = !rotate;
	}	

	if (rotate) {
		gint tmp1;
		gdouble tmp2;

		tmp1 = x;
		x = y;
		y = tmp1;

		tmp2 = page_width;
		page_width = page_height;
		page_height = tmp2;
	}

	pwidth = width / ctx->pages_x;
	pheight = height / ctx->pages_y;

	if ((page_width > pwidth || page_height > pheight) ||
	    (page_width < pwidth && page_height < pheight)) {
		xscale = pwidth / page_width;
		yscale = pheight / page_height;
		
		if (yscale < xscale) {
			xscale = yscale;
		} else {
			yscale = xscale;
		}
		
	} else {	
		xscale = yscale = 1;
	}

	/* TODO: center */

	cairo_save (ctx->cr);
	if (rotate) {
		cairo_matrix_t matrix;
		
		cairo_translate (ctx->cr, (2 * y + 1) * pwidth, 0);
		cairo_matrix_init (&matrix,
				   0,  1,
				   -1,  0,
				   0,  0);
		cairo_transform (ctx->cr, &matrix);
	}
	
	cairo_translate (ctx->cr,
			 x * (rotate ? pheight : pwidth),
			 y * (rotate ? pwidth : pheight));
	cairo_scale (ctx->cr, xscale, yscale);

	poppler_page_render_for_printing (poppler_page, ctx->cr);

	ctx->pages_printed++;
			
	cairo_restore (ctx->cr);
#else /* HAVE_CAIRO_PRINT */
	if (ctx->format == EV_FILE_FORMAT_PS)
		poppler_page_render_to_ps (poppler_page, ctx->ps_file);
#endif /* HAVE_CAIRO_PRINT */
}

static void
pdf_document_file_exporter_end_page (EvFileExporter *exporter)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);
	PdfPrintContext *ctx = pdf_document->print_ctx;
	
	g_return_if_fail (pdf_document->print_ctx != NULL);

#ifdef HAVE_CAIRO_PRINT
	cairo_show_page (ctx->cr);
#endif
}

static void
pdf_document_file_exporter_end (EvFileExporter *exporter)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);

	pdf_print_context_free (pdf_document->print_ctx);
	pdf_document->print_ctx = NULL;
}

static EvFileExporterCapabilities
pdf_document_file_exporter_get_capabilities (EvFileExporter *exporter)
{
	return  (EvFileExporterCapabilities) (
		EV_FILE_EXPORTER_CAN_PAGE_SET |
		EV_FILE_EXPORTER_CAN_COPIES |
		EV_FILE_EXPORTER_CAN_COLLATE |
		EV_FILE_EXPORTER_CAN_REVERSE |
		EV_FILE_EXPORTER_CAN_SCALE |
#ifdef HAVE_CAIRO_PRINT
#ifdef HAVE_POPPLER_PAGE_RENDER
		EV_FILE_EXPORTER_CAN_NUMBER_UP |
#endif
#endif
		
#ifdef HAVE_CAIRO_PDF
#ifdef HAVE_POPPLER_PAGE_RENDER
		EV_FILE_EXPORTER_CAN_GENERATE_PDF |
#endif
#endif
		EV_FILE_EXPORTER_CAN_GENERATE_PS);
}

static void
pdf_document_file_exporter_iface_init (EvFileExporterIface *iface)
{
        iface->begin = pdf_document_file_exporter_begin;
	iface->begin_page = pdf_document_file_exporter_begin_page;
        iface->do_page = pdf_document_file_exporter_do_page;
	iface->end_page = pdf_document_file_exporter_end_page;
        iface->end = pdf_document_file_exporter_end;
	iface->get_capabilities = pdf_document_file_exporter_get_capabilities;
}

#ifdef HAVE_POPPLER_PAGE_RENDER
/* EvDocumentPrint */
static void
pdf_document_print_print_page (EvDocumentPrint *document,
			       EvPage          *page,
			       cairo_t         *cr)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);

	poppler_page_render_for_printing (POPPLER_PAGE (page->backend_page), cr);
}

static void
pdf_document_document_print_iface_init (EvDocumentPrintIface *iface)
{
	iface->print_page = pdf_document_print_print_page;
}
#endif /* HAVE_POPPLER_PAGE_RENDER */

static void
pdf_selection_render_selection (EvSelection      *selection,
				EvRenderContext  *rc,
				cairo_surface_t **surface,
				EvRectangle      *points,
				EvRectangle      *old_points,
				EvSelectionStyle  style,
				GdkColor         *text,
				GdkColor         *base)
{
	PopplerPage *poppler_page;
	double width_points, height_points;
	gint width, height;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	poppler_page_get_size (poppler_page,
			       &width_points, &height_points);
	width = (int) ((width_points * rc->scale) + 0.5);
	height = (int) ((height_points * rc->scale) + 0.5);

#ifdef HAVE_POPPLER_PAGE_RENDER
	cairo_t *cr;
	PopplerColor text_color, base_color;
	
	text_color.red = text->red;
	text_color.green = text->green;
	text_color.blue = text->blue;

	base_color.red = base->red;
	base_color.green = base->green;
	base_color.blue = base->blue;

	if (*surface == NULL) {
		*surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						       width, height);
		
	}

	cr = cairo_create (*surface);
	cairo_scale (cr, rc->scale, rc->scale);
	cairo_surface_set_device_offset (*surface, 0, 0);
	memset (cairo_image_surface_get_data (*surface), 0x00,
		cairo_image_surface_get_height (*surface) *
		cairo_image_surface_get_stride (*surface));
	poppler_page_render_selection (poppler_page,
				       cr,
				       (PopplerRectangle *)points,
				       (PopplerRectangle *)old_points,
				       (PopplerSelectionStyle)style,
				       &text_color,
				       &base_color);
	cairo_destroy (cr);
#else /* HAVE_POPPLER_PAGE_RENDER */
	GdkPixbuf *pixbuf;
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 TRUE, 8,
				 width, height);

	poppler_page_render_selection_to_pixbuf (poppler_page,
						 rc->scale, rc->rotation, pixbuf,
						 (PopplerRectangle *)points,
						 (PopplerRectangle *)old_points,
						 (PopplerSelectionStyle)style,
						 text,
						 base);
	if (*surface)
		cairo_surface_destroy (*surface);
	*surface = ev_document_misc_surface_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
#endif /* HAVE_POPPLER_PAGE_RENDER */
}

static gchar *
pdf_selection_get_selected_text (EvSelection     *selection,
				 EvRenderContext *rc,
				 EvSelectionStyle style,
				 EvRectangle     *points)
{
	PopplerPage *poppler_page;
	PopplerRectangle r;
	double height;
	char *retval;
	
	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	poppler_page_get_size (poppler_page, NULL, &height);
	r.x1 = points->x1;
	r.y1 = height - points->y2;
	r.x2 = points->x2;
	r.y2 = height - points->y1;

	retval = poppler_page_get_text (poppler_page,
					(PopplerSelectionStyle)style,
					&r);

	return retval;
}

static GdkRegion *
create_gdk_region_from_poppler_region (GList *region)
{
	GList *l;
	GdkRegion *retval;
	
	retval = gdk_region_new ();
	
	for (l = region; l; l = g_list_next (l)) {
		PopplerRectangle *rectangle;
		GdkRectangle      rect;
		
		rectangle = (PopplerRectangle *)l->data;
		
		rect.x = (gint) rectangle->x1;
		rect.y = (gint) rectangle->y1;
		rect.width  = (gint) (rectangle->x2 - rectangle->x1);
		rect.height = (gint) (rectangle->y2 - rectangle->y1);
		gdk_region_union_with_rect (retval, &rect);
		
		poppler_rectangle_free (rectangle);
	}

	return retval;
}

static GdkRegion *
pdf_selection_get_selection_region (EvSelection     *selection,
				    EvRenderContext *rc,
				    EvSelectionStyle style,
				    EvRectangle     *points)
{
	PopplerPage *poppler_page;
	GdkRegion   *retval;
	GList       *region;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);
	
	region = poppler_page_get_selection_region (poppler_page,
						    rc->scale,
						    (PopplerSelectionStyle)style,
						    (PopplerRectangle *) points);
	retval = create_gdk_region_from_poppler_region (region);
	g_list_free (region);
	
	return retval;
}

static GdkRegion *
pdf_selection_get_selection_map (EvSelection     *selection,
				 EvRenderContext *rc)
{
	PopplerPage *poppler_page;
	PopplerRectangle points;
	GList *region;
	GdkRegion *retval;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	points.x1 = 0.0;
	points.y1 = 0.0;
	poppler_page_get_size (poppler_page, &(points.x2), &(points.y2));
	
	region = poppler_page_get_selection_region (poppler_page, 1.0,
						    POPPLER_SELECTION_GLYPH,
						    &points);
	retval = create_gdk_region_from_poppler_region (region);
	g_list_free (region);

	return retval;
}

static void
pdf_selection_iface_init (EvSelectionIface *iface)
{
        iface->render_selection = pdf_selection_render_selection;
	iface->get_selected_text = pdf_selection_get_selected_text;
        iface->get_selection_region = pdf_selection_get_selection_region;
        iface->get_selection_map = pdf_selection_get_selection_map;
}

/* Page Transitions */
static gdouble
pdf_document_get_page_duration (EvDocumentTransition *trans,
				gint                  page)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	gdouble      duration = -1;

	pdf_document = PDF_DOCUMENT (trans);
	poppler_page = poppler_document_get_page (pdf_document->document, page);
	if (!poppler_page)
		return -1;

	duration = poppler_page_get_duration (poppler_page);
	g_object_unref (poppler_page);

	return duration;
}

static EvTransitionEffect *
pdf_document_get_effect (EvDocumentTransition *trans,
			 gint                  page)
{
	PdfDocument            *pdf_document;
	PopplerPage            *poppler_page;
	PopplerPageTransition  *page_transition;
	EvTransitionEffect     *effect;

	pdf_document = PDF_DOCUMENT (trans);
	poppler_page = poppler_document_get_page (pdf_document->document, page);

	if (!poppler_page)
		return NULL;

	page_transition = poppler_page_get_transition (poppler_page);

	if (!page_transition) {
		g_object_unref (poppler_page);
		return NULL;
	}

	/* enums in PopplerPageTransition match the EvTransitionEffect ones */
	effect = ev_transition_effect_new ((EvTransitionEffectType) page_transition->type,
					   "alignment", page_transition->alignment,
					   "direction", page_transition->direction,
					   "duration", page_transition->duration,
					   "angle", page_transition->angle,
					   "scale", page_transition->scale,
					   "rectangular", page_transition->rectangular,
					   NULL);

	poppler_page_transition_free (page_transition);
	g_object_unref (poppler_page);

	return effect;
}

static void
pdf_document_page_transition_iface_init (EvDocumentTransitionIface *iface)
{
	iface->get_page_duration = pdf_document_get_page_duration;
	iface->get_effect = pdf_document_get_effect;
}

/* Forms */
static void
pdf_document_get_crop_box (EvDocument  *document, 
			   int          page, 
			   EvRectangle *rect)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	PopplerRectangle poppler_rect;

	pdf_document = PDF_DOCUMENT (document);
	poppler_page = poppler_document_get_page (pdf_document->document, page);
	poppler_page_get_crop_box (poppler_page, &poppler_rect);
	rect->x1 = poppler_rect.x1;
	rect->x2 = poppler_rect.x2;
	rect->y1 = poppler_rect.y1;
	rect->y2 = poppler_rect.y2;
}

static EvFormField *
ev_form_field_from_poppler_field (PopplerFormField *poppler_field)
{
	EvFormField *ev_field = NULL;
	gint         id;
	gdouble      font_size;
	gboolean     is_read_only;

	id = poppler_form_field_get_id (poppler_field);
	font_size = poppler_form_field_get_font_size (poppler_field);
	is_read_only = poppler_form_field_is_read_only (poppler_field);

	switch (poppler_form_field_get_field_type (poppler_field)) {
	        case POPPLER_FORM_FIELD_TEXT: {
			EvFormFieldText    *field_text;
			EvFormFieldTextType ev_text_type = EV_FORM_FIELD_TEXT_NORMAL;

			switch (poppler_form_field_text_get_text_type (poppler_field)) {
			        case POPPLER_FORM_TEXT_NORMAL:
					ev_text_type = EV_FORM_FIELD_TEXT_NORMAL;
					break;
			        case POPPLER_FORM_TEXT_MULTILINE:
					ev_text_type = EV_FORM_FIELD_TEXT_MULTILINE;
					break;
			        case POPPLER_FORM_TEXT_FILE_SELECT:
					ev_text_type = EV_FORM_FIELD_TEXT_FILE_SELECT;
					break;
			}
			
			ev_field = ev_form_field_text_new (id, ev_text_type);
			field_text = EV_FORM_FIELD_TEXT (ev_field);

			field_text->do_spell_check = poppler_form_field_text_do_spell_check (poppler_field);
			field_text->do_scroll = poppler_form_field_text_do_scroll (poppler_field);
			field_text->is_rich_text = poppler_form_field_text_is_rich_text (poppler_field);
			field_text->is_password = poppler_form_field_text_is_password (poppler_field);
			field_text->max_len = poppler_form_field_text_get_max_len (poppler_field);
			field_text->text = poppler_form_field_text_get_text (poppler_field);

		}
			break;
	        case POPPLER_FORM_FIELD_BUTTON: {
			EvFormFieldButton    *field_button;
			EvFormFieldButtonType ev_button_type = EV_FORM_FIELD_BUTTON_PUSH;

			switch (poppler_form_field_button_get_button_type (poppler_field)) {
			        case POPPLER_FORM_BUTTON_PUSH:
					ev_button_type = EV_FORM_FIELD_BUTTON_PUSH;
					break;
			        case POPPLER_FORM_BUTTON_CHECK:
					ev_button_type = EV_FORM_FIELD_BUTTON_CHECK;
					break;
			        case POPPLER_FORM_BUTTON_RADIO:
					ev_button_type = EV_FORM_FIELD_BUTTON_RADIO;
					break;
			}

			ev_field = ev_form_field_button_new (id, ev_button_type);
			field_button = EV_FORM_FIELD_BUTTON (ev_field);
			
			field_button->state = poppler_form_field_button_get_state (poppler_field);
		}
			break;
	        case POPPLER_FORM_FIELD_CHOICE: {
			EvFormFieldChoice    *field_choice;
			EvFormFieldChoiceType ev_choice_type = EV_FORM_FIELD_CHOICE_COMBO;

			switch (poppler_form_field_choice_get_choice_type (poppler_field)) {
			        case POPPLER_FORM_CHOICE_COMBO:
					ev_choice_type = EV_FORM_FIELD_CHOICE_COMBO;
					break;
			        case EV_FORM_FIELD_CHOICE_LIST:
					ev_choice_type = EV_FORM_FIELD_CHOICE_LIST;
					break;
			}

			ev_field = ev_form_field_choice_new (id, ev_choice_type);
			field_choice = EV_FORM_FIELD_CHOICE (ev_field);

			field_choice->is_editable = poppler_form_field_choice_is_editable (poppler_field);
			field_choice->multi_select = poppler_form_field_choice_can_select_multiple (poppler_field);
			field_choice->do_spell_check = poppler_form_field_choice_do_spell_check (poppler_field);
			field_choice->commit_on_sel_change = poppler_form_field_choice_commit_on_change (poppler_field);

			/* TODO: we need poppler_form_field_choice_get_selected_items in poppler 
			field_choice->selected_items = poppler_form_field_choice_get_selected_items (poppler_field);*/
			if (field_choice->is_editable)
				field_choice->text = poppler_form_field_choice_get_text (poppler_field);
		}
			break;
	        case POPPLER_FORM_FIELD_SIGNATURE:
			/* TODO */
			ev_field = ev_form_field_signature_new (id);
			break;
	        case POPPLER_FORM_FIELD_UNKNOWN:
			return NULL;
	}

	ev_field->font_size = font_size;
	ev_field->is_read_only = is_read_only;

	return ev_field;
}

static GList *
pdf_document_forms_get_form_fields (EvDocumentForms *document, 
				    EvPage          *page)
{
 	PopplerPage *poppler_page;
 	GList *retval = NULL;
 	GList *fields;
 	GList *list;
 	double height;

	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);
	
 	poppler_page = POPPLER_PAGE (page->backend_page);
 	fields = poppler_page_get_form_field_mapping (poppler_page);
 	poppler_page_get_size (poppler_page, NULL, &height);

 	for (list = fields; list; list = list->next) {
 		PopplerFormFieldMapping *mapping;
 		EvMapping *field_mapping;
		EvFormField *ev_field;

 		mapping = (PopplerFormFieldMapping *)list->data;

		ev_field = ev_form_field_from_poppler_field (mapping->field);
		if (!ev_field)
			continue;

 		field_mapping = g_new0 (EvMapping, 1);
		field_mapping->area.x1 = mapping->area.x1;
		field_mapping->area.x2 = mapping->area.x2;
		field_mapping->area.y1 = height - mapping->area.y2;
		field_mapping->area.y2 = height - mapping->area.y1;
		field_mapping->data = ev_field;
		ev_field->page = EV_PAGE (g_object_ref (page));

		g_object_set_data_full (G_OBJECT (ev_field),
					"poppler-field",
					g_object_ref (mapping->field),
					(GDestroyNotify) g_object_unref);
		
		retval = g_list_prepend (retval, field_mapping);
	}
	
	poppler_page_free_form_field_mapping (fields);

	return g_list_reverse (retval);
}

static gchar *
pdf_document_forms_form_field_text_get_text (EvDocumentForms *document,
					     EvFormField     *field)
	
{
	PopplerFormField *poppler_field;
	gchar *text;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return NULL;
	
	text = poppler_form_field_text_get_text (poppler_field);

	return text;
}

static void
pdf_document_forms_form_field_text_set_text (EvDocumentForms *document, 
					     EvFormField     *field,
					     const gchar     *text)
{
	PopplerFormField *poppler_field;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return;
	
	poppler_form_field_text_set_text (poppler_field, text);
	PDF_DOCUMENT (document)->modified = TRUE;
}

static void
pdf_document_forms_form_field_button_set_state (EvDocumentForms *document, 
						EvFormField     *field,
						gboolean         state)
{
	PopplerFormField *poppler_field;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return;
	
	poppler_form_field_button_set_state (poppler_field, state);
	PDF_DOCUMENT (document)->modified = TRUE;
}

static gboolean
pdf_document_forms_form_field_button_get_state (EvDocumentForms *document, 
						EvFormField     *field)
{
	PopplerFormField *poppler_field;
	gboolean state;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return FALSE;

	state = poppler_form_field_button_get_state (poppler_field);

	return state;
}

static gchar *
pdf_document_forms_form_field_choice_get_item (EvDocumentForms *document, 
					       EvFormField     *field,
					       gint             index)
{
	PopplerFormField *poppler_field;
	gchar *text;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return NULL;

	text = poppler_form_field_choice_get_item (poppler_field, index);

	return text;
}

static int
pdf_document_forms_form_field_choice_get_n_items (EvDocumentForms *document, 
						  EvFormField     *field)
{
	PopplerFormField *poppler_field;
	gint n_items;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return -1;
	
	n_items = poppler_form_field_choice_get_n_items (poppler_field);

	return n_items;
}

static gboolean
pdf_document_forms_form_field_choice_is_item_selected (EvDocumentForms *document, 
						       EvFormField     *field,
						       gint             index)
{
	PopplerFormField *poppler_field;
	gboolean selected;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return FALSE;

	selected = poppler_form_field_choice_is_item_selected (poppler_field, index);

	return selected;
}

static void
pdf_document_forms_form_field_choice_select_item (EvDocumentForms *document, 
						  EvFormField     *field,
						  gint             index)
{
	PopplerFormField *poppler_field;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return;

	poppler_form_field_choice_select_item (poppler_field, index);
	PDF_DOCUMENT (document)->modified = TRUE;
}

static void
pdf_document_forms_form_field_choice_toggle_item (EvDocumentForms *document, 
						  EvFormField     *field,
						  gint             index)
{
	PopplerFormField *poppler_field;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return;

	poppler_form_field_choice_toggle_item (poppler_field, index);
	PDF_DOCUMENT (document)->modified = TRUE;
}

static void
pdf_document_forms_form_field_choice_unselect_all (EvDocumentForms *document, 
						   EvFormField     *field)
{
	PopplerFormField *poppler_field;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return;
	
	poppler_form_field_choice_unselect_all (poppler_field);
	PDF_DOCUMENT (document)->modified = TRUE;
}

static void
pdf_document_forms_form_field_choice_set_text (EvDocumentForms *document,
					       EvFormField     *field,
					       const gchar     *text)
{
	PopplerFormField *poppler_field;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return;
	
	poppler_form_field_choice_set_text (poppler_field, text);
	PDF_DOCUMENT (document)->modified = TRUE;
}

static gchar *
pdf_document_forms_form_field_choice_get_text (EvDocumentForms *document,
					       EvFormField     *field)
{
	PopplerFormField *poppler_field;
	gchar *text;

	poppler_field = POPPLER_FORM_FIELD (g_object_get_data (G_OBJECT (field), "poppler-field"));
	if (!poppler_field)
		return NULL;

	text = poppler_form_field_choice_get_text (poppler_field);

	return text;
}

static void
pdf_document_document_forms_iface_init (EvDocumentFormsIface *iface)
{
	iface->get_form_fields = pdf_document_forms_get_form_fields;
	iface->form_field_text_get_text = pdf_document_forms_form_field_text_get_text;
	iface->form_field_text_set_text = pdf_document_forms_form_field_text_set_text;
	iface->form_field_button_set_state = pdf_document_forms_form_field_button_set_state;
	iface->form_field_button_get_state = pdf_document_forms_form_field_button_get_state;
	iface->form_field_choice_get_item = pdf_document_forms_form_field_choice_get_item;
	iface->form_field_choice_get_n_items = pdf_document_forms_form_field_choice_get_n_items;
	iface->form_field_choice_is_item_selected = pdf_document_forms_form_field_choice_is_item_selected;
	iface->form_field_choice_select_item = pdf_document_forms_form_field_choice_select_item;
	iface->form_field_choice_toggle_item = pdf_document_forms_form_field_choice_toggle_item;
	iface->form_field_choice_unselect_all = pdf_document_forms_form_field_choice_unselect_all;
	iface->form_field_choice_set_text = pdf_document_forms_form_field_choice_set_text;
	iface->form_field_choice_get_text = pdf_document_forms_form_field_choice_get_text;
}

/* Annotations */
static void
poppler_annot_color_to_gdk_color (PopplerAnnot *poppler_annot,
				  GdkColor     *color)
{
	PopplerColor *poppler_color;

	poppler_color = poppler_annot_get_color (poppler_annot);
	if (poppler_color) {
		color->red = poppler_color->red;
		color->green = poppler_color->green;
		color->blue = poppler_color->blue;

		g_free (poppler_color);
	} /* TODO: else use a default color */
}

static EvAnnotation *
ev_annot_from_poppler_annot (PopplerAnnot *poppler_annot,
			     EvPage       *page)
{
	EvAnnotation *ev_annot = NULL;
	const gchar  *unimplemented_annot = NULL;

	switch (poppler_annot_get_annot_type (poppler_annot)) {
	        case POPPLER_ANNOT_TEXT:
			PopplerAnnotText *poppler_text;
			EvAnnotationText *ev_annot_text;

			poppler_text = POPPLER_ANNOT_TEXT (poppler_annot);

			ev_annot = ev_annotation_text_new (page);

			ev_annot_text = EV_ANNOTATION_TEXT (ev_annot);
			ev_annot_text->is_open = poppler_annot_text_get_is_open (poppler_text);

			break;
	        case POPPLER_ANNOT_LINK:
	        case POPPLER_ANNOT_WIDGET:
			/* Ignore link and widgets annots since they are already handled */
			break;
	        default: {
			GEnumValue *enum_value;

			enum_value = g_enum_get_value ((GEnumClass *) g_type_class_ref (POPPLER_TYPE_ANNOT_TYPE),
						       poppler_annot_get_annot_type (poppler_annot));
			unimplemented_annot = enum_value ? enum_value->value_name : "Unknown annotation";
		}
	}

	if (unimplemented_annot) {
		g_warning ("Unimplemented annotation: %s, please post a "
		           "bug report in Evince bugzilla "
		           "(http://bugzilla.gnome.org) with a testcase.",
			   unimplemented_annot);
	}

	if (ev_annot) {
		ev_annot->contents = poppler_annot_get_contents (poppler_annot);
		ev_annot->name = poppler_annot_get_name (poppler_annot);
		ev_annot->modified = poppler_annot_get_modified (poppler_annot);
		poppler_annot_color_to_gdk_color (poppler_annot, &ev_annot->color);

		if (POPPLER_IS_ANNOT_MARKUP (poppler_annot)) {
			PopplerAnnotMarkup *markup;
			gchar *label;
			gdouble opacity;
			PopplerRectangle poppler_rect;

			markup = POPPLER_ANNOT_MARKUP (poppler_annot);

			if (poppler_annot_markup_get_popup_rectangle (markup, &poppler_rect)) {
				EvRectangle ev_rect;
				gboolean is_open;
				gdouble height;

				poppler_page_get_size (POPPLER_PAGE (page->backend_page),
						       NULL, &height);
				ev_rect.x1 = poppler_rect.x1;
				ev_rect.x2 = poppler_rect.x2;
				ev_rect.y1 = height - poppler_rect.y2;
				ev_rect.y2 = height - poppler_rect.y1;

				is_open = poppler_annot_markup_get_popup_is_open (markup);

				g_object_set (ev_annot,
					      "rectangle", &ev_rect,
					      "is_open", is_open,
					      "has_popup", TRUE,
					      NULL);
			} else {
				/* FIXME: Use poppler_annot_markup_has_popup() when
				 * new poppler is released.
				 */
				g_object_set (ev_annot,
					      "has_popup", FALSE,
					      NULL);
			}

			label = poppler_annot_markup_get_label (markup);
			opacity = poppler_annot_markup_get_opacity (markup);

			g_object_set (ev_annot,
				      "label", label,
				      "opacity", opacity,
				      NULL);

			g_free (label);
		}
	}

	return ev_annot;
}

static GList *
pdf_document_annotations_get_annotations (EvDocumentAnnotations *document_annotations,
					  EvPage                *page)
{
	GList *retval = NULL;
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GList *annots;
	GList *list;
	gdouble height;
	gint i = 0;

	pdf_document = PDF_DOCUMENT (document_annotations);
	poppler_page = POPPLER_PAGE (page->backend_page);
	annots = poppler_page_get_annot_mapping (poppler_page);
	poppler_page_get_size (poppler_page, NULL, &height);

	for (list = annots; list; list = list->next) {
		PopplerAnnotMapping *mapping;
		EvMapping *annot_mapping;
		EvAnnotation        *ev_annot;

		mapping = (PopplerAnnotMapping *)list->data;

		ev_annot = ev_annot_from_poppler_annot (mapping->annot, page);
		if (!ev_annot)
			continue;

		i++;

		/* Make sure annot has a unique name */
		if (!ev_annot->name)
			ev_annot->name = g_strdup_printf ("annot-%d-%d", page->index, i);

		annot_mapping = g_new (EvMapping, 1);
		annot_mapping->area.x1 = mapping->area.x1;
		annot_mapping->area.x2 = mapping->area.x2;
		annot_mapping->area.y1 = height - mapping->area.y2;
		annot_mapping->area.y2 = height - mapping->area.y1;
		annot_mapping->data = ev_annot;

		g_object_set_data_full (G_OBJECT (ev_annot),
					"poppler-annot",
					g_object_ref (mapping->annot),
					(GDestroyNotify) g_object_unref);

		retval = g_list_prepend (retval, annot_mapping);
	}

	poppler_page_free_annot_mapping (annots);

	return g_list_reverse (retval);
}

static void
pdf_document_annotations_annotation_set_contents (EvDocumentAnnotations *document,
						  EvAnnotation          *annot,
						  const gchar           *contents)
{
	PopplerAnnot *poppler_annot;

	poppler_annot = POPPLER_ANNOT (g_object_get_data (G_OBJECT (annot), "poppler-annot"));
	if (!poppler_annot)
		return;

	poppler_annot_set_contents (poppler_annot, contents);
	PDF_DOCUMENT (document)->modified = TRUE;
}

static void
pdf_document_document_annotations_iface_init (EvDocumentAnnotationsIface *iface)
{
	iface->get_annotations = pdf_document_annotations_get_annotations;
	iface->annotation_set_contents = pdf_document_annotations_annotation_set_contents;
}

/* Layers */
static gboolean
pdf_document_layers_has_layers (EvDocumentLayers *document)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerLayersIter *iter;

	iter = poppler_layers_iter_new (pdf_document->document);
	if (!iter)
		return FALSE;
	poppler_layers_iter_free (iter);

	return TRUE;
}

static void
build_layers_tree (PdfDocument       *pdf_document,
		   GtkTreeModel      *model,
		   GtkTreeIter       *parent,
		   PopplerLayersIter *iter)
{
	do {
		GtkTreeIter        tree_iter;
		PopplerLayersIter *child;
		PopplerLayer      *layer;
		EvLayer           *ev_layer = NULL;
		gboolean           visible;
		gchar             *markup;
		gint               rb_group = 0;

		layer = poppler_layers_iter_get_layer (iter);
		if (layer) {
			markup = g_markup_escape_text (poppler_layer_get_title (layer), -1);
			visible = poppler_layer_is_visible (layer);
			rb_group = poppler_layer_get_radio_button_group_id (layer);
			pdf_document->layers = g_list_append (pdf_document->layers,
							      g_object_ref (layer));
			ev_layer = ev_layer_new (g_list_length (pdf_document->layers) - 1,
						 poppler_layer_is_parent (layer),
						 rb_group);
		} else {
			gchar *title;

			title = poppler_layers_iter_get_title (iter);
			markup = g_markup_escape_text (title, -1);
			g_free (title);

			visible = FALSE;
			layer = NULL;
		}

		gtk_tree_store_append (GTK_TREE_STORE (model), &tree_iter, parent);
		gtk_tree_store_set (GTK_TREE_STORE (model), &tree_iter,
				    EV_DOCUMENT_LAYERS_COLUMN_TITLE, markup,
				    EV_DOCUMENT_LAYERS_COLUMN_VISIBLE, visible,
				    EV_DOCUMENT_LAYERS_COLUMN_ENABLED, TRUE, /* FIXME */
				    EV_DOCUMENT_LAYERS_COLUMN_SHOWTOGGLE, (layer != NULL),
				    EV_DOCUMENT_LAYERS_COLUMN_RBGROUP, rb_group,
				    EV_DOCUMENT_LAYERS_COLUMN_LAYER, ev_layer,
				    -1);
		if (ev_layer)
			g_object_unref (ev_layer);
		g_free (markup);

		child = poppler_layers_iter_get_child (iter);
		if (child)
			build_layers_tree (pdf_document, model, &tree_iter, child);
		poppler_layers_iter_free (child);
	} while (poppler_layers_iter_next (iter));
}

static GtkTreeModel *
pdf_document_layers_get_layers (EvDocumentLayers *document)
{
	GtkTreeModel *model = NULL;
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerLayersIter *iter;

	iter = poppler_layers_iter_new (pdf_document->document);
	if (iter) {
		model = (GtkTreeModel *) gtk_tree_store_new (EV_DOCUMENT_LAYERS_N_COLUMNS,
							     G_TYPE_STRING,  /* TITLE */
							     G_TYPE_OBJECT,  /* LAYER */
							     G_TYPE_BOOLEAN, /* VISIBLE */
							     G_TYPE_BOOLEAN, /* ENABLED */
							     G_TYPE_BOOLEAN, /* SHOWTOGGLE */
							     G_TYPE_INT);    /* RBGROUP */
		build_layers_tree (pdf_document, model, NULL, iter);
		poppler_layers_iter_free (iter);
	}
	return model;
}

static void
pdf_document_layers_show_layer (EvDocumentLayers *document,
				EvLayer          *layer)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	guint        layer_id = ev_layer_get_id (layer);

	poppler_layer_show (POPPLER_LAYER (g_list_nth_data (pdf_document->layers, layer_id)));
}

static void
pdf_document_layers_hide_layer (EvDocumentLayers *document,
				EvLayer          *layer)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	guint        layer_id = ev_layer_get_id (layer);

	poppler_layer_hide (POPPLER_LAYER (g_list_nth_data (pdf_document->layers, layer_id)));
}

static gboolean
pdf_document_layers_layer_is_visible (EvDocumentLayers *document,
				      EvLayer          *layer)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	guint        layer_id = ev_layer_get_id (layer);

	return poppler_layer_is_visible (POPPLER_LAYER (g_list_nth_data (pdf_document->layers, layer_id)));
}

static void
pdf_document_document_layers_iface_init (EvDocumentLayersIface *iface)
{
	iface->has_layers = pdf_document_layers_has_layers;
	iface->get_layers = pdf_document_layers_get_layers;
	iface->show_layer = pdf_document_layers_show_layer;
	iface->hide_layer = pdf_document_layers_hide_layer;
	iface->layer_is_visible = pdf_document_layers_layer_is_visible;
}
