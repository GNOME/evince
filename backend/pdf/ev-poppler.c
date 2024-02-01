/* this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2018, Evangelos Rigas <erigas@rnd2.org>
 * Copyright (C) 2009, Juanjo Marín <juanj.marin@juntadeandalucia.es>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <math.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <gtk/gtk.h>
#include <poppler.h>
#include <poppler-document.h>
#include <poppler-page.h>
#include <poppler-features.h>
#ifdef HAVE_CAIRO_PDF
#include <cairo-pdf.h>
#endif
#ifdef HAVE_CAIRO_PS
#include <cairo-ps.h>
#endif
#include <glib/gi18n-lib.h>

#include "ev-poppler.h"
#include "ev-file-exporter.h"
#include "ev-document-find.h"
#include "ev-document-misc.h"
#include "ev-document-links.h"
#include "ev-document-images.h"
#include "ev-document-fonts.h"
#include "ev-document-security.h"
#include "ev-document-transition.h"
#include "ev-document-forms.h"
#include "ev-document-layers.h"
#include "ev-document-media.h"
#include "ev-document-print.h"
#include "ev-document-annotations.h"
#include "ev-document-attachments.h"
#include "ev-document-text.h"
#include "ev-form-field-private.h"
#include "ev-selection.h"
#include "ev-transition-effect.h"
#include "ev-attachment.h"
#include "ev-image.h"
#include "ev-media.h"
#include "ev-file-helpers.h"

#if (defined (HAVE_CAIRO_PDF) || defined (HAVE_CAIRO_PS))
#define HAVE_CAIRO_PRINT
#endif

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
	EvDocumentClass parent_class;
};

struct _PdfDocument
{
	EvDocument parent_instance;

	PopplerDocument *document;
	gchar *password;
	gboolean forms_modified;
	gboolean annots_modified;

	PopplerFontsIter *fonts_iter;
	gboolean missing_fonts;

	PdfPrintContext *print_ctx;

	GHashTable *annots;
};

static void pdf_document_security_iface_init             (EvDocumentSecurityInterface    *iface);
static void pdf_document_document_links_iface_init       (EvDocumentLinksInterface       *iface);
static void pdf_document_document_images_iface_init      (EvDocumentImagesInterface      *iface);
static void pdf_document_document_forms_iface_init       (EvDocumentFormsInterface       *iface);
static void pdf_document_document_fonts_iface_init       (EvDocumentFontsInterface       *iface);
static void pdf_document_document_layers_iface_init      (EvDocumentLayersInterface      *iface);
static void pdf_document_document_print_iface_init       (EvDocumentPrintInterface       *iface);
static void pdf_document_document_annotations_iface_init (EvDocumentAnnotationsInterface *iface);
static void pdf_document_document_attachments_iface_init (EvDocumentAttachmentsInterface *iface);
static void pdf_document_document_media_iface_init       (EvDocumentMediaInterface       *iface);
static void pdf_document_find_iface_init                 (EvDocumentFindInterface        *iface);
static void pdf_document_file_exporter_iface_init        (EvFileExporterInterface        *iface);
static void pdf_selection_iface_init                     (EvSelectionInterface           *iface);
static void pdf_document_page_transition_iface_init      (EvDocumentTransitionInterface  *iface);
static void pdf_document_text_iface_init                 (EvDocumentTextInterface        *iface);
static int  pdf_document_get_n_pages			 (EvDocument                     *document);

static EvLinkDest *ev_link_dest_from_dest    (PdfDocument       *pdf_document,
					      PopplerDest       *dest);
static EvLink     *ev_link_from_action       (PdfDocument       *pdf_document,
					      PopplerAction     *action);
static void        pdf_print_context_free    (PdfPrintContext   *ctx);
static gboolean    attachment_save_to_buffer (PopplerAttachment *attachment,
					      gchar            **buffer,
					      gsize             *buffer_size,
					      GError           **error);

G_DEFINE_TYPE_WITH_CODE (PdfDocument,
			 pdf_document,
			 EV_TYPE_DOCUMENT,
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_SECURITY,
						pdf_document_security_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LINKS,
						pdf_document_document_links_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_IMAGES,
						pdf_document_document_images_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FORMS,
						pdf_document_document_forms_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FONTS,
						pdf_document_document_fonts_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LAYERS,
						pdf_document_document_layers_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_PRINT,
						pdf_document_document_print_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_ANNOTATIONS,
						pdf_document_document_annotations_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_ATTACHMENTS,
						pdf_document_document_attachments_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_MEDIA,
						pdf_document_document_media_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND,
						pdf_document_find_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER,
						pdf_document_file_exporter_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_SELECTION,
						pdf_selection_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_TRANSITION,
						pdf_document_page_transition_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_TEXT,
						pdf_document_text_iface_init));

static void
pdf_document_dispose (GObject *object)
{
	PdfDocument *pdf_document = PDF_DOCUMENT(object);

	if (pdf_document->print_ctx) {
		pdf_print_context_free (pdf_document->print_ctx);
		pdf_document->print_ctx = NULL;
	}

	if (pdf_document->annots) {
		g_hash_table_destroy (pdf_document->annots);
		pdf_document->annots = NULL;
	}

        g_clear_object (&pdf_document->document);
        g_clear_pointer (&pdf_document->fonts_iter, poppler_fonts_iter_free);

	G_OBJECT_CLASS (pdf_document_parent_class)->dispose (object);
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

	retval = poppler_document_save (pdf_document->document,
					uri, &poppler_error);
	if (retval) {
		pdf_document->forms_modified = FALSE;
		pdf_document->annots_modified = FALSE;
		ev_document_set_modified (EV_DOCUMENT (document), FALSE);
	} else {
		convert_error (poppler_error, error);
	}

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

static gboolean
pdf_document_load_stream (EvDocument          *document,
                          GInputStream        *stream,
                          EvDocumentLoadFlags  flags,
                          GCancellable        *cancellable,
                          GError             **error)
{
        GError *err = NULL;
        PdfDocument *pdf_document = PDF_DOCUMENT (document);

        pdf_document->document =
                poppler_document_new_from_stream (stream, -1,
                                                  pdf_document->password,
                                                  cancellable,
                                                  &err);

        if (pdf_document->document == NULL) {
                convert_error (err, error);
                return FALSE;
        }

        return TRUE;
}

static gboolean
pdf_document_load_gfile (EvDocument          *document,
                         GFile               *file,
                         EvDocumentLoadFlags  flags,
                         GCancellable        *cancellable,
                         GError             **error)
{
        GError *err = NULL;
        PdfDocument *pdf_document = PDF_DOCUMENT (document);

        pdf_document->document =
                poppler_document_new_from_gfile (file,
                                                 pdf_document->password,
                                                 cancellable,
                                                 &err);

        if (pdf_document->document == NULL) {
                convert_error (err, error);
                return FALSE;
        }

        return TRUE;
}

static gboolean
pdf_document_load_fd (EvDocument          *document,
                      int                  fd,
                      EvDocumentLoadFlags  flags,
                      GCancellable        *cancellable,
                      GError             **error)
{
        GError *err = NULL;
        PdfDocument *pdf_document = PDF_DOCUMENT (document);

        /* Note: this consumes @fd */
        pdf_document->document =
                poppler_document_new_from_fd (fd,
                                              pdf_document->password,
                                              &err);

        if (pdf_document->document == NULL) {
                convert_error (err, error);
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

static cairo_surface_t *
pdf_page_render (PopplerPage     *page,
		 gint             width,
		 gint             height,
		 EvRenderContext *rc)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	double page_width, page_height;
	double xscale, yscale;

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

	poppler_page_get_size (page,
			       &page_width, &page_height);

	ev_render_context_compute_scales (rc, page_width, page_height, &xscale, &yscale);
	cairo_scale (cr, xscale, yscale);
	cairo_rotate (cr, rc->rotation * G_PI / 180.0);
	poppler_page_render (page, cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
	cairo_set_source_rgb (cr, 1., 1., 1.);
	cairo_paint (cr);

	cairo_destroy (cr);

	return surface;
}

static cairo_surface_t *
pdf_document_render (EvDocument      *document,
		     EvRenderContext *rc)
{
	PopplerPage *poppler_page;
	double width_points, height_points;
	gint width, height;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	poppler_page_get_size (poppler_page,
			       &width_points, &height_points);

	ev_render_context_compute_transformed_size (rc, width_points, height_points,
						    &width, &height);
	return pdf_page_render (poppler_page,
				width, height, rc);
}

static GdkPixbuf *
make_thumbnail_for_page (PopplerPage     *poppler_page,
			 EvRenderContext *rc,
			 gint             width,
			 gint             height)
{
	GdkPixbuf *pixbuf;
	cairo_surface_t *surface;

	ev_document_fc_mutex_lock ();
	surface = pdf_page_render (poppler_page, width, height, rc);
	ev_document_fc_mutex_unlock ();

	pixbuf = ev_document_misc_pixbuf_from_surface (surface);
	cairo_surface_destroy (surface);

	return pixbuf;
}

static GdkPixbuf *
pdf_document_get_thumbnail (EvDocument      *document,
			    EvRenderContext *rc)
{
	PopplerPage *poppler_page;
	cairo_surface_t *surface;
	GdkPixbuf *pixbuf = NULL;
	double page_width, page_height;
	gint width, height;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	poppler_page_get_size (poppler_page,
			       &page_width, &page_height);

	ev_render_context_compute_transformed_size (rc, page_width, page_height,
						    &width, &height);

	surface = poppler_page_get_thumbnail (poppler_page);
	if (surface) {
		pixbuf = ev_document_misc_pixbuf_from_surface (surface);
		cairo_surface_destroy (surface);
	}

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

	return pixbuf;
}

static cairo_surface_t *
pdf_document_get_thumbnail_surface (EvDocument      *document,
				    EvRenderContext *rc)
{

	PopplerPage *poppler_page;
	cairo_surface_t *surface;
	double page_width, page_height;
	gint width, height;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	poppler_page_get_size (poppler_page,
			       &page_width, &page_height);

	ev_render_context_compute_transformed_size (rc, page_width, page_height,
						    &width, &height);

	surface = poppler_page_get_thumbnail (poppler_page);
	if (surface) {
		int surface_width = (rc->rotation == 90 || rc->rotation == 270) ?
			cairo_image_surface_get_height (surface) :
			cairo_image_surface_get_width (surface);

		if (surface_width == width) {
			cairo_surface_t *rotated_surface;

			rotated_surface = ev_document_misc_surface_rotate_and_scale (surface, width, height, rc->rotation);
			cairo_surface_destroy (surface);
			return rotated_surface;
		} else {
			/* The provided thumbnail has a different size */
			cairo_surface_destroy (surface);
		}
	}

	ev_document_fc_mutex_lock ();
	surface = pdf_page_render (poppler_page, width, height, rc);
	ev_document_fc_mutex_unlock ();

	return surface;
}

static EvDocumentInfo *
pdf_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	PopplerPageLayout layout;
	PopplerPageMode mode;
	PopplerViewerPreferences view_prefs;
	PopplerPermissions permissions;
	char *metadata;
	gboolean linearized;
        GDateTime *created_datetime = NULL;
        GDateTime *modified_datetime = NULL;

	info = ev_document_info_new ();

	info->fields_mask |= EV_DOCUMENT_INFO_LAYOUT |
			     EV_DOCUMENT_INFO_START_MODE |
		             EV_DOCUMENT_INFO_PERMISSIONS |
			     EV_DOCUMENT_INFO_UI_HINTS |
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
		      "creation-datetime", &created_datetime,
		      "mod-datetime", &modified_datetime,
		      "linearized", &linearized,
		      "metadata", &metadata,
		      NULL);

        if (info->title)
                info->fields_mask |= EV_DOCUMENT_INFO_TITLE;
        if (info->format)
                info->fields_mask |= EV_DOCUMENT_INFO_FORMAT;
        if (info->author)
                info->fields_mask |= EV_DOCUMENT_INFO_AUTHOR;
        if (info->subject)
                info->fields_mask |= EV_DOCUMENT_INFO_SUBJECT;
        if (info->keywords)
                info->fields_mask |= EV_DOCUMENT_INFO_KEYWORDS;
        if (info->creator)
                info->fields_mask |= EV_DOCUMENT_INFO_CREATOR;
        if (info->producer)
                info->fields_mask |= EV_DOCUMENT_INFO_PRODUCER;

        ev_document_info_take_created_datetime (info, created_datetime);
        ev_document_info_take_modified_datetime (info, modified_datetime);

	if (metadata != NULL) {
                ev_document_info_set_from_xmp (info, metadata, -1);
		g_free (metadata);
	}

	info->n_pages = poppler_document_get_n_pages (PDF_DOCUMENT (document)->document);

	if (info->n_pages > 0) {
		PopplerPage *poppler_page;

		poppler_page = poppler_document_get_page (PDF_DOCUMENT (document)->document, 0);
		poppler_page_get_size (poppler_page, &(info->paper_width), &(info->paper_height));
		g_object_unref (poppler_page);

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
			break;
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

	info->linearized = linearized ? g_strdup (_("Yes")) : g_strdup (_("No"));

	info->contains_js = poppler_document_has_javascript (PDF_DOCUMENT (document)->document) ?
	                    EV_DOCUMENT_CONTAINS_JS_YES : EV_DOCUMENT_CONTAINS_JS_NO;
        info->fields_mask |= EV_DOCUMENT_INFO_CONTAINS_JS;

	return info;
}

