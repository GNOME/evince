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
#include <glib/gi18n.h>

#include "ev-poppler.h"
#include "ev-file-exporter.h"
#include "ev-document-find.h"
#include "ev-document-misc.h"
#include "ev-document-links.h"
#include "ev-document-images.h"
#include "ev-document-fonts.h"
#include "ev-document-security.h"
#include "ev-document-thumbnails.h"
#include "ev-document-transition.h"
#include "ev-document-forms.h"
#include "ev-selection.h"
#include "ev-transition-effect.h"
#include "ev-attachment.h"
#include "ev-image.h"

#if (defined (HAVE_POPPLER_PAGE_RENDER) || defined (HAVE_POPPLER_PAGE_RENDER_FOR_PRINTING)) && (defined (HAVE_CAIRO_PDF) || defined (HAVE_CAIRO_PS))
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

	PopplerFontInfo *font_info;
	PopplerFontsIter *fonts_iter;
	int fonts_scanned_pages;

	PdfDocumentSearch *search;
	PdfPrintContext *print_ctx;
};

static void pdf_document_document_iface_init            (EvDocumentIface           *iface);
static void pdf_document_security_iface_init            (EvDocumentSecurityIface   *iface);
static void pdf_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);
static void pdf_document_document_links_iface_init      (EvDocumentLinksIface      *iface);
static void pdf_document_document_images_iface_init     (EvDocumentImagesIface     *iface);
static void pdf_document_document_forms_iface_init      (EvDocumentFormsIface      *iface);
static void pdf_document_document_fonts_iface_init      (EvDocumentFontsIface      *iface);
static void pdf_document_find_iface_init                (EvDocumentFindIface       *iface);
static void pdf_document_file_exporter_iface_init       (EvFileExporterIface       *iface);
static void pdf_selection_iface_init                    (EvSelectionIface          *iface);
static void pdf_document_page_transition_iface_init     (EvDocumentTransitionIface *iface);
static void pdf_document_thumbnails_get_dimensions      (EvDocumentThumbnails      *document_thumbnails,
							 EvRenderContext           *rc,
							 gint                      *width,
							 gint                      *height);
static int  pdf_document_get_n_pages			(EvDocument                *document);

static EvLinkDest *ev_link_dest_from_dest   (PdfDocument       *pdf_document,
					     PopplerDest       *dest);
static EvLink     *ev_link_from_action      (PdfDocument       *pdf_document,
					     PopplerAction     *action);
static void        pdf_document_search_free (PdfDocumentSearch *search);
static void        pdf_print_context_free   (PdfPrintContext   *ctx);

EV_BACKEND_REGISTER_WITH_CODE (PdfDocument, pdf_document,
			 {
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_SECURITY,
							pdf_document_security_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
							pdf_document_document_thumbnails_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LINKS,
							pdf_document_document_links_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_IMAGES,
							pdf_document_document_images_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FORMS,
							pdf_document_document_forms_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FONTS,
							pdf_document_document_fonts_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND,
							pdf_document_find_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER,
							pdf_document_file_exporter_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_SELECTION,
							pdf_selection_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_TRANSITION,
							pdf_document_page_transition_iface_init);
			 });

static void
set_rc_data (PdfDocument     *pdf_document,
	     EvRenderContext *rc)
{
	if (rc->data == NULL) {
		rc->data = poppler_document_get_page (pdf_document->document,
						      rc->page);
		rc->destroy = g_object_unref;
	} else {
		g_assert (rc->page == poppler_page_get_index (POPPLER_PAGE (rc->data)));
	}
}

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
			

		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     code,
			     poppler_error->message,
			     NULL);
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
	gboolean retval;
	GError *poppler_error = NULL;

	retval = poppler_document_save (PDF_DOCUMENT (document)->document,
					uri,
					&poppler_error);
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

static void
pdf_document_get_page_size (EvDocument   *document,
			    int           page,
			    double       *width,
			    double       *height)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerPage *poppler_page;

	poppler_page = poppler_document_get_page (pdf_document->document, page);
	poppler_page_get_size (poppler_page, width, height);
	g_object_unref (poppler_page);
}