static gboolean
pdf_document_get_backend_info (EvDocument *document, EvDocumentBackendInfo *info)
{
	PopplerBackend backend;

	backend = poppler_get_backend ();
	switch (backend) {
		case POPPLER_BACKEND_CAIRO:
			info->name = "poppler/cairo";
			break;
		case POPPLER_BACKEND_SPLASH:
			info->name = "poppler/splash";
			break;
		default:
			info->name = "poppler/unknown";
			break;
	}

	info->version = poppler_get_version ();

	return TRUE;
}

static gboolean
pdf_document_support_synctex (EvDocument *document)
{
	return TRUE;
}

static void
pdf_document_class_init (PdfDocumentClass *klass)
{
	GObjectClass    *g_object_class = G_OBJECT_CLASS (klass);
	EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

	g_object_class->dispose = pdf_document_dispose;

	ev_document_class->save = pdf_document_save;
	ev_document_class->load = pdf_document_load;
        ev_document_class->load_stream = pdf_document_load_stream;
        ev_document_class->load_gfile = pdf_document_load_gfile;
	ev_document_class->get_n_pages = pdf_document_get_n_pages;
	ev_document_class->get_page = pdf_document_get_page;
	ev_document_class->get_page_size = pdf_document_get_page_size;
	ev_document_class->get_page_label = pdf_document_get_page_label;
	ev_document_class->render = pdf_document_render;
	ev_document_class->get_thumbnail = pdf_document_get_thumbnail;
	ev_document_class->get_thumbnail_surface = pdf_document_get_thumbnail_surface;
	ev_document_class->get_info = pdf_document_get_info;
	ev_document_class->get_backend_info = pdf_document_get_backend_info;
	ev_document_class->support_synctex = pdf_document_support_synctex;
        ev_document_class->load_fd = pdf_document_load_fd;
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

static void
pdf_document_security_iface_init (EvDocumentSecurityInterface *iface)
{
	iface->has_document_security = pdf_document_has_document_security;
	iface->set_password = pdf_document_set_password;
}

static void
pdf_document_fonts_scan (EvDocumentFonts *document_fonts)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_fonts);
	PopplerFontInfo *font_info;
	PopplerFontsIter *fonts_iter;
	int n_pages;

	g_return_if_fail (PDF_IS_DOCUMENT (document_fonts));

	font_info = poppler_font_info_new (pdf_document->document);
	n_pages = pdf_document_get_n_pages (EV_DOCUMENT (document_fonts));
	poppler_font_info_scan (font_info, n_pages, &fonts_iter);

	g_clear_pointer (&pdf_document->fonts_iter, poppler_fonts_iter_free);
	pdf_document->fonts_iter = fonts_iter;

	poppler_font_info_free (font_info);
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

static gboolean
is_standard_font (const gchar *name, PopplerFontType type)
{
	/* list borrowed from Poppler: poppler/GfxFont.cc */
	static const char *base_14_subst_fonts[14] = {
	  "Courier",
	  "Courier-Oblique",
	  "Courier-Bold",
	  "Courier-BoldOblique",
	  "Helvetica",
	  "Helvetica-Oblique",
	  "Helvetica-Bold",
	  "Helvetica-BoldOblique",
	  "Times-Roman",
	  "Times-Italic",
	  "Times-Bold",
	  "Times-BoldItalic",
	  "Symbol",
	  "ZapfDingbats"
	};
	unsigned int i;

	/* The Standard 14 fonts are all Type 1 fonts. A non embedded TrueType
	 * font with the same name is not a Standard 14 font. */
	if (type != POPPLER_FONT_TYPE_TYPE1)
		return FALSE;

	for (i = 0; i < G_N_ELEMENTS (base_14_subst_fonts); i++) {
		if (g_str_equal (name, base_14_subst_fonts[i]))
			return TRUE;
	}
	return FALSE;
}

static const gchar *
pdf_document_fonts_get_fonts_summary (EvDocumentFonts *document_fonts)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document_fonts);

	if (pdf_document->missing_fonts)
		return _("This document contains non-embedded fonts that are not from the "
			 "PDF Standard 14 fonts. If the substitute fonts selected by fontconfig "
			 "are not the same as the fonts used to create the PDF, the rendering may "
			 "not be correct.");
	else
		return _("All fonts are either standard or embedded.");
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
		PopplerFontType type;
		const char *type_str;
		const char *embedded;
		const char *standard_str = "";
		const gchar *substitute;
		const gchar *filename;
		const gchar *encoding;
		char *details;

		name = poppler_fonts_iter_get_name (iter);

		if (name == NULL) {
			name = _("No name");
		}

		encoding = poppler_fonts_iter_get_encoding (iter);
		if (!encoding) {
			/* translators: When a font type does not have
			   encoding information or it is unknown.  Example:
			   Encoding: None
			*/
			encoding = _("None");
		}

		type = poppler_fonts_iter_get_font_type (iter);
		type_str = font_type_to_string (type);

		if (poppler_fonts_iter_is_embedded (iter)) {
			if (poppler_fonts_iter_is_subset (iter))
				embedded = _("Embedded subset");
			else
				embedded = _("Embedded");
		} else {
			embedded = _("Not embedded");
			if (is_standard_font (name, type)) {
				/* Translators: string starting with a space
				 * because it is directly appended to the font
				 * type. Example:
				 * "Type 1 (One of the Standard 14 Fonts)"
				 */
				standard_str = _(" (One of the Standard 14 Fonts)");
			} else {
				/* Translators: string starting with a space
				 * because it is directly appended to the font
				 * type. Example:
				 * "TrueType (Not one of the Standard 14 Fonts)"
				 */
				standard_str = _(" (Not one of the Standard 14 Fonts)");
				pdf_document->missing_fonts = TRUE;
			}
		}

		substitute = poppler_fonts_iter_get_substitute_name (iter);
		filename = poppler_fonts_iter_get_file_name (iter);

		if (substitute && filename)
			/* Translators: string is a concatenation of previous
			 * translated strings to indicate the fonts properties
			 * in a PDF document.
			 *
			 * Example:
			 * Type 1 (One of the standard 14 Fonts)
			 * Not embedded
			 * Substituting with TeXGyreTermes-Regular
			 * (/usr/share/textmf/.../texgyretermes-regular.otf)
			 */
			details = g_markup_printf_escaped (_("%s%s\n"
			                                     "Encoding: %s\n"
			                                     "%s\n"
			                                     "Substituting with <b>%s</b>\n"
			                                     "(%s)"),
							   type_str, standard_str,
							   encoding, embedded,
							   substitute, filename);
		else
			/* Translators: string is a concatenation of previous
			 * translated strings to indicate the fonts properties
			 * in a PDF document.
			 *
			 * Example:
			 * TrueType (CID)
			 * Encoding: Custom
			 * Embedded subset
			 */
			details = g_markup_printf_escaped (_("%s%s\n"
			                                     "Encoding: %s\n"
			                                     "%s"),
							   type_str, standard_str,
							   encoding, embedded);

		gtk_list_store_append (GTK_LIST_STORE (model), &list_iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &list_iter,
				    EV_DOCUMENT_FONTS_COLUMN_NAME, name,
				    EV_DOCUMENT_FONTS_COLUMN_DETAILS, details,
				    -1);

		g_free (details);
	} while (poppler_fonts_iter_next (iter));
}

static void
pdf_document_document_fonts_iface_init (EvDocumentFontsInterface *iface)
{
	iface->fill_model = pdf_document_fonts_fill_model;
	iface->get_fonts_summary = pdf_document_fonts_get_fonts_summary;
	iface->scan = pdf_document_fonts_scan;
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
			/* for evince we ensure that bottom <= top and left <= right */
			/* also evince has its origin in the top left, so we invert the y axis. */
			ev_dest = ev_link_dest_new_fitr (dest->page_num - 1,
							 MIN (dest->left, dest->right),
							 height - MIN (height, MIN (dest->bottom, dest->top)),
							 MAX (dest->left, dest->right),
							 height - MIN (height, MAX (dest->bottom, dest->top)));
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
		           "bug report in Evince issue tracker "
		           "(https://gitlab.gnome.org/GNOME/evince/issues) with a testcase.",
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
			g_object_unref (dest);
		}
			break;
	        case POPPLER_ACTION_GOTO_REMOTE: {
			EvLinkDest *dest;

			dest = ev_link_dest_from_dest (pdf_document, action->goto_remote.dest);
			ev_action = ev_link_action_new_remote (dest,
							       action->goto_remote.file_name);
			g_object_unref (dest);

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
	        case POPPLER_ACTION_RENDITION:
			unimplemented_action = "POPPLER_ACTION_RENDITION";
			break;
	        case POPPLER_ACTION_OCG_STATE: {
			GList *on_list = NULL;
			GList *off_list = NULL;
			GList *toggle_list = NULL;
			GList *l, *m;

			for (l = action->ocg_state.state_list; l; l = g_list_next (l)) {
				PopplerActionLayer *action_layer = (PopplerActionLayer *)l->data;

				for (m = action_layer->layers; m; m = g_list_next (m)) {
					PopplerLayer *layer = (PopplerLayer *)m->data;
					EvLayer      *ev_layer;

					ev_layer = ev_layer_new (poppler_layer_is_parent (layer),
								 poppler_layer_get_radio_button_group_id (layer));
					g_object_set_data_full (G_OBJECT (ev_layer),
								"poppler-layer",
								g_object_ref (layer),
								(GDestroyNotify)g_object_unref);

					switch (action_layer->action) {
					case POPPLER_ACTION_LAYER_ON:
						on_list = g_list_prepend (on_list, ev_layer);
						break;
					case POPPLER_ACTION_LAYER_OFF:
						off_list = g_list_prepend (off_list, ev_layer);
						break;
					case POPPLER_ACTION_LAYER_TOGGLE:
						toggle_list = g_list_prepend (toggle_list, ev_layer);
						break;
					}
				}
			}

			/* The action takes the ownership of the lists */
			ev_action = ev_link_action_new_layers_state (g_list_reverse (on_list),
								     g_list_reverse (off_list),
								     g_list_reverse (toggle_list));


		}
			break;
	        case POPPLER_ACTION_JAVASCRIPT:
			unimplemented_action = "POPPLER_ACTION_JAVASCRIPT";
			break;
	        case POPPLER_ACTION_RESET_FORM: {
			gboolean  exclude_reset_fields;
			GList    *reset_fields = NULL;
			GList    *iter;

			for (iter = action->reset_form.fields; iter; iter = iter->next)
				reset_fields = g_list_prepend (reset_fields, g_strdup ((char *) iter->data));

			exclude_reset_fields = action->reset_form.exclude;

			/* The action takes the ownership of the list */
			ev_action = ev_link_action_new_reset_form (g_list_reverse (reset_fields),
								   exclude_reset_fields);
			break;
		}
	        case POPPLER_ACTION_UNKNOWN:
			unimplemented_action = "POPPLER_ACTION_UNKNOWN";
	}

	if (unimplemented_action) {
		g_warning ("Unimplemented action: %s, please post a bug report "
			   "in Evince issue tracker (https://gitlab.gnome.org/GNOME/evince/issues) "
			   "with a testcase.", unimplemented_action);
	}

	link = ev_link_new (action->any.title, ev_action);
	if (ev_action)
		g_object_unref (ev_action);

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

		link = ev_link_from_action (pdf_document, action);
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

static EvMappingList *
pdf_document_links_get_links (EvDocumentLinks *document_links,
			      EvPage          *page)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GList *retval = NULL;
	GList *mapping_list;
	GList *list;
	double height;

	pdf_document = PDF_DOCUMENT (document_links);
	poppler_page = POPPLER_PAGE (page->backend_page);
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

	return ev_mapping_list_new (page->index, g_list_reverse (retval), (GDestroyNotify)g_object_unref);
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

static gint
pdf_document_links_find_link_page (EvDocumentLinks  *document_links,
				   const gchar      *link_name)
{
	PdfDocument *pdf_document;
	PopplerDest *dest;
	gint         retval = -1;

	pdf_document = PDF_DOCUMENT (document_links);
	dest = poppler_document_find_dest (pdf_document->document,
					   link_name);
	if (dest) {
		retval = dest->page_num - 1;
		poppler_dest_free (dest);
	}

	return retval;
}

static void
pdf_document_document_links_iface_init (EvDocumentLinksInterface *iface)
{
	iface->has_document_links = pdf_document_links_has_document_links;
	iface->get_links_model = pdf_document_links_get_links_model;
	iface->get_links = pdf_document_links_get_links;
	iface->find_link_dest = pdf_document_links_find_link_dest;
	iface->find_link_page = pdf_document_links_find_link_page;
}

static EvMappingList *
pdf_document_images_get_image_mapping (EvDocumentImages *document_images,
				       EvPage           *page)
{
	GList *retval = NULL;
	PopplerPage *poppler_page;
	GList *mapping_list;
	GList *list;

	poppler_page = POPPLER_PAGE (page->backend_page);
	mapping_list = poppler_page_get_image_mapping (poppler_page);

	for (list = mapping_list; list; list = list->next) {
		PopplerImageMapping *image_mapping;
		EvMapping *ev_image_mapping;

		image_mapping = (PopplerImageMapping *)list->data;

		ev_image_mapping = g_new (EvMapping, 1);

		ev_image_mapping->data = ev_image_new (page->index, image_mapping->image_id);
		ev_image_mapping->area.x1 = image_mapping->area.x1;
		ev_image_mapping->area.y1 = image_mapping->area.y1;
		ev_image_mapping->area.x2 = image_mapping->area.x2;
		ev_image_mapping->area.y2 = image_mapping->area.y2;

		retval = g_list_prepend (retval, ev_image_mapping);
	}

	poppler_page_free_image_mapping (mapping_list);

	return ev_mapping_list_new (page->index, g_list_reverse (retval), (GDestroyNotify)g_object_unref);
}

static GdkPixbuf *
pdf_document_images_get_image (EvDocumentImages *document_images,
			       EvImage          *image)
{
	GdkPixbuf       *retval = NULL;
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

	return retval;
}

static void
pdf_document_document_images_iface_init (EvDocumentImagesInterface *iface)
{
	iface->get_image_mapping = pdf_document_images_get_image_mapping;
	iface->get_image = pdf_document_images_get_image;
}

static GList *
pdf_document_find_find_text (EvDocumentFind *document_find,
			     EvPage         *page,
			     const gchar    *text,
			     EvFindOptions   options)
{
	GList *matches, *l;
	PopplerPage *poppler_page;
	gdouble height;
	GList *retval = NULL;
	guint find_flags = 0;

	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);
	g_return_val_if_fail (text != NULL, NULL);

	poppler_page = POPPLER_PAGE (page->backend_page);

	if (options & EV_FIND_CASE_SENSITIVE)
		find_flags |= POPPLER_FIND_CASE_SENSITIVE;
	else    /* When search is not case sensitive, do also ignore diacritics
	        to broaden our search in order to match on more expected results */
		find_flags |= POPPLER_FIND_IGNORE_DIACRITICS;

	if (options & EV_FIND_WHOLE_WORDS_ONLY)
		find_flags |= POPPLER_FIND_WHOLE_WORDS_ONLY;

	/* Allow to match on text spanning from one line to the next */
	find_flags |= POPPLER_FIND_MULTILINE;
	matches = poppler_page_find_text_with_options (poppler_page, text, (PopplerFindFlags)find_flags);
	if (!matches)
		return NULL;

	poppler_page_get_size (poppler_page, NULL, &height);
	for (l = matches; l && l->data; l = g_list_next (l)) {
		EvFindRectangle *ev_rect = ev_find_rectangle_new ();

		PopplerRectangle *rect = (PopplerRectangle *)l->data;
		ev_rect->x1 = rect->x1;
		ev_rect->x2 = rect->x2;
		/* Invert this for X-style coordinates */
		ev_rect->y1 = height - rect->y2;
		ev_rect->y2 = height - rect->y1;
		ev_rect->next_line = poppler_rectangle_find_get_match_continued (rect);
		ev_rect->after_hyphen = ev_rect->next_line && poppler_rectangle_find_get_ignored_hyphen (rect);
		retval = g_list_prepend (retval, ev_rect);
	}

	g_list_free_full (matches, (GDestroyNotify) poppler_rectangle_free);

	return g_list_reverse (retval);
}

static EvFindOptions
pdf_document_find_get_supported_options (EvDocumentFind *document_find)
{
	return (EvFindOptions)(EV_FIND_CASE_SENSITIVE | EV_FIND_WHOLE_WORDS_ONLY);
}

static void
pdf_document_find_iface_init (EvDocumentFindInterface *iface)
{
        iface->find_text = pdf_document_find_find_text;
	iface->get_supported_options = pdf_document_find_get_supported_options;
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

	g_return_if_fail (pdf_document->print_ctx != NULL);

#ifdef HAVE_CAIRO_PRINT
	cairo_show_page (pdf_document->print_ctx->cr);
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
		EV_FILE_EXPORTER_CAN_NUMBER_UP |
#endif

#ifdef HAVE_CAIRO_PDF
		EV_FILE_EXPORTER_CAN_GENERATE_PDF |
#endif
		EV_FILE_EXPORTER_CAN_GENERATE_PS);
}

static void
pdf_document_file_exporter_iface_init (EvFileExporterInterface *iface)
{
        iface->begin = pdf_document_file_exporter_begin;
	iface->begin_page = pdf_document_file_exporter_begin_page;
        iface->do_page = pdf_document_file_exporter_do_page;
	iface->end_page = pdf_document_file_exporter_end_page;
        iface->end = pdf_document_file_exporter_end;
	iface->get_capabilities = pdf_document_file_exporter_get_capabilities;
}

/* EvDocumentPrint */
static void
pdf_document_print_print_page (EvDocumentPrint *document,
			       EvPage          *page,
			       cairo_t         *cr)
{
	poppler_page_render_for_printing (POPPLER_PAGE (page->backend_page), cr);
}

static void
pdf_document_document_print_iface_init (EvDocumentPrintInterface *iface)
{
	iface->print_page = pdf_document_print_print_page;
}

static void
pdf_selection_render_selection (EvSelection      *selection,
				EvRenderContext  *rc,
				cairo_surface_t **surface,
				EvRectangle      *points,
				EvRectangle      *old_points,
				EvSelectionStyle  style,
				GdkRGBA          *text,
				GdkRGBA          *base)
{
	PopplerPage *poppler_page;
	cairo_t *cr;
	PopplerColor text_color, base_color;
	double width_points, height_points;
	gint width, height;
	double xscale, yscale;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);

	poppler_page_get_size (poppler_page,
			       &width_points, &height_points);
	ev_render_context_compute_scaled_size (rc, width_points, height_points, &width, &height);

	text_color.red = CLAMP ((guint) (text->red * 65535), 0, 65535);
	text_color.green = CLAMP ((guint) (text->green * 65535), 0, 65535);
	text_color.blue = CLAMP ((guint) (text->blue * 65535), 0, 65535);

	base_color.red = CLAMP ((guint) (base->red * 65535), 0, 65535);
	base_color.green = CLAMP ((guint) (base->green * 65535), 0, 65535);
	base_color.blue = CLAMP ((guint) (base->blue * 65535), 0, 65535);

	if (*surface == NULL) {
		*surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						       width, height);

	}

	cr = cairo_create (*surface);
	ev_render_context_compute_scales (rc, width_points, height_points, &xscale, &yscale);
	cairo_scale (cr, xscale, yscale);
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
}