static char *
pdf_document_get_page_label (EvDocument *document,
			     int         page)
{
	PopplerPage *poppler_page;
	char *label = NULL;

	poppler_page = poppler_document_get_page (PDF_DOCUMENT (document)->document,
						  page);

	g_object_get (G_OBJECT (poppler_page),
		      "label", &label,
		      NULL);
	g_object_unref (poppler_page);

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
pdf_document_render (EvDocument      *document,
		     EvRenderContext *rc)
{
	PdfDocument *pdf_document;
	cairo_surface_t *surface;
	double width_points, height_points;
	gint width, height;

	pdf_document = PDF_DOCUMENT (document);

	set_rc_data (pdf_document, rc);

	poppler_page_get_size (POPPLER_PAGE (rc->data), &width_points, &height_points);

	if (rc->rotation == 90 || rc->rotation == 270) {
		width = (int) ((height_points * rc->scale) + 0.5);
		height = (int) ((width_points * rc->scale) + 0.5);
	} else {
		width = (int) ((width_points * rc->scale) + 0.5);
		height = (int) ((height_points * rc->scale) + 0.5);
	}

#ifdef HAVE_POPPLER_PAGE_RENDER
	cairo_t *cr;
	
	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					      width, height);
	memset (cairo_image_surface_get_data (surface), 0xff,
		cairo_image_surface_get_height (surface) *
		cairo_image_surface_get_stride (surface));
	
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
	poppler_page_render (POPPLER_PAGE (rc->data), cr);
	cairo_destroy (cr);
#else /* HAVE_POPPLER_PAGE_RENDER */
	GdkPixbuf *pixbuf;
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 FALSE, 8,
				 width, height);

	poppler_page_render_to_pixbuf (POPPLER_PAGE (rc->data),
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

static EvDocumentInfo *
pdf_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	PopplerPageLayout layout;
	PopplerPageMode mode;
	PopplerViewerPreferences view_prefs;
	PopplerPermissions permissions;

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
		      NULL);

	pdf_document_get_page_size(document, 0,
				   &(info->paper_width),
				   &(info->paper_height));

	// Convert to mm.
	info->paper_width = info->paper_width / 72.0f * 25.4f;
	info->paper_height = info->paper_height / 72.0f * 25.4f;

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

	info->n_pages = ev_document_get_n_pages (document);

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
							height - dest->top,
							dest->zoom,
							dest->change_left,
							dest->change_top,
							dest->change_zoom);
			g_object_unref (poppler_page);
		}
			break;
	        case POPPLER_DEST_FIT:
			ev_dest = ev_link_dest_new_fit (dest->page_num - 1);
			break;
	        case POPPLER_DEST_FITH: {
			PopplerPage *poppler_page;
			double height;

			poppler_page = poppler_document_get_page (pdf_document->document,
								  MAX (0, dest->page_num - 1));
			poppler_page_get_size (poppler_page, NULL, &height);
			ev_dest = ev_link_dest_new_fith (dest->page_num - 1,
							 height - dest->top,
							 dest->change_top);
			g_object_unref (poppler_page);
		}
			break;
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
							 height - dest->bottom,
							 dest->right,
							 height - dest->top);
			g_object_unref (poppler_page);
		}
			break;
	        case POPPLER_DEST_FITB:
			unimplemented_dest = "POPPLER_DEST_FITB";
			break;
	        case POPPLER_DEST_FITBH:
			unimplemented_dest = "POPPLER_DEST_FITBH";
			break;
	        case POPPLER_DEST_FITBV:
			unimplemented_dest = "POPPLER_DEST_FITBV";
			break;
	        case POPPLER_DEST_NAMED:
			ev_dest = ev_link_dest_new_named (dest->named_dest);
			break;
	        case POPPLER_DEST_UNKNOWN:
			unimplemented_dest = "POPPLER_DEST_UNKNOWN";
			break;
	}

	if (unimplemented_dest) {
		g_warning ("Unimplemented named action: %s, please post a "
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
		g_warning ("Unimplemented action: %s, please post a bug report with a testcase.",
			   unimplemented_action);
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
		EvLinkMapping *ev_link_mapping;

		link_mapping = (PopplerLinkMapping *)list->data;
		ev_link_mapping = g_new (EvLinkMapping, 1);
		ev_link_mapping->link = ev_link_from_action (pdf_document,
							     link_mapping->action);
		ev_link_mapping->x1 = link_mapping->area.x1;
		ev_link_mapping->x2 = link_mapping->area.x2;
		/* Invert this for X-style coordinates */
		ev_link_mapping->y1 = height - link_mapping->area.y2;
		ev_link_mapping->y2 = height - link_mapping->area.y1;

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
		EvImageMapping *ev_image_mapping;

		image_mapping = (PopplerImageMapping *)list->data;

		ev_image_mapping = g_new (EvImageMapping, 1);
#ifdef HAVE_POPPLER_PAGE_GET_IMAGE
		ev_image_mapping->image = ev_image_new (page, image_mapping->image_id);
#else
		ev_image_mapping->image = ev_image_new_from_pixbuf (image_mapping->image);
#endif
		ev_image_mapping->x1 = image_mapping->area.x1;
		ev_image_mapping->x2 = image_mapping->area.x2;
		ev_image_mapping->y1 = image_mapping->area.y1;
		ev_image_mapping->y2 = image_mapping->area.y2;

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
#ifdef HAVE_POPPLER_PAGE_GET_IMAGE
	PdfDocument     *pdf_document;
	PopplerPage     *poppler_page;
	cairo_surface_t *surface;
	GdkPixbuf       *retval = NULL;

	pdf_document = PDF_DOCUMENT (document_images);
	poppler_page = poppler_document_get_page (pdf_document->document,
						  ev_image_get_page (image));

	surface = poppler_page_get_image (poppler_page, ev_image_get_id (image));
	if (surface) {
		retval = ev_document_misc_pixbuf_from_surface (surface);
		cairo_surface_destroy (surface);
	}

	g_object_unref (poppler_page);

	return retval;
#else
	return GDK_PIXBUF (g_object_ref (ev_image_get_pixbuf (image)));
#endif /* HAVE_POPPLER_PAGE_GET_IMAGE */
}

static void
pdf_document_document_images_iface_init (EvDocumentImagesIface *iface)
{
	iface->get_image_mapping = pdf_document_images_get_image_mapping;
	iface->get_image = pdf_document_images_get_image;
}

static GdkPixbuf *
make_thumbnail_for_page (PdfDocument     *pdf_document,
			 PopplerPage     *poppler_page, 
			 EvRenderContext *rc)
{
	GdkPixbuf *pixbuf;
	int width, height;

	pdf_document_thumbnails_get_dimensions (EV_DOCUMENT_THUMBNAILS (pdf_document),
						rc, &width, &height);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				 width, height);
	gdk_pixbuf_fill (pixbuf, 0xffffffff);

	ev_document_fc_mutex_lock ();
	poppler_page_render_to_pixbuf (poppler_page, 0, 0,
				       width, height,
				       rc->scale, rc->rotation, pixbuf);
	ev_document_fc_mutex_unlock ();

	return pixbuf;
}