static gchar *
pdf_selection_get_selected_text (EvSelection     *selection,
				 EvPage          *page,
				 EvSelectionStyle style,
				 EvRectangle     *points)
{
	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);

	return poppler_page_get_selected_text (POPPLER_PAGE (page->backend_page),
					       (PopplerSelectionStyle)style,
					       (PopplerRectangle *)points);
}

static cairo_region_t *
create_region_from_poppler_region (cairo_region_t *region,
				   gdouble         xscale,
				   gdouble         yscale)
{
	int n_rects;
	cairo_region_t *retval;

	retval = cairo_region_create ();

	n_rects = cairo_region_num_rectangles (region);
	for (int i = 0; i < n_rects; i++) {
		cairo_rectangle_int_t rect;

		cairo_region_get_rectangle (region, i, &rect);
		rect.x = (int) (rect.x * xscale + 0.5);
		rect.y = (int) (rect.y * yscale + 0.5);
		rect.width = (int) (rect.width * xscale + 0.5);
		rect.height = (int) (rect.height * yscale + 0.5);
		cairo_region_union_rectangle (retval, &rect);
	}

	return retval;
}

static cairo_region_t *
pdf_selection_get_selection_region (EvSelection     *selection,
				    EvRenderContext *rc,
				    EvSelectionStyle style,
				    EvRectangle     *points)
{
	PopplerPage    *poppler_page;
	cairo_region_t *retval, *region;
	double page_width, page_height;
	double xscale, yscale;

	poppler_page = POPPLER_PAGE (rc->page->backend_page);
	region = poppler_page_get_selected_region (poppler_page,
						   1.0,
						   (PopplerSelectionStyle)style,
						   (PopplerRectangle *)points);

	poppler_page_get_size (poppler_page,
			       &page_width, &page_height);
	ev_render_context_compute_scales (rc, page_width, page_height, &xscale, &yscale);
	retval = create_region_from_poppler_region (region, xscale, yscale);
	cairo_region_destroy (region);

	return retval;
}

static void
pdf_selection_iface_init (EvSelectionInterface *iface)
{
        iface->render_selection = pdf_selection_render_selection;
	iface->get_selected_text = pdf_selection_get_selected_text;
        iface->get_selection_region = pdf_selection_get_selection_region;
}


/* EvDocumentText */
static cairo_region_t *
pdf_document_text_get_text_mapping (EvDocumentText *document_text,
				    EvPage         *page)
{
	PopplerPage *poppler_page;
	PopplerRectangle points;
	cairo_region_t *retval;

	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);

	poppler_page = POPPLER_PAGE (page->backend_page);

	points.x1 = 0.0;
	points.y1 = 0.0;
	poppler_page_get_size (poppler_page, &(points.x2), &(points.y2));

	retval = poppler_page_get_selected_region (poppler_page, 1.0,
						   POPPLER_SELECTION_GLYPH,
						   &points);

	return retval;
}

static gchar *
pdf_document_text_get_text (EvDocumentText  *selection,
			    EvPage          *page)
{
	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);

	return poppler_page_get_text (POPPLER_PAGE (page->backend_page));
}

static gboolean
pdf_document_text_get_text_layout (EvDocumentText  *selection,
				   EvPage          *page,
				   EvRectangle    **areas,
				   guint           *n_areas)
{
	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), FALSE);

	return poppler_page_get_text_layout (POPPLER_PAGE (page->backend_page),
					     (PopplerRectangle **)areas, n_areas);
}

static PangoAttrList *
pdf_document_text_get_text_attrs (EvDocumentText *document_text,
				  EvPage         *page)
{
	GList         *backend_attrs_list,  *l;
	PangoAttrList *attrs_list;

	g_return_val_if_fail (POPPLER_IS_PAGE (page->backend_page), NULL);

	backend_attrs_list = poppler_page_get_text_attributes (POPPLER_PAGE (page->backend_page));
	if (!backend_attrs_list)
		return NULL;

	attrs_list = pango_attr_list_new ();
        for (l = backend_attrs_list; l; l = g_list_next (l)) {
                PopplerTextAttributes *backend_attrs = (PopplerTextAttributes *)l->data;
		PangoAttribute        *attr;

		if (backend_attrs->is_underlined) {
			attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
			attr->start_index = backend_attrs->start_index;
			attr->end_index = backend_attrs->end_index;
			pango_attr_list_insert (attrs_list, attr);
		}

		attr = pango_attr_foreground_new (backend_attrs->color.red,
						  backend_attrs->color.green,
						  backend_attrs->color.blue);
		attr->start_index = backend_attrs->start_index;
		attr->end_index = backend_attrs->end_index;
		pango_attr_list_insert (attrs_list, attr);

		if (backend_attrs->font_name) {
			attr = pango_attr_family_new (backend_attrs->font_name);
			attr->start_index = backend_attrs->start_index;
			attr->end_index = backend_attrs->end_index;
			pango_attr_list_insert (attrs_list, attr);
		}

		if (backend_attrs->font_size) {
			attr = pango_attr_size_new (backend_attrs->font_size * PANGO_SCALE);
			attr->start_index = backend_attrs->start_index;
			attr->end_index = backend_attrs->end_index;
			pango_attr_list_insert (attrs_list, attr);
		}
	}

	poppler_page_free_text_attributes (backend_attrs_list);

	return attrs_list;
}

static void
pdf_document_text_iface_init (EvDocumentTextInterface *iface)
{
        iface->get_text_mapping = pdf_document_text_get_text_mapping;
        iface->get_text = pdf_document_text_get_text;
        iface->get_text_layout = pdf_document_text_get_text_layout;
	iface->get_text_attrs = pdf_document_text_get_text_attrs;
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
					   "duration-real", page_transition->duration_real,
					   "angle", page_transition->angle,
					   "scale", page_transition->scale,
					   "rectangular", page_transition->rectangular,
					   NULL);

	poppler_page_transition_free (page_transition);
	g_object_unref (poppler_page);

	return effect;
}

static void
pdf_document_page_transition_iface_init (EvDocumentTransitionInterface *iface)
{
	iface->get_page_duration = pdf_document_get_page_duration;
	iface->get_effect = pdf_document_get_effect;
}

/* Forms */
#if 0
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
#endif

static EvFormField *
ev_form_field_from_poppler_field (PdfDocument      *pdf_document,
				  PopplerFormField *poppler_field)
{
	EvFormField *ev_field = NULL;
	gint         id;
	gdouble      font_size;
	gboolean     is_read_only;
	PopplerAction *action;
	gchar       *alt_ui_name = NULL;

	id = poppler_form_field_get_id (poppler_field);
	font_size = poppler_form_field_get_font_size (poppler_field);
	is_read_only = poppler_form_field_is_read_only (poppler_field);
	action = poppler_form_field_get_action (poppler_field);
	alt_ui_name = poppler_form_field_get_alternate_ui_name (poppler_field);

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
			        case POPPLER_FORM_CHOICE_LIST:
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
	ev_form_field_set_alternate_name (ev_field, alt_ui_name);

	if (action)
		ev_field->activation_link = ev_link_from_action (pdf_document, action);

	return ev_field;
}

static EvMappingList *
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

		ev_field = ev_form_field_from_poppler_field (PDF_DOCUMENT (document), mapping->field);
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

	return retval ? ev_mapping_list_new (page->index,
					     g_list_reverse (retval),
					     (GDestroyNotify)g_object_unref) : NULL;
}

static gboolean
pdf_document_forms_document_is_modified (EvDocumentForms *document)
{
	return PDF_DOCUMENT (document)->forms_modified;
}

static void
pdf_document_forms_reset_form (EvDocumentForms *document,
                               EvLinkAction    *action)
{
	poppler_document_reset_form (PDF_DOCUMENT (document)->document,
	                             ev_link_action_get_reset_fields (action),
	                             ev_link_action_get_exclude_reset_fields (action));
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
	PDF_DOCUMENT (document)->forms_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document), TRUE);
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
	PDF_DOCUMENT (document)->forms_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document), TRUE);
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
	PDF_DOCUMENT (document)->forms_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document), TRUE);
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
	PDF_DOCUMENT (document)->forms_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document), TRUE);
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
	PDF_DOCUMENT (document)->forms_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document), TRUE);
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
	PDF_DOCUMENT (document)->forms_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document), TRUE);
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
pdf_document_document_forms_iface_init (EvDocumentFormsInterface *iface)
{
	iface->get_form_fields = pdf_document_forms_get_form_fields;
	iface->document_is_modified = pdf_document_forms_document_is_modified;
	iface->reset_form = pdf_document_forms_reset_form;
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
poppler_annot_color_to_gdk_rgba (PopplerAnnot *poppler_annot,
				 GdkRGBA      *color)
{
	PopplerColor *poppler_color;

	poppler_color = poppler_annot_get_color (poppler_annot);
	if (poppler_color) {
		color->red = CLAMP ((double) poppler_color->red / 65535.0,   0.0, 1.0);
		color->green = CLAMP ((double) poppler_color->green / 65535.0,   0.0, 1.0),
		color->blue = CLAMP ((double) poppler_color->blue / 65535.0,   0.0, 1.0),
		color->alpha = 1.0;

		g_free (poppler_color);
	} else { /* default color */
		*color = EV_ANNOTATION_DEFAULT_COLOR;
	}
}

static EvAnnotationTextIcon
get_annot_text_icon (PopplerAnnotText *poppler_annot)
{
	gchar *icon = poppler_annot_text_get_icon (poppler_annot);
	EvAnnotationTextIcon retval;

	if (!icon)
		return EV_ANNOTATION_TEXT_ICON_UNKNOWN;

	if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_NOTE) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_NOTE;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_COMMENT) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_COMMENT;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_KEY) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_KEY;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_HELP) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_HELP;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_NEW_PARAGRAPH) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_NEW_PARAGRAPH;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_PARAGRAPH) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_PARAGRAPH;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_INSERT) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_INSERT;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_CROSS) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_CROSS;
	else if (strcmp (icon, POPPLER_ANNOT_TEXT_ICON_CIRCLE) == 0)
		retval = EV_ANNOTATION_TEXT_ICON_CIRCLE;
	else
		retval = EV_ANNOTATION_TEXT_ICON_UNKNOWN;

	g_free (icon);

	return retval;
}