static GdkPixbuf *
pdf_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document_thumbnails,
				       EvRenderContext      *rc, 
				       gboolean              border)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GdkPixbuf *pixbuf;
	GdkPixbuf *border_pixbuf;

	pdf_document = PDF_DOCUMENT (document_thumbnails);

	poppler_page = poppler_document_get_page (pdf_document->document, rc->page);
	g_return_val_if_fail (poppler_page != NULL, NULL);

	pixbuf = poppler_page_get_thumbnail (poppler_page);
	if (pixbuf) {
		/* Rotate provided thumbnail if needed */
		GdkPixbuf *rotated_pixbuf;

		rotated_pixbuf = gdk_pixbuf_rotate_simple (pixbuf,
							   (GdkPixbufRotation) (360 - rc->rotation));
		g_object_unref (pixbuf);
		pixbuf = rotated_pixbuf;
	} else {
		/* There is no provided thumbnail.  We need to make one. */
		pixbuf = make_thumbnail_for_page (pdf_document, poppler_page, rc);
	}

        if (border) {		
		border_pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, pixbuf);
		g_object_unref (pixbuf);
		pixbuf = border_pixbuf;
	}		

	g_object_unref (poppler_page);
	
	return pixbuf;
}

static void
pdf_document_thumbnails_get_dimensions (EvDocumentThumbnails *document_thumbnails,
					EvRenderContext      *rc,
					gint                 *width,
					gint                 *height)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	gint has_thumb;
	
	pdf_document = PDF_DOCUMENT (document_thumbnails);
	poppler_page = poppler_document_get_page (pdf_document->document, rc->page);

	g_return_if_fail (poppler_page != NULL);

	has_thumb = poppler_page_get_thumbnail_size (poppler_page, width, height);

	if (!has_thumb) {
		double page_width, page_height;

		poppler_page_get_size (poppler_page, &page_width, &page_height);

		*width = (gint) MAX (page_width * rc->scale, 1);
		*height = (gint) MAX (page_height * rc->scale, 1);
	}
	
	if (rc->rotation == 90 || rc->rotation == 270) {
		gint  temp;

		temp = *width;
		*width = *height;
		*height = temp;
	}
	
	g_object_unref (poppler_page);
}

static void
pdf_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = pdf_document_thumbnails_get_thumbnail;
	iface->get_dimensions = pdf_document_thumbnails_get_dimensions;
}


static gboolean
pdf_document_search_idle_callback (void *data)
{
        PdfDocumentSearch *search = (PdfDocumentSearch*) data;
        PdfDocument *pdf_document = search->document;
        int n_pages;
	GList *matches;
	PopplerPage *page;

	page = poppler_document_get_page (search->document->document,
					  search->search_page);

	ev_document_doc_mutex_lock ();
	matches = poppler_page_find_text (page, search->text);
	ev_document_doc_mutex_unlock ();

	g_object_unref (page);

	search->pages[search->search_page] = matches;
	ev_document_find_changed (EV_DOCUMENT_FIND (pdf_document),
				  search->search_page);

        n_pages = pdf_document_get_n_pages (EV_DOCUMENT (search->document));
        search->search_page += 1;
        if (search->search_page == n_pages) {
                /* wrap around */
                search->search_page = 0;
        }

        if (search->search_page != search->start_page) {
	        return TRUE;
	}

        /* We're done. */
        search->idle = 0; /* will return FALSE to remove */
        return FALSE;
}


static PdfDocumentSearch *
pdf_document_search_new (PdfDocument *pdf_document,
			 int          start_page,
			 const char  *text)
{
	PdfDocumentSearch *search;
	int n_pages;
	int i;

	n_pages = pdf_document_get_n_pages (EV_DOCUMENT (pdf_document));

        search = g_new0 (PdfDocumentSearch, 1);

	search->text = g_strdup (text);
        search->pages = g_new0 (GList *, n_pages);
        search->document = pdf_document;

        /* We add at low priority so the progress bar repaints */
        search->idle = g_idle_add_full (G_PRIORITY_LOW,
                                        pdf_document_search_idle_callback,
                                        search,
                                        NULL);

        search->start_page = start_page;
        search->search_page = start_page;

	return search;
}

static void
pdf_document_find_begin (EvDocumentFind   *document,
			 int               page,
                         const char       *search_string,
                         gboolean          case_sensitive)
{
        PdfDocument *pdf_document = PDF_DOCUMENT (document);

        /* FIXME handle case_sensitive (right now XPDF
         * code is always case insensitive for ASCII
         * and case sensitive for all other languaages)
         */

	if (pdf_document->search &&
	    strcmp (search_string, pdf_document->search->text) == 0)
                return;

        if (pdf_document->search)
                pdf_document_search_free (pdf_document->search);

        pdf_document->search = pdf_document_search_new (pdf_document,
							page,
							search_string);
}