static const gchar *
get_poppler_annot_text_icon (EvAnnotationTextIcon icon)
{
	switch (icon) {
	case EV_ANNOTATION_TEXT_ICON_NOTE:
		return POPPLER_ANNOT_TEXT_ICON_NOTE;
	case EV_ANNOTATION_TEXT_ICON_COMMENT:
		return POPPLER_ANNOT_TEXT_ICON_COMMENT;
	case EV_ANNOTATION_TEXT_ICON_KEY:
		return POPPLER_ANNOT_TEXT_ICON_KEY;
	case EV_ANNOTATION_TEXT_ICON_HELP:
		return POPPLER_ANNOT_TEXT_ICON_HELP;
	case EV_ANNOTATION_TEXT_ICON_NEW_PARAGRAPH:
		return POPPLER_ANNOT_TEXT_ICON_NEW_PARAGRAPH;
	case EV_ANNOTATION_TEXT_ICON_PARAGRAPH:
		return POPPLER_ANNOT_TEXT_ICON_PARAGRAPH;
	case EV_ANNOTATION_TEXT_ICON_INSERT:
		return POPPLER_ANNOT_TEXT_ICON_INSERT;
	case EV_ANNOTATION_TEXT_ICON_CROSS:
		return POPPLER_ANNOT_TEXT_ICON_CROSS;
	case EV_ANNOTATION_TEXT_ICON_CIRCLE:
		return POPPLER_ANNOT_TEXT_ICON_CIRCLE;
	case EV_ANNOTATION_TEXT_ICON_UNKNOWN:
	default:
		return POPPLER_ANNOT_TEXT_ICON_NOTE;
	}
}

static gboolean
poppler_annot_can_have_popup_window (PopplerAnnot *poppler_annot)
{
	switch (poppler_annot_get_annot_type (poppler_annot)) {
	case POPPLER_ANNOT_TEXT:
	case POPPLER_ANNOT_LINE:
	case POPPLER_ANNOT_SQUARE:
	case POPPLER_ANNOT_CIRCLE:
	case POPPLER_ANNOT_POLYGON:
	case POPPLER_ANNOT_POLY_LINE:
	case POPPLER_ANNOT_HIGHLIGHT:
	case POPPLER_ANNOT_UNDERLINE:
	case POPPLER_ANNOT_SQUIGGLY:
	case POPPLER_ANNOT_STRIKE_OUT:
	case POPPLER_ANNOT_STAMP:
	case POPPLER_ANNOT_CARET:
	case POPPLER_ANNOT_INK:
	case POPPLER_ANNOT_FILE_ATTACHMENT:
		return TRUE;
	default:
		return FALSE;
	}
}

static EvAnnotation *
ev_annot_from_poppler_annot (PopplerAnnot *poppler_annot,
			     EvPage       *page)
{
	EvAnnotation *ev_annot = NULL;
	const gchar  *unimplemented_annot = NULL;
	gboolean reported_annot = FALSE;

	switch (poppler_annot_get_annot_type (poppler_annot)) {
	        case POPPLER_ANNOT_TEXT: {
			PopplerAnnotText *poppler_text;
			EvAnnotationText *ev_annot_text;

			poppler_text = POPPLER_ANNOT_TEXT (poppler_annot);

			ev_annot = ev_annotation_text_new (page);

			ev_annot_text = EV_ANNOTATION_TEXT (ev_annot);
			ev_annotation_text_set_is_open (ev_annot_text,
							poppler_annot_text_get_is_open (poppler_text));
			ev_annotation_text_set_icon (ev_annot_text, get_annot_text_icon (poppler_text));
		}
			break;
	        case POPPLER_ANNOT_FILE_ATTACHMENT: {
			PopplerAnnotFileAttachment *poppler_annot_attachment;
			PopplerAttachment          *poppler_attachment;
			gchar                      *data = NULL;
			gsize                       size;
			GError                     *error = NULL;

			poppler_annot_attachment = POPPLER_ANNOT_FILE_ATTACHMENT (poppler_annot);
			poppler_attachment = poppler_annot_file_attachment_get_attachment (poppler_annot_attachment);

			if (poppler_attachment &&
			    attachment_save_to_buffer (poppler_attachment, &data, &size, &error)) {
				EvAttachment *ev_attachment;
				GDateTime *mtime, *ctime;

				mtime = poppler_attachment_get_mtime (poppler_attachment);
				ctime = poppler_attachment_get_ctime (poppler_attachment);

				ev_attachment = ev_attachment_new (poppler_attachment->name,
								   poppler_attachment->description,
								   mtime, ctime,
								   size, data);
				ev_annot = ev_annotation_attachment_new (page, ev_attachment);
				g_object_unref (ev_attachment);
			} else if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}

			if (poppler_attachment)
				g_object_unref (poppler_attachment);
		}
			break;
		case POPPLER_ANNOT_HIGHLIGHT:
			ev_annot = ev_annotation_text_markup_highlight_new (page);
			break;
	        case POPPLER_ANNOT_STRIKE_OUT:
			ev_annot = ev_annotation_text_markup_strike_out_new (page);
			break;
	        case POPPLER_ANNOT_UNDERLINE:
			ev_annot = ev_annotation_text_markup_underline_new (page);
			break;
	        case POPPLER_ANNOT_SQUIGGLY:
			ev_annot = ev_annotation_text_markup_squiggly_new (page);
			break;
	        case POPPLER_ANNOT_LINK:
	        case POPPLER_ANNOT_WIDGET:
	        case POPPLER_ANNOT_MOVIE:
			/* Ignore link, widgets and movie annots since they are already handled */
			break;
	        case POPPLER_ANNOT_SCREEN: {
			PopplerAction *action;

			/* Ignore screen annots containing a rendition action */
			action = poppler_annot_screen_get_action (POPPLER_ANNOT_SCREEN (poppler_annot));
			if (action && action->type == POPPLER_ACTION_RENDITION)
				break;
		}
			/* Fall through */
		case POPPLER_ANNOT_3D:
		case POPPLER_ANNOT_CARET:
		case POPPLER_ANNOT_FREE_TEXT:
		case POPPLER_ANNOT_LINE:
		case POPPLER_ANNOT_SOUND:
		case POPPLER_ANNOT_SQUARE:
	        case POPPLER_ANNOT_STAMP: {
			/* FIXME: These annotations are unimplemented, but they were already
			 * reported in Evince Bugzilla with test case.  We add a special
			 * warning to let the user know it is unimplemented, yet we do not
			 * want more duplicates of known issues.
			 */
			GEnumValue *enum_value;
			reported_annot = TRUE;

			enum_value = g_enum_get_value ((GEnumClass *) g_type_class_ref (POPPLER_TYPE_ANNOT_TYPE),
						       poppler_annot_get_annot_type (poppler_annot));
			unimplemented_annot = enum_value ? enum_value->value_name : "Unknown annotation";
		}
			break;
	        default: {
			GEnumValue *enum_value;
			reported_annot = FALSE;

			enum_value = g_enum_get_value ((GEnumClass *) g_type_class_ref (POPPLER_TYPE_ANNOT_TYPE),
						       poppler_annot_get_annot_type (poppler_annot));
			unimplemented_annot = enum_value ? enum_value->value_name : "Unknown annotation";
		}
	}

	if (unimplemented_annot) {
		if (reported_annot) {
			g_warning ("Unimplemented annotation: %s.  It is a known issue "
			           "and it might be implemented in the future.",
				   unimplemented_annot);
		} else {
			g_warning ("Unimplemented annotation: %s, please post a "
			           "bug report in Evince issue tracker "
			           "(https://gitlab.gnome.org/GNOME/evince/issues) with a testcase.",
				   unimplemented_annot);
		}
	}

	if (ev_annot) {
		time_t   utime;
		gchar   *modified;
		gchar   *contents;
		gchar   *name;
		GdkRGBA  color;

		contents = poppler_annot_get_contents (poppler_annot);
		if (contents) {
			ev_annotation_set_contents (ev_annot, contents);
			g_free (contents);
		}

		name = poppler_annot_get_name (poppler_annot);
		if (name) {
			ev_annotation_set_name (ev_annot, name);
			g_free (name);
		}

		modified = poppler_annot_get_modified (poppler_annot);
		if (poppler_date_parse (modified, &utime)) {
			ev_annotation_set_modified_from_time_t (ev_annot, utime);
		} else {
			ev_annotation_set_modified (ev_annot, modified);
		}
		g_free (modified);

		poppler_annot_color_to_gdk_rgba (poppler_annot, &color);
		ev_annotation_set_rgba (ev_annot, &color);

		if (poppler_annot_can_have_popup_window (poppler_annot)) {
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
					      "popup_is_open", is_open,
					      "has_popup", TRUE,
					      NULL);
			} else {
				g_object_set (ev_annot,
					      "has_popup", FALSE,
					      NULL);
			}

			label = poppler_annot_markup_get_label (markup);
			if (label)
				g_object_set (ev_annot, "label", label, NULL);
			opacity = poppler_annot_markup_get_opacity (markup);

			g_object_set (ev_annot,
				      "opacity", opacity,
				      "can_have_popup", TRUE,
				      NULL);

			g_free (label);
		}
	}

	return ev_annot;
}

static void
annot_set_unique_name (EvAnnotation *annot)
{
	gchar *name;

	name = g_strdup_printf ("annot-%" G_GUINT64_FORMAT, g_get_real_time ());
	ev_annotation_set_name (annot, name);
	g_free (name);
}

static void
annot_area_changed_cb (EvAnnotation *annot,
		       GParamSpec   *spec,
		       EvMapping    *mapping)
{
	ev_annotation_get_area (annot, &mapping->area);
}

static EvMappingList *
pdf_document_annotations_get_annotations (EvDocumentAnnotations *document_annotations,
					  EvPage                *page)
{
	GList *retval = NULL;
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	EvMappingList *mapping_list;
	GList *annots;
	GList *list;
	gdouble height;

	pdf_document = PDF_DOCUMENT (document_annotations);
	poppler_page = POPPLER_PAGE (page->backend_page);

	if (pdf_document->annots) {
		mapping_list = (EvMappingList *)g_hash_table_lookup (pdf_document->annots,
								     GINT_TO_POINTER (page->index));
		if (mapping_list)
			return ev_mapping_list_ref (mapping_list);
	}

	annots = poppler_page_get_annot_mapping (poppler_page);
	poppler_page_get_size (poppler_page, NULL, &height);

	for (list = annots; list; list = list->next) {
		PopplerAnnotMapping *mapping;
		EvMapping           *annot_mapping;
		EvAnnotation        *ev_annot;

		mapping = (PopplerAnnotMapping *)list->data;

		ev_annot = ev_annot_from_poppler_annot (mapping->annot, page);
		if (!ev_annot)
			continue;

		/* Make sure annot has a unique name */
		if (!ev_annotation_get_name (ev_annot))
			annot_set_unique_name (ev_annot);

		annot_mapping = g_new (EvMapping, 1);
		if (EV_IS_ANNOTATION_TEXT (ev_annot)) {
			/* Force 24x24 rectangle */
			annot_mapping->area.x1 = mapping->area.x1;
			annot_mapping->area.x2 = annot_mapping->area.x1 + 24;
			annot_mapping->area.y1 = height - mapping->area.y2;
			annot_mapping->area.y2 = MIN(height, annot_mapping->area.y1 + 24);
		} else {
			annot_mapping->area.x1 = mapping->area.x1;
			annot_mapping->area.x2 = mapping->area.x2;
			annot_mapping->area.y1 = height - mapping->area.y2;
			annot_mapping->area.y2 = height - mapping->area.y1;
		}
		annot_mapping->data = ev_annot;
		ev_annotation_set_area (ev_annot, &annot_mapping->area);
		g_signal_connect (ev_annot, "notify::area",
				  G_CALLBACK (annot_area_changed_cb),
				  annot_mapping);

		g_object_set_data_full (G_OBJECT (ev_annot),
					"poppler-annot",
					g_object_ref (mapping->annot),
					(GDestroyNotify) g_object_unref);

		retval = g_list_prepend (retval, annot_mapping);
	}

	poppler_page_free_annot_mapping (annots);

	if (!retval)
		return NULL;

	if (!pdf_document->annots) {
		pdf_document->annots = g_hash_table_new_full (g_direct_hash,
							      g_direct_equal,
							      (GDestroyNotify)NULL,
							      (GDestroyNotify)ev_mapping_list_unref);
	}

	mapping_list = ev_mapping_list_new (page->index, g_list_reverse (retval), (GDestroyNotify)g_object_unref);
	g_hash_table_insert (pdf_document->annots,
			     GINT_TO_POINTER (page->index),
			     ev_mapping_list_ref (mapping_list));

	return mapping_list;
}

static gboolean
pdf_document_annotations_document_is_modified (EvDocumentAnnotations *document_annotations)
{
	return PDF_DOCUMENT (document_annotations)->annots_modified;
}

static void
pdf_document_annotations_remove_annotation (EvDocumentAnnotations *document_annotations,
                                            EvAnnotation          *annot)
{
        PopplerPage   *poppler_page;
        PdfDocument   *pdf_document;
        EvPage        *page;
        PopplerAnnot  *poppler_annot;
        EvMappingList *mapping_list;
        EvMapping     *annot_mapping;

        poppler_annot = POPPLER_ANNOT (g_object_get_data (G_OBJECT (annot), "poppler-annot"));
        pdf_document = PDF_DOCUMENT (document_annotations);
        page = ev_annotation_get_page (annot);
        poppler_page = POPPLER_PAGE (page->backend_page);

	poppler_page_remove_annot (poppler_page, poppler_annot);

        /* We don't check for pdf_document->annots, if it were NULL then something is really wrong */
        mapping_list = (EvMappingList *)g_hash_table_lookup (pdf_document->annots,
                                                             GINT_TO_POINTER (page->index));
        if (mapping_list) {
                annot_mapping = ev_mapping_list_find (mapping_list, annot);
                ev_mapping_list_remove (mapping_list, annot_mapping);
		if (ev_mapping_list_length (mapping_list) == 0)
			g_hash_table_remove (pdf_document->annots, GINT_TO_POINTER (page->index));
        }

        pdf_document->annots_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document_annotations), TRUE);
}

/* FIXME: this could be moved to poppler */
static GArray *
get_quads_for_area (PopplerPage      *page,
		    EvRectangle      *area,
		    PopplerRectangle *bbox)
{
	cairo_region_t *region;
	guint   n_rects;
	guint   i;
	GArray *quads;
	gdouble height;
	gdouble max_x, max_y, min_x, min_y;

	if (bbox) {
		bbox->x1 = G_MAXDOUBLE;
		bbox->y1 = G_MAXDOUBLE;
		bbox->x2 = G_MINDOUBLE;
		bbox->y2 = G_MINDOUBLE;
	}

	poppler_page_get_size (page, NULL, &height);

	region = poppler_page_get_selected_region (page, 1.0, POPPLER_SELECTION_GLYPH,
						   (PopplerRectangle *)area);
	n_rects = cairo_region_num_rectangles (region);
	g_debug ("Number rects: %d", n_rects);

	quads = g_array_sized_new (TRUE, TRUE,
				   sizeof (PopplerQuadrilateral),
				   n_rects);
	g_array_set_size (quads, MAX (1, n_rects));

	for (i = 0; i < n_rects; i++) {
		cairo_rectangle_int_t r;
		PopplerQuadrilateral *quad = &g_array_index (quads, PopplerQuadrilateral, i);
		cairo_region_get_rectangle (region, i, &r);

		quad->p1.x = r.x;
		quad->p1.y = height - r.y;
		quad->p2.x = r.x + r.width;
		quad->p2.y = height - r.y;
		quad->p3.x = r.x;
		quad->p3.y = height - (r.y + r.height);
		quad->p4.x = r.x + r.width;
		quad->p4.y = height - (r.y + r.height);

		if (!bbox)
			continue;

		max_x = MAX (quad->p1.x, MAX (quad->p2.x, MAX (quad->p3.x, quad->p4.x)));
		max_y = MAX (quad->p1.y, MAX (quad->p2.y, MAX (quad->p3.y, quad->p4.y)));
		min_x = MIN (quad->p1.x, MIN (quad->p2.x, MIN (quad->p3.x, quad->p4.x)));
		min_y = MIN (quad->p1.y, MIN (quad->p2.y, MIN (quad->p3.y, quad->p4.y)));

		if (min_x < bbox->x1)
			bbox->x1 = min_x;
		if (min_y < bbox->y1)
			bbox->y1 = min_y;
		if (max_x > bbox->x2)
			bbox->x2 = max_x;
		if (max_y > bbox->y2)
			bbox->y2 = max_y;
	}
	cairo_region_destroy (region);

	if (n_rects == 0 && bbox) {
		bbox->x1 = 0;
		bbox->y1 = 0;
		bbox->x2 = 0;
		bbox->y2 = 0;
	}

	return quads;
}

static void
pdf_document_annotations_add_annotation (EvDocumentAnnotations *document_annotations,
					 EvAnnotation          *annot,
					 EvRectangle           *rect_deprecated)
{
	PopplerAnnot    *poppler_annot;
	PdfDocument     *pdf_document;
	EvPage          *page;
	PopplerPage     *poppler_page;
	GList           *list = NULL;
	EvMappingList   *mapping_list;
	EvMapping       *annot_mapping;
	PopplerRectangle poppler_rect;
	gdouble          height;
	PopplerColor     poppler_color;
	GdkRGBA          color;
	EvRectangle      rect;

	pdf_document = PDF_DOCUMENT (document_annotations);
	page = ev_annotation_get_page (annot);
	poppler_page = POPPLER_PAGE (page->backend_page);

	ev_annotation_get_area (annot, &rect);

	poppler_page_get_size (poppler_page, NULL, &height);
	poppler_rect.x1 = rect.x1;
	poppler_rect.x2 = rect.x2;
	poppler_rect.y1 = height - rect.y2;
	poppler_rect.y2 = height - rect.y1;

	switch (ev_annotation_get_annotation_type (annot)) {
		case EV_ANNOTATION_TYPE_TEXT: {
			EvAnnotationText    *text = EV_ANNOTATION_TEXT (annot);
			EvAnnotationTextIcon icon;

			poppler_annot = poppler_annot_text_new (pdf_document->document, &poppler_rect);

			icon = ev_annotation_text_get_icon (text);
			poppler_annot_text_set_icon (POPPLER_ANNOT_TEXT (poppler_annot),
						     get_poppler_annot_text_icon (icon));
			}
			break;
		case EV_ANNOTATION_TYPE_TEXT_MARKUP: {
			GArray *quads;
			PopplerRectangle bbox;

			quads = get_quads_for_area (poppler_page, &rect, &bbox);

			if (bbox.x1 != 0 && bbox.y1 != 0 && bbox.x2 != 0 && bbox.y2 != 0) {
				poppler_rect.x1 = rect.x1 = bbox.x1;
				poppler_rect.x2 = rect.x2 = bbox.x2;
				rect.y1 = height - bbox.y2;
				rect.y2 = height - bbox.y1;
				poppler_rect.y1 = bbox.y1;
				poppler_rect.y2 = bbox.y2;

				ev_annotation_set_area (annot, &rect);
			}

			switch (ev_annotation_text_markup_get_markup_type (EV_ANNOTATION_TEXT_MARKUP (annot))) {
				case EV_ANNOTATION_TEXT_MARKUP_HIGHLIGHT:
					poppler_annot = poppler_annot_text_markup_new_highlight (pdf_document->document, &poppler_rect, quads);
					break;
				default:
					g_assert_not_reached ();
			}
			g_array_unref (quads);
		}
			break;
		default:
			g_assert_not_reached ();
	}

	ev_annotation_get_rgba (annot, &color);
	poppler_color.red = CLAMP ((guint) (color.red * 65535), 0, 65535);
	poppler_color.green = CLAMP ((guint) (color.green * 65535), 0, 65535);
	poppler_color.blue = CLAMP ((guint) (color.blue * 65535), 0, 65535);
	poppler_annot_set_color (poppler_annot, &poppler_color);

	if (EV_IS_ANNOTATION_MARKUP (annot)) {
		EvAnnotationMarkup *markup = EV_ANNOTATION_MARKUP (annot);
		const gchar *label;

		if (ev_annotation_markup_has_popup (markup)) {
			EvRectangle popup_rect;

			ev_annotation_markup_get_rectangle (markup, &popup_rect);
			poppler_rect.x1 = popup_rect.x1;
			poppler_rect.x2 = popup_rect.x2;
			poppler_rect.y1 = height - popup_rect.y2;
			poppler_rect.y2 = height - popup_rect.y1;
			poppler_annot_markup_set_popup (POPPLER_ANNOT_MARKUP (poppler_annot), &poppler_rect);
			poppler_annot_markup_set_popup_is_open (POPPLER_ANNOT_MARKUP (poppler_annot),
								ev_annotation_markup_get_popup_is_open (markup));
		}

		label = ev_annotation_markup_get_label (markup);
		if (label)
			poppler_annot_markup_set_label (POPPLER_ANNOT_MARKUP (poppler_annot), label);
	}

	poppler_page_add_annot (poppler_page, poppler_annot);

	annot_mapping = g_new (EvMapping, 1);
	annot_mapping->area = rect;
	annot_mapping->data = annot;
	g_signal_connect (annot, "notify::area",
			  G_CALLBACK (annot_area_changed_cb),
			  annot_mapping);
	g_object_set_data_full (G_OBJECT (annot),
				"poppler-annot",
				poppler_annot,
				(GDestroyNotify) g_object_unref);

	if (pdf_document->annots) {
		mapping_list = (EvMappingList *)g_hash_table_lookup (pdf_document->annots,
								     GINT_TO_POINTER (page->index));
	} else {
		pdf_document->annots = g_hash_table_new_full (g_direct_hash,
							      g_direct_equal,
							      (GDestroyNotify)NULL,
							      (GDestroyNotify)ev_mapping_list_unref);
		mapping_list = NULL;
	}

	annot_set_unique_name (annot);

	if (mapping_list) {
		list = ev_mapping_list_get_list (mapping_list);
		list = g_list_append (list, annot_mapping);
	} else {
		list = g_list_append (list, annot_mapping);
		mapping_list = ev_mapping_list_new (page->index, list, (GDestroyNotify)g_object_unref);
		g_hash_table_insert (pdf_document->annots,
				     GINT_TO_POINTER (page->index),
				     ev_mapping_list_ref (mapping_list));
	}

	pdf_document->annots_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document_annotations), TRUE);
}