static int
pdf_document_find_get_n_results (EvDocumentFind *document_find, int page)
{
	PdfDocumentSearch *search = PDF_DOCUMENT (document_find)->search;

	if (search) {
		return g_list_length (search->pages[page]);
	} else {
		return 0;
	}
}

static gboolean
pdf_document_find_get_result (EvDocumentFind *document_find,
			      int             page,
			      int             n_result,
			      EvRectangle    *rectangle)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_find);
	PdfDocumentSearch *search = pdf_document->search;
	PopplerPage *poppler_page;
	PopplerRectangle *r;
	double height;

	if (search == NULL)
		return FALSE;

	r = (PopplerRectangle *) g_list_nth_data (search->pages[page],
						  n_result);
	if (r == NULL)
		return FALSE;

	poppler_page = poppler_document_get_page (pdf_document->document, page);
	poppler_page_get_size (poppler_page, NULL, &height);
	rectangle->x1 = r->x1;
	rectangle->y1 = height - r->y2;
	rectangle->x2 = r->x2;
	rectangle->y2 = height - r->y1;
	g_object_unref (poppler_page);
		
	return TRUE;
}

static int
pdf_document_find_page_has_results (EvDocumentFind *document_find,
				    int             page)
{
	PdfDocumentSearch *search = PDF_DOCUMENT (document_find)->search;

	return search && search->pages[page] != NULL;
}

static double
pdf_document_find_get_progress (EvDocumentFind *document_find)
{
	PdfDocumentSearch *search;
	int n_pages, pages_done;

	search = PDF_DOCUMENT (document_find)->search;

	if (search == NULL) {
		return 0;
	}

	n_pages = pdf_document_get_n_pages (EV_DOCUMENT (document_find));
	if (search->search_page > search->start_page) {
		pages_done = search->search_page - search->start_page + 1;
	} else if (search->search_page == search->start_page) {
		pages_done = n_pages;
	} else {
		pages_done = n_pages - search->start_page + search->search_page;
	}

	return pages_done / (double) n_pages;
}

static void
pdf_document_find_cancel (EvDocumentFind *document)
{
        PdfDocument *pdf_document = PDF_DOCUMENT (document);

	if (pdf_document->search) {
		pdf_document_search_free (pdf_document->search);
		pdf_document->search = NULL;
	}
}