/* FIXME: We could probably add this to poppler */
static void
copy_poppler_annot (PopplerAnnot* src_annot,
		    PopplerAnnot* dst_annot)
{
	char         *contents;
	PopplerColor *color;

	contents = poppler_annot_get_contents (src_annot);
	poppler_annot_set_contents (dst_annot, contents);
	g_free (contents);

	poppler_annot_set_flags (dst_annot, poppler_annot_get_flags (src_annot));

	color = poppler_annot_get_color (src_annot);
	poppler_annot_set_color (dst_annot, color);
	g_free (color);

	if (POPPLER_IS_ANNOT_MARKUP (src_annot) && POPPLER_IS_ANNOT_MARKUP (dst_annot)) {
		PopplerAnnotMarkup *src_markup = POPPLER_ANNOT_MARKUP (src_annot);
		PopplerAnnotMarkup *dst_markup = POPPLER_ANNOT_MARKUP (dst_annot);
		char               *label;

		label = poppler_annot_markup_get_label (src_markup);
		poppler_annot_markup_set_label (dst_markup, label);
		g_free (label);

		poppler_annot_markup_set_opacity (dst_markup, poppler_annot_markup_get_opacity (src_markup));

		if (poppler_annot_markup_has_popup (src_markup)) {
			PopplerRectangle popup_rect;

			if (poppler_annot_markup_get_popup_rectangle (src_markup, &popup_rect)) {
				poppler_annot_markup_set_popup (dst_markup, &popup_rect);
				poppler_annot_markup_set_popup_is_open (dst_markup, poppler_annot_markup_get_popup_is_open (src_markup));
			}

		}
	}
}

static void
pdf_document_annotations_save_annotation (EvDocumentAnnotations *document_annotations,
					  EvAnnotation          *annot,
					  EvAnnotationsSaveMask  mask)
{
	PopplerAnnot *poppler_annot;

	poppler_annot = POPPLER_ANNOT (g_object_get_data (G_OBJECT (annot), "poppler-annot"));
	if (!poppler_annot)
		return;

	if (mask & EV_ANNOTATIONS_SAVE_CONTENTS)
		poppler_annot_set_contents (poppler_annot,
					    ev_annotation_get_contents (annot));

	if (mask & EV_ANNOTATIONS_SAVE_COLOR) {
		PopplerColor color;
		GdkRGBA      ev_color;

		ev_annotation_get_rgba (annot, &ev_color);
		color.red = CLAMP ((guint) (ev_color.red * 65535), 0, 65535);
		color.green = CLAMP ((guint) (ev_color.green * 65535), 0, 65535);
		color.blue = CLAMP ((guint) (ev_color.blue * 65535), 0, 65535);
		poppler_annot_set_color (poppler_annot, &color);
	}

	if (mask & EV_ANNOTATIONS_SAVE_AREA && !EV_IS_ANNOTATION_TEXT_MARKUP (annot)) {
		EvRectangle      area;
		PopplerRectangle poppler_rect;
		EvPage          *page;
		gdouble          height;

		page = ev_annotation_get_page (annot);
		poppler_page_get_size (POPPLER_PAGE (page->backend_page), NULL, &height);

		ev_annotation_get_area (annot, &area);
		poppler_rect.x1 = area.x1;
		poppler_rect.x2 = area.x2;
		poppler_rect.y1 = height - area.y2;
		poppler_rect.y2 = height - area.y1;
		poppler_annot_set_rectangle (poppler_annot, &poppler_rect);
	}

	if (EV_IS_ANNOTATION_MARKUP (annot)) {
		EvAnnotationMarkup *ev_markup = EV_ANNOTATION_MARKUP (annot);
		PopplerAnnotMarkup *markup = POPPLER_ANNOT_MARKUP (poppler_annot);

		if (mask & EV_ANNOTATIONS_SAVE_LABEL)
			poppler_annot_markup_set_label (markup, ev_annotation_markup_get_label (ev_markup));
		if (mask & EV_ANNOTATIONS_SAVE_OPACITY)
			poppler_annot_markup_set_opacity (markup, ev_annotation_markup_get_opacity (ev_markup));
		if (mask & EV_ANNOTATIONS_SAVE_POPUP_RECT) {
			EvPage *page;
			EvRectangle ev_rect;
			PopplerRectangle poppler_rect;
			gdouble height;

			page = ev_annotation_get_page (annot);
			poppler_page_get_size (POPPLER_PAGE (page->backend_page),
						NULL, &height);
			ev_annotation_markup_get_rectangle (ev_markup, &ev_rect);

			poppler_rect.x1 = ev_rect.x1;
			poppler_rect.x2 = ev_rect.x2;
			poppler_rect.y1 = height - ev_rect.y2;
			poppler_rect.y2 = height - ev_rect.y1;

			if (poppler_annot_markup_has_popup (markup))
				poppler_annot_markup_set_popup_rectangle (markup, &poppler_rect);
			else
				poppler_annot_markup_set_popup (markup, &poppler_rect);
		}
		if (mask & EV_ANNOTATIONS_SAVE_POPUP_IS_OPEN)
			poppler_annot_markup_set_popup_is_open (markup, ev_annotation_markup_get_popup_is_open (ev_markup));
	}

	if (EV_IS_ANNOTATION_TEXT (annot)) {
		EvAnnotationText *ev_text = EV_ANNOTATION_TEXT (annot);
		PopplerAnnotText *text = POPPLER_ANNOT_TEXT (poppler_annot);

		if (mask & EV_ANNOTATIONS_SAVE_TEXT_IS_OPEN) {
			poppler_annot_text_set_is_open (text,
							ev_annotation_text_get_is_open (ev_text));
		}
		if (mask & EV_ANNOTATIONS_SAVE_TEXT_ICON) {
			EvAnnotationTextIcon icon;

			icon = ev_annotation_text_get_icon (ev_text);
			poppler_annot_text_set_icon (text, get_poppler_annot_text_icon (icon));
		}
	}

	if (EV_IS_ANNOTATION_TEXT_MARKUP (annot)) {
		EvAnnotationTextMarkup *ev_text_markup = EV_ANNOTATION_TEXT_MARKUP (annot);
		PopplerAnnotTextMarkup *text_markup = POPPLER_ANNOT_TEXT_MARKUP (poppler_annot);

		if (mask & EV_ANNOTATIONS_SAVE_TEXT_MARKUP_TYPE) {
			/* In poppler every text markup annotation type is a different class */
			GArray           *quads;
			PopplerRectangle  rect;
			PopplerAnnot     *new_annot = NULL;
			PdfDocument      *pdf_document;
			EvPage           *page;
			PopplerPage      *poppler_page;

			pdf_document = PDF_DOCUMENT (document_annotations);

			quads = poppler_annot_text_markup_get_quadrilaterals (text_markup);
			poppler_annot_get_rectangle (POPPLER_ANNOT (text_markup), &rect);

			switch (ev_annotation_text_markup_get_markup_type (ev_text_markup)) {
			case EV_ANNOTATION_TEXT_MARKUP_HIGHLIGHT:
				new_annot = poppler_annot_text_markup_new_highlight (pdf_document->document, &rect, quads);
				break;
			case EV_ANNOTATION_TEXT_MARKUP_STRIKE_OUT:
				new_annot = poppler_annot_text_markup_new_strikeout (pdf_document->document, &rect, quads);
				break;
			case EV_ANNOTATION_TEXT_MARKUP_UNDERLINE:
				new_annot = poppler_annot_text_markup_new_underline (pdf_document->document, &rect, quads);
				break;
			case EV_ANNOTATION_TEXT_MARKUP_SQUIGGLY:
				new_annot = poppler_annot_text_markup_new_squiggly (pdf_document->document, &rect, quads);
				break;
			}

			g_array_unref (quads);

			copy_poppler_annot (poppler_annot, new_annot);

			page = ev_annotation_get_page (annot);
			poppler_page = POPPLER_PAGE (page->backend_page);

			poppler_page_remove_annot (poppler_page, poppler_annot);
			poppler_page_add_annot (poppler_page, new_annot);
			g_object_set_data_full (G_OBJECT (annot),
						"poppler-annot",
						new_annot,
						(GDestroyNotify) g_object_unref);
		}

		if (mask & EV_ANNOTATIONS_SAVE_AREA) {
			EvRectangle       area;
			GArray           *quads;
			PopplerRectangle  bbox;
			EvPage           *page;
			PopplerPage      *poppler_page;

			page = ev_annotation_get_page (annot);
			poppler_page = POPPLER_PAGE (page->backend_page);

			ev_annotation_get_area (annot, &area);
			quads = get_quads_for_area (poppler_page, &area, &bbox);
			poppler_annot_text_markup_set_quadrilaterals (text_markup, quads);
			poppler_annot_set_rectangle (poppler_annot, &bbox);
			g_array_unref (quads);

			if (bbox.x1 != 0 && bbox.y1 != 0 && bbox.x2 != 0 && bbox.y2 != 0) {
				gdouble height;

				poppler_page_get_size (poppler_page, NULL, &height);
				area.x1 = bbox.x1;
				area.x2 = bbox.x2;
				area.y1 = height - bbox.y2;
				area.y2 = height - bbox.y1;
			} else {
				area.x1 = 0;
				area.x2 = 0;
				area.y1 = 0;
				area.y2 = 0;
			}
			ev_annotation_set_area (annot, &area);
		}
	}

	PDF_DOCUMENT (document_annotations)->annots_modified = TRUE;
	ev_document_set_modified (EV_DOCUMENT (document_annotations), TRUE);
}

/* Creates a vector from points @p1 and @p2 and stores it on @vector */
static inline void
set_vector (PopplerPoint *p1,
	    PopplerPoint *p2,
	    PopplerPoint *vector)
{
	vector->x = p2->x - p1->x;
	vector->y = p2->y - p1->y;
}

/* Returns the dot product of the passed vectors @v1 and @v2 */
static inline gdouble
dot_product (PopplerPoint *v1,
	     PopplerPoint *v2)
{
	return v1->x * v2->x + v1->y * v2->y;
}

/* Algorithm: https://math.stackexchange.com/a/190203
   Implementation: https://stackoverflow.com/a/37865332 */