static void
pdf_document_find_iface_init (EvDocumentFindIface *iface)
{
        iface->begin = pdf_document_find_begin;
	iface->get_n_results = pdf_document_find_get_n_results;
	iface->get_result = pdf_document_find_get_result;
	iface->page_has_results = pdf_document_find_page_has_results;
	iface->get_progress = pdf_document_find_get_progress;
        iface->cancel = pdf_document_find_cancel;
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
	gboolean rotate, landscape;
	gdouble  width, height;
	gdouble  pwidth, pheight;
	gdouble  xscale, yscale;
#endif

	g_return_if_fail (pdf_document->print_ctx != NULL);

	poppler_page = poppler_document_get_page (pdf_document->document, rc->page);
	
#ifdef HAVE_CAIRO_PRINT
	x = (ctx->pages_printed % ctx->pages_per_sheet) % ctx->pages_x;
	y = (ctx->pages_printed % ctx->pages_per_sheet) / ctx->pages_x;
	poppler_page_get_size (poppler_page, &page_width, &page_height);

	if (page_width > page_height && page_width > ctx->paper_width) {
		rotate = TRUE;
	} else {
		rotate = FALSE;
	}

	landscape = (ctx->paper_width > ctx->paper_height);

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
		
		cairo_translate (ctx->cr, width, 0);
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

#ifdef HAVE_POPPLER_PAGE_RENDER_FOR_PRINTING
	poppler_page_render_for_printing (poppler_page, ctx->cr);
#else
#ifdef HAVE_POPPLER_PAGE_RENDER
	poppler_page_render (poppler_page, ctx->cr);
#endif
#endif

	ctx->pages_printed++;
			
	cairo_restore (ctx->cr);
#else /* HAVE_CAIRO_PRINT */
	if (ctx->format == EV_FILE_FORMAT_PS)
		poppler_page_render_to_ps (poppler_page, ctx->ps_file);
#endif /* HAVE_CAIRO_PRINT */
	
	g_object_unref (poppler_page);
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
#if GTK_CHECK_VERSION (2, 11, 1)
		EV_FILE_EXPORTER_CAN_NUMBER_UP |
#endif
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
	PdfDocument *pdf_document;
	double width_points, height_points;
	gint width, height;

	pdf_document = PDF_DOCUMENT (selection);
	set_rc_data (pdf_document, rc);

	poppler_page_get_size (POPPLER_PAGE (rc->data),
			       &width_points, &height_points);
	width = (int) ((width_points * rc->scale) + 0.5);
	height = (int) ((height_points * rc->scale) + 0.5);

#ifdef HAVE_POPPLER_PAGE_RENDER
	cairo_t *cr;

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
	poppler_page_render_selection (POPPLER_PAGE (rc->data),
				       cr,
				       (PopplerRectangle *)points,
				       (PopplerRectangle *)old_points,
				       (PopplerSelectionStyle)style,
				       text,
				       base);
	cairo_destroy (cr);
#else /* HAVE_POPPLER_PAGE_RENDER */
	GdkPixbuf *pixbuf;
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 TRUE, 8,
				 width, height);

	poppler_page_render_selection_to_pixbuf (POPPLER_PAGE (rc->data),
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
	PdfDocument *pdf_document = PDF_DOCUMENT (selection);
	PopplerPage *poppler_page;
	PopplerRectangle r;
	double height;
	char *retval;
	
	poppler_page = poppler_document_get_page (pdf_document->document, rc->page);
	g_return_val_if_fail (poppler_page != NULL, NULL);

	poppler_page_get_size (poppler_page, NULL, &height);
	r.x1 = points->x1;
	r.y1 = height - points->y2;
	r.x2 = points->x2;
	r.y2 = height - points->y1;

	retval = poppler_page_get_text (poppler_page,
					(PopplerSelectionStyle)style,
					&r);

	g_object_unref (poppler_page);

	return retval;
}

static GdkRegion *
pdf_selection_get_selection_region (EvSelection     *selection,
				    EvRenderContext *rc,
				    EvSelectionStyle style,
				    EvRectangle     *points)
{
	PdfDocument *pdf_document;
	GdkRegion *retval;

	pdf_document = PDF_DOCUMENT (selection);

	set_rc_data (pdf_document, rc);

	retval = poppler_page_get_selection_region ((PopplerPage *)rc->data,
						    rc->scale,
						    (PopplerSelectionStyle)style,
						    (PopplerRectangle *) points);
	return retval;
}

static GdkRegion *
pdf_selection_get_selection_map (EvSelection     *selection,
				 EvRenderContext *rc)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	PopplerRectangle points;
	GdkRegion *retval;

	pdf_document = PDF_DOCUMENT (selection);
	poppler_page = poppler_document_get_page (pdf_document->document,
						  rc->page);

	points.x1 = 0.0;
	points.y1 = 0.0;
	poppler_page_get_size (poppler_page, &(points.x2), &(points.y2));
	retval = poppler_page_get_selection_region (poppler_page, 1.0,
						    POPPLER_SELECTION_GLYPH,
						    &points);
	g_object_unref (poppler_page);

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
				    gint             page)
{
 	PdfDocument *pdf_document;
 	PopplerPage *poppler_page;
 	GList *retval = NULL;
 	GList *fields;
 	GList *list;
 	double height;

 	pdf_document = PDF_DOCUMENT (document);
 	poppler_page = poppler_document_get_page (pdf_document->document, page);
 	fields = poppler_page_get_form_field_mapping (poppler_page);
 	poppler_page_get_size (poppler_page, NULL, &height);

 	for (list = fields; list; list = list->next) {
 		PopplerFormFieldMapping *mapping;
 		EvFormFieldMapping *field_mapping;
		EvFormField *ev_field;

 		mapping = (PopplerFormFieldMapping *)list->data;

		ev_field = ev_form_field_from_poppler_field (mapping->field);
		if (!ev_field)
			continue;

 		field_mapping = g_new0 (EvFormFieldMapping, 1);
		field_mapping->x1 = mapping->area.x1;
		field_mapping->x2 = mapping->area.x2;
		field_mapping->y1 = height - mapping->area.y2;
		field_mapping->y2 = height - mapping->area.y1;
		field_mapping->field = ev_field;
		field_mapping->field->page = page;
		
		retval = g_list_prepend (retval, field_mapping);
	}
	
	poppler_page_free_form_field_mapping (fields);
	g_object_unref (poppler_page);

	return g_list_reverse (retval);
}