static gboolean
point_over_poppler_quadrilateral (PopplerQuadrilateral *quad,
				  PopplerPoint         *M)
{
	PopplerPoint AB, AM, BC, BM;
	gdouble dotABAM, dotABAB, dotBCBM, dotBCBC;

	/* We interchange p3 and p4 because algorithm expects clockwise eg. p1 -> p2
	   while pdf quadpoints are defined as p1 -> p2                     p4 <- p3
	                                       p3 -> p4 (https://stackoverflow.com/q/9855814) */
	set_vector (&quad->p1, &quad->p2, &AB);
	set_vector (&quad->p1, M, &AM);
	set_vector (&quad->p2, &quad->p4, &BC);
	set_vector (&quad->p2, M, &BM);

	dotABAM = dot_product (&AB, &AM);
	dotABAB = dot_product (&AB, &AB);
	dotBCBM = dot_product (&BC, &BM);
	dotBCBC = dot_product (&BC, &BC);

	return 0 <= dotABAM && dotABAM <= dotABAB && 0 <= dotBCBM && dotBCBM <= dotBCBC;
}

static EvAnnotationsOverMarkup
pdf_document_annotations_over_markup (EvDocumentAnnotations *document_annotations,
				      EvAnnotation          *annot,
				      gdouble                x,
				      gdouble                y)
{
	EvPage       *page;
	PopplerAnnot *poppler_annot;
	GArray       *quads;
	PopplerPoint  M;
	guint         quads_len;
	gdouble       height;

	poppler_annot = POPPLER_ANNOT (g_object_get_data (G_OBJECT (annot), "poppler-annot"));

	if (!poppler_annot || !POPPLER_IS_ANNOT_TEXT_MARKUP (poppler_annot))
		return EV_ANNOTATION_OVER_MARKUP_UNKNOWN;

	quads = poppler_annot_text_markup_get_quadrilaterals (POPPLER_ANNOT_TEXT_MARKUP (poppler_annot));
	quads_len = quads->len;

	page = ev_annotation_get_page (annot);
	poppler_page_get_size (POPPLER_PAGE (page->backend_page), NULL, &height);
	M.x = x;
	M.y = height - y;

	for (guint i = 0; i < quads_len; ++i) {
		PopplerQuadrilateral *quadrilateral = &g_array_index (quads, PopplerQuadrilateral, i);

		if (point_over_poppler_quadrilateral (quadrilateral, &M)) {
			g_array_unref (quads);
			return EV_ANNOTATION_OVER_MARKUP_YES;
		}
	}
	g_array_unref (quads);

	return EV_ANNOTATION_OVER_MARKUP_NOT;
}

static void
pdf_document_document_annotations_iface_init (EvDocumentAnnotationsInterface *iface)
{
	iface->get_annotations = pdf_document_annotations_get_annotations;
	iface->document_is_modified = pdf_document_annotations_document_is_modified;
	iface->add_annotation = pdf_document_annotations_add_annotation;
	iface->save_annotation = pdf_document_annotations_save_annotation;
	iface->remove_annotation = pdf_document_annotations_remove_annotation;
	iface->over_markup = pdf_document_annotations_over_markup;
}

/* Media */
static GFile *
get_media_file (const gchar *filename,
		EvDocument  *document)
{
	GFile *file;

	if (g_path_is_absolute (filename)) {
		file = g_file_new_for_path (filename);
	} else if (g_strrstr (filename, "://")) {
		file = g_file_new_for_uri (filename);
	} else {
		gchar *doc_path;
		gchar *path;
		gchar *base_dir;

		doc_path = g_filename_from_uri (ev_document_get_uri (document), NULL, NULL);
		base_dir = g_path_get_dirname (doc_path);
		g_free (doc_path);

		path = g_build_filename (base_dir, filename, NULL);
		g_free (base_dir);

		file = g_file_new_for_path (path);
		g_free (path);
	}

	return file;
}

static EvMedia *
ev_media_from_poppler_movie (EvDocument   *document,
			     EvPage       *page,
			     PopplerMovie *movie)
{
	EvMedia     *media;
	GFile       *file;
	gchar       *uri;

	file = get_media_file (poppler_movie_get_filename (movie), document);
	uri = g_file_get_uri (file);
	g_object_unref (file);

	media = ev_media_new_for_uri (page, uri);
	g_free (uri);
	ev_media_set_show_controls (media, poppler_movie_show_controls (movie));

	return media;
}

static void
delete_temp_file (GFile *file)
{
	g_file_delete (file, NULL, NULL);
	g_object_unref (file);
}

static gboolean
media_save_to_file_callback (const gchar *buffer,
			     gsize        count,
			     gpointer     data,
			     GError     **error)
{
	gint fd = GPOINTER_TO_INT (data);

	return write (fd, buffer, count) == (gssize)count;
}

static EvMedia *
ev_media_from_poppler_rendition (EvDocument   *document,
				 EvPage       *page,
				 PopplerMedia *poppler_media)
{
	EvMedia *media;
	GFile   *file = NULL;
	gchar   *uri;
	gboolean is_temp_file = FALSE;

	if (!poppler_media)
		return NULL;

	if (poppler_media_is_embedded (poppler_media)) {
		gint   fd;
		gchar *filename;

		fd = ev_mkstemp ("evmedia.XXXXXX", &filename, NULL);
		if (fd == -1)
			return NULL;

		if (poppler_media_save_to_callback (poppler_media,
						    media_save_to_file_callback,
						    GINT_TO_POINTER (fd), NULL)) {
			file = g_file_new_for_path (filename);
			is_temp_file = TRUE;
		}
		close (fd);
		g_free (filename);
	} else {
		file = get_media_file (poppler_media_get_filename (poppler_media), document);
	}

	if (!file)
		return NULL;

	uri = g_file_get_uri (file);
	media = ev_media_new_for_uri (page, uri);
	ev_media_set_show_controls (media, TRUE);
	g_free (uri);

	if (is_temp_file)
		g_object_set_data_full (G_OBJECT (media), "poppler-media-temp-file", file, (GDestroyNotify)delete_temp_file);
	else
		g_object_unref (file);

	return media;
}

static EvMappingList *
pdf_document_media_get_media_mapping (EvDocumentMedia *document_media,
				      EvPage          *page)
{
	GList *retval = NULL;
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GList *annots;
	GList *list;
	gdouble height;

	pdf_document = PDF_DOCUMENT (document_media);
	poppler_page = POPPLER_PAGE (page->backend_page);

	annots = poppler_page_get_annot_mapping (poppler_page);
	poppler_page_get_size (poppler_page, NULL, &height);

	for (list = annots; list; list = list->next) {
		PopplerAnnotMapping *mapping;
		EvMapping           *media_mapping;
		EvMedia             *media = NULL;

		mapping = (PopplerAnnotMapping *)list->data;

		switch (poppler_annot_get_annot_type (mapping->annot)) {
		case POPPLER_ANNOT_MOVIE: {
			PopplerAnnotMovie *poppler_annot;

			poppler_annot = POPPLER_ANNOT_MOVIE (mapping->annot);
			media = ev_media_from_poppler_movie (EV_DOCUMENT (pdf_document), page,
							     poppler_annot_movie_get_movie (poppler_annot));
		}
			break;
		case POPPLER_ANNOT_SCREEN: {
			PopplerAction *action;

			action = poppler_annot_screen_get_action (POPPLER_ANNOT_SCREEN (mapping->annot));
			if (action && action->type == POPPLER_ACTION_RENDITION) {
				media = ev_media_from_poppler_rendition (EV_DOCUMENT (pdf_document), page,
									 action->rendition.media);
			}
		}
			break;
		default:
			break;
		}

		if (!media)
			continue;

		media_mapping = g_new (EvMapping, 1);

		media_mapping->data = media;
		media_mapping->area.x1 = mapping->area.x1;
		media_mapping->area.x2 = mapping->area.x2;
		media_mapping->area.y1 = height - mapping->area.y2;
		media_mapping->area.y2 = height - mapping->area.y1;

		retval = g_list_prepend (retval, media_mapping);
	}

	poppler_page_free_annot_mapping (annots);

	if (!retval)
		return NULL;

	return ev_mapping_list_new (page->index, g_list_reverse (retval), (GDestroyNotify)g_object_unref);
}

static void
pdf_document_document_media_iface_init (EvDocumentMediaInterface *iface)
{
	iface->get_media_mapping = pdf_document_media_get_media_mapping;
}

/* Attachments */
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
	struct SaveToBufferData *sdata = (struct SaveToBufferData *)user_data;
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
pdf_document_attachments_get_attachments (EvDocumentAttachments *document)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	GList *attachments;
	GList *list;
	GList *retval = NULL;

	attachments = poppler_document_get_attachments (pdf_document->document);

	for (list = attachments; list; list = list->next) {
		PopplerAttachment *attachment;
		EvAttachment *ev_attachment;
		gchar *data = NULL;
		gsize size;
		GError *error = NULL;

		attachment = (PopplerAttachment *) list->data;

		if (attachment_save_to_buffer (attachment, &data, &size, &error)) {
			GDateTime *mtime, *ctime;

			mtime = poppler_attachment_get_mtime (attachment);
			ctime = poppler_attachment_get_ctime (attachment);

			ev_attachment = ev_attachment_new (attachment->name,
							   attachment->description,
							   mtime, ctime,
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

static gboolean
pdf_document_attachments_has_attachments (EvDocumentAttachments *document)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);

	return poppler_document_has_attachments (pdf_document->document);
}

static void
pdf_document_document_attachments_iface_init (EvDocumentAttachmentsInterface *iface)
{
	iface->has_attachments = pdf_document_attachments_has_attachments;
	iface->get_attachments = pdf_document_attachments_get_attachments;
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
			ev_layer = ev_layer_new (poppler_layer_is_parent (layer),
						 rb_group);
			g_object_set_data_full (G_OBJECT (ev_layer),
						"poppler-layer",
						g_object_ref (layer),
						(GDestroyNotify) g_object_unref);
		} else {
			gchar *title;

			title = poppler_layers_iter_get_title (iter);

			if (title == NULL)
				continue;

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
	PopplerLayer *poppler_layer;

	poppler_layer = POPPLER_LAYER (g_object_get_data (G_OBJECT (layer), "poppler-layer"));
	poppler_layer_show (poppler_layer);
}

static void
pdf_document_layers_hide_layer (EvDocumentLayers *document,
				EvLayer          *layer)
{
	PopplerLayer *poppler_layer;

	poppler_layer = POPPLER_LAYER (g_object_get_data (G_OBJECT (layer), "poppler-layer"));
	poppler_layer_hide (poppler_layer);
}

static gboolean
pdf_document_layers_layer_is_visible (EvDocumentLayers *document,
				      EvLayer          *layer)
{
	PopplerLayer *poppler_layer;

	poppler_layer = POPPLER_LAYER (g_object_get_data (G_OBJECT (layer), "poppler-layer"));
	return poppler_layer_is_visible (poppler_layer);
}

static void
pdf_document_document_layers_iface_init (EvDocumentLayersInterface *iface)
{
	iface->has_layers = pdf_document_layers_has_layers;
	iface->get_layers = pdf_document_layers_get_layers;
	iface->show_layer = pdf_document_layers_show_layer;
	iface->hide_layer = pdf_document_layers_hide_layer;
	iface->layer_is_visible = pdf_document_layers_layer_is_visible;
}

GType
ev_backend_query_type (void)
{
	return PDF_TYPE_DOCUMENT;
}