static gchar *
pdf_document_forms_form_field_text_get_text (EvDocumentForms *document,
					     EvFormField     *field)
	
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;
	gchar *text;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return NULL;
	
	text = poppler_form_field_text_get_text (poppler_field);
	g_object_unref (poppler_field);

	return text;
}

static void
pdf_document_forms_form_field_text_set_text (EvDocumentForms *document, 
					     EvFormField     *field,
					     const gchar     *text)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return;
	poppler_form_field_text_set_text (poppler_field, text);
	g_object_unref (poppler_field);
}

static void
pdf_document_forms_form_field_button_set_state (EvDocumentForms *document, 
						EvFormField     *field,
						gboolean         state)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return;
	
	poppler_form_field_button_set_state (poppler_field, state);
	g_object_unref (poppler_field);
}

static gboolean
pdf_document_forms_form_field_button_get_state (EvDocumentForms *document, 
						EvFormField     *field)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;
	gboolean state;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return FALSE;

	state = poppler_form_field_button_get_state (poppler_field);
	g_object_unref (poppler_field);

	return state;
}

static gchar *
pdf_document_forms_form_field_choice_get_item (EvDocumentForms *document, 
					       EvFormField     *field,
					       gint             index)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;
	gchar *text;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return NULL;

	text = poppler_form_field_choice_get_item (poppler_field, index);
	g_object_unref (poppler_field);

	return text;
}

static int
pdf_document_forms_form_field_choice_get_n_items (EvDocumentForms *document, 
						  EvFormField     *field)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;
	gint n_items;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return -1;
	
	n_items = poppler_form_field_choice_get_n_items (poppler_field);
	g_object_unref (poppler_field);

	return n_items;
}

static gboolean
pdf_document_forms_form_field_choice_is_item_selected (EvDocumentForms *document, 
						       EvFormField     *field,
						       gint             index)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;
	gboolean selected;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return FALSE;

	selected = poppler_form_field_choice_is_item_selected (poppler_field, index);
	g_object_unref (poppler_field);

	return selected;
}

static void
pdf_document_forms_form_field_choice_select_item (EvDocumentForms *document, 
						  EvFormField     *field,
						  gint             index)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return;

	poppler_form_field_choice_select_item (poppler_field, index);
	g_object_unref (poppler_field);
}

static void
pdf_document_forms_form_field_choice_toggle_item (EvDocumentForms *document, 
						  EvFormField     *field,
						  gint             index)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return;

	poppler_form_field_choice_toggle_item (poppler_field, index);
	g_object_unref (poppler_field);
}

static void
pdf_document_forms_form_field_choice_unselect_all (EvDocumentForms *document, 
						   EvFormField     *field)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return;
	
	poppler_form_field_choice_unselect_all (poppler_field);
	g_object_unref (poppler_field);
}

static void
pdf_document_forms_form_field_choice_set_text (EvDocumentForms *document,
					       EvFormField     *field,
					       const gchar     *text)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return;
	
	poppler_form_field_choice_set_text (poppler_field, text);
	g_object_unref (poppler_field);
}

static gchar *
pdf_document_forms_form_field_choice_get_text (EvDocumentForms *document,
					       EvFormField     *field)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerFormField *poppler_field;
	gchar *text;

	poppler_field = poppler_document_get_form_field (pdf_document->document, field->id);
	if (!poppler_field)
		return NULL;

	text = poppler_form_field_choice_get_text (poppler_field);
	g_object_unref (poppler_field);

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

