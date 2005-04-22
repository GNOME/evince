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

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <poppler.h>
#include <poppler-document.h>
#include <poppler-page.h>

#include "ev-poppler.h"
#include "ev-ps-exporter.h"
#include "ev-document-find.h"
#include "ev-document-misc.h"
#include "ev-document-links.h"
#include "ev-document-security.h"
#include "ev-document-thumbnails.h"

typedef struct {
	PdfDocument *document;
	char *text;
	GList **pages;
	guint idle;
	int start_page;
	int search_page;
} PdfDocumentSearch;

struct _PdfDocumentClass
{
	GObjectClass parent_class;
};

struct _PdfDocument
{
	GObject parent_instance;

	PopplerDocument *document;
	PopplerPSFile *ps_file;
	gchar *password;

	PdfDocumentSearch *search;
};

static void pdf_document_document_iface_init            (EvDocumentIface           *iface);
static void pdf_document_security_iface_init            (EvDocumentSecurityIface   *iface);
static void pdf_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);
static void pdf_document_document_links_iface_init      (EvDocumentLinksIface      *iface);
static void pdf_document_find_iface_init                (EvDocumentFindIface       *iface);
static void pdf_document_ps_exporter_iface_init         (EvPSExporterIface         *iface);
static void pdf_document_thumbnails_get_dimensions      (EvDocumentThumbnails      *document_thumbnails,
							 gint                       page,
							 gint                       size,
							 gint                      *width,
							 gint                      *height);
static EvLink * ev_link_from_action (PopplerAction *action);


G_DEFINE_TYPE_WITH_CODE (PdfDocument, pdf_document, G_TYPE_OBJECT,
                         {
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
							pdf_document_document_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_SECURITY,
							pdf_document_security_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
							pdf_document_document_thumbnails_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LINKS,
							pdf_document_document_links_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND,
							pdf_document_find_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_PS_EXPORTER,
							pdf_document_ps_exporter_iface_init);
			 });

static void
pdf_document_class_init (PdfDocumentClass *klass)
{
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
	PopplerPage *poppler_page;

	poppler_page = poppler_document_get_page (PDF_DOCUMENT (document)->document,
						  page);

	poppler_page_get_size (poppler_page, width, height);
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

	return label;
}

static GList *
pdf_document_get_links (EvDocument *document,
			int         page)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GList *retval = NULL;
	GList *mapping_list;
	GList *list;
	double height;

	pdf_document = PDF_DOCUMENT (document);
	poppler_page = poppler_document_get_page (pdf_document->document,
						  page);
	mapping_list = poppler_page_get_link_mapping (poppler_page);
	poppler_page_get_size (poppler_page, NULL, &height);

	for (list = mapping_list; list; list = list->next) {
		PopplerLinkMapping *link_mapping;
		EvLinkMapping *ev_link_mapping;

		link_mapping = (PopplerLinkMapping *)list->data;
		ev_link_mapping = g_new (EvLinkMapping, 1);
		ev_link_mapping->link = ev_link_from_action (link_mapping->action);
		ev_link_mapping->x1 = link_mapping->area.x1;
		ev_link_mapping->x2 = link_mapping->area.x2;
		/* Invert this for X-style coordinates */
		ev_link_mapping->y1 = height - link_mapping->area.y2;
		ev_link_mapping->y2 = height - link_mapping->area.y1;

		retval = g_list_prepend (retval, ev_link_mapping);
	}

	poppler_page_free_link_mapping (mapping_list);

	return g_list_reverse (retval);
}
			

static GdkPixbuf *
pdf_document_render_pixbuf (EvDocument   *document,
			    int           page,
			    double        scale)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GdkPixbuf *pixbuf;
	double width_points, height_points;
	gint width, height;

	pdf_document = PDF_DOCUMENT (document);
	poppler_page = poppler_document_get_page (pdf_document->document,
						  page);

	poppler_page_get_size (poppler_page, &width_points, &height_points);
	width = (int) ceil (width_points * scale);
	height = (int) ceil (height_points * scale);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 FALSE, 8,
				 width, height);

	poppler_page_render_to_pixbuf (poppler_page,
				       0, 0,
				       width, height,
				       scale,
				       pixbuf,
				       0, 0);

	return pixbuf;
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

static gboolean
pdf_document_can_get_text (EvDocument *document)
{
	return TRUE;
}

static EvDocumentInfo *
pdf_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	PopplerPageLayout layout;
	PopplerPageMode mode;
	PopplerViewerPreferences view_prefs;

	info = g_new0 (EvDocumentInfo, 1);

	info->fields_mask = EV_DOCUMENT_INFO_TITLE |
			    EV_DOCUMENT_INFO_FORMAT |
			    EV_DOCUMENT_INFO_AUTHOR |
			    EV_DOCUMENT_INFO_SUBJECT |
			    EV_DOCUMENT_INFO_KEYWORDS |
			    EV_DOCUMENT_INFO_LAYOUT |
			    EV_DOCUMENT_INFO_START_MODE |
			    /* Missing EV_DOCUMENT_INFO_CREATION_DATE | */
			    EV_DOCUMENT_INFO_UI_HINTS;


	g_object_get (PDF_DOCUMENT (document)->document,
		      "title", &(info->title),
		      "format", &(info->format),
		      "author", &(info->author),
		      "subject", &(info->subject),
		      "keywords", &(info->keywords),
		      "page-mode", &mode,
		      "page-layout", &layout,
		      "viewer-preferences", &view_prefs,
		      NULL);

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

	return info;
}

static char *
pdf_document_get_text (EvDocument *document, int page, EvRectangle *rect)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (document);
	PopplerPage *poppler_page;
	PopplerRectangle r;
	double height;
	
	poppler_page = poppler_document_get_page (pdf_document->document, page);
	g_return_val_if_fail (poppler_page != NULL, NULL);

	poppler_page_get_size (poppler_page, NULL, &height);
	r.x1 = rect->x1;
	r.y1 = height - rect->y2;
	r.x2 = rect->x2;
	r.y2 = height - rect->y1;

	return poppler_page_get_text (poppler_page, &r);
}

static void
pdf_document_document_iface_init (EvDocumentIface *iface)
{
	iface->save = pdf_document_save;
	iface->load = pdf_document_load;
	iface->get_n_pages = pdf_document_get_n_pages;
	iface->get_page_size = pdf_document_get_page_size;
	iface->get_page_label = pdf_document_get_page_label;
	iface->get_links = pdf_document_get_links;
	iface->render_pixbuf = pdf_document_render_pixbuf;
	iface->get_text = pdf_document_get_text;
	iface->can_get_text = pdf_document_can_get_text;
	iface->get_info = pdf_document_get_info;
};

static void
pdf_document_security_iface_init (EvDocumentSecurityIface *iface)
{
	iface->has_document_security = pdf_document_has_document_security;
	iface->set_password = pdf_document_set_password;
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

static EvLink *
ev_link_from_action (PopplerAction *action)
{
	EvLink *link;
	const char *title;

	title = action->any.title;
	
	if (action->type == POPPLER_ACTION_GOTO_DEST) {
		link = ev_link_new_page (title, action->goto_dest.dest->page_num - 1);
	} else if (action->type == POPPLER_ACTION_URI) {
		link = ev_link_new_external (title, action->uri.uri);
	} else {
		link = ev_link_new_title (title);
	}

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
		EvLink *link;
		
		action = poppler_index_iter_get_action (iter);
		if (action) {
			gtk_tree_store_append (GTK_TREE_STORE (model), &tree_iter, parent);
			link = ev_link_from_action (action);
			poppler_action_free (action);

			gtk_tree_store_set (GTK_TREE_STORE (model), &tree_iter,
					    EV_DOCUMENT_LINKS_COLUMN_MARKUP, ev_link_get_title (link),
					    EV_DOCUMENT_LINKS_COLUMN_LINK, link,
					    -1);
			child = poppler_index_iter_get_child (iter);
			if (child)
				build_tree (pdf_document, model, &tree_iter, child);
			poppler_index_iter_free (child);
		}
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
	/* Create the model iff we have items*/
	if (iter != NULL) {
		model = (GtkTreeModel *) gtk_tree_store_new (EV_DOCUMENT_LINKS_COLUMN_NUM_COLUMNS,
							     G_TYPE_STRING,
							     G_TYPE_POINTER);
		build_tree (pdf_document, model, NULL, iter);
		poppler_index_iter_free (iter);
	}
	

	return model;
}


static void
pdf_document_document_links_iface_init (EvDocumentLinksIface *iface)
{
	iface->has_document_links = pdf_document_links_has_document_links;
	iface->get_links_model = pdf_document_links_get_links_model;
}


static GdkPixbuf *
make_thumbnail_for_size (PdfDocument *pdf_document,
			 gint         page,
			 gint         size,
			 gboolean     border)
{
	PopplerPage *poppler_page;
	GdkPixbuf *pixbuf;
	int width, height;
	int x_offset, y_offset;
	double scale;
	gdouble unscaled_width, unscaled_height;

	poppler_page = poppler_document_get_page (pdf_document->document, page);

	g_return_val_if_fail (poppler_page != NULL, NULL);

	pdf_document_thumbnails_get_dimensions (EV_DOCUMENT_THUMBNAILS (pdf_document), page, size, &width, &height);
	poppler_page_get_size (poppler_page, &unscaled_width, &unscaled_height);
	scale = width / unscaled_width;

	if (border) {
		pixbuf = ev_document_misc_get_thumbnail_frame (width, height, NULL);
		x_offset = 1;
		y_offset = 1;
	} else {
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
					 width, height);
		gdk_pixbuf_fill (pixbuf, 0xffffffff);
		x_offset = 0;
		y_offset = 0;
	}

	poppler_page_render_to_pixbuf (poppler_page, 0, 0,
				       width, height,
				       scale, pixbuf,
				       x_offset, y_offset);

	return pixbuf;
}

static GdkPixbuf *
pdf_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document_thumbnails,
	 			       gint 		     page,
 				       gint                  size,
		 		       gboolean              border)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	GdkPixbuf *pixbuf;

	pdf_document = PDF_DOCUMENT (document_thumbnails);

	poppler_page = poppler_document_get_page (pdf_document->document, page);
	g_return_val_if_fail (poppler_page != NULL, NULL);

	pixbuf = poppler_page_get_thumbnail (poppler_page);
	if (pixbuf != NULL) {
		/* The document provides its own thumbnails. */
		if (border) {
			GdkPixbuf *real_pixbuf;

			real_pixbuf = ev_document_misc_get_thumbnail_frame (-1, -1, pixbuf);
			g_object_unref (pixbuf);
			pixbuf = real_pixbuf;
		}
	} else {
		/* There is no provided thumbnail.  We need to make one. */
		pixbuf = make_thumbnail_for_size (pdf_document, page, size, border);
	}
	return pixbuf;
}

static void
pdf_document_thumbnails_get_dimensions (EvDocumentThumbnails *document_thumbnails,
					gint                  page,
					gint                  size,
					gint                 *width,
					gint                 *height)
{
	PdfDocument *pdf_document;
	PopplerPage *poppler_page;
	gint has_thumb;
	
	pdf_document = PDF_DOCUMENT (document_thumbnails);
	poppler_page = poppler_document_get_page (pdf_document->document, page);

	g_return_if_fail (width != NULL);
	g_return_if_fail (height != NULL);
	g_return_if_fail (poppler_page != NULL);

	has_thumb = poppler_page_get_thumbnail_size (poppler_page, width, height);

	if (!has_thumb) {
		double page_width, page_height;

		poppler_page_get_size (poppler_page, &page_width, &page_height);
		if (page_width > page_height) {
			*width = size;
			*height = (int) (size * page_height / page_width);
		} else {
			*width = (int) (size * page_width / page_height);
			*height = size;
		}
	}
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
        int n_pages, changed_page;
	GList *matches;
	PopplerPage *page;

	page = poppler_document_get_page (search->document->document,
					  search->search_page);

	ev_document_doc_mutex_lock ();
	matches = poppler_page_find_text (page, search->text);
	ev_document_doc_mutex_unlock ();

	search->pages[search->search_page] = matches;
        n_pages = pdf_document_get_n_pages (EV_DOCUMENT (search->document));

	changed_page = search->search_page;
        search->search_page += 1;
        if (search->search_page == n_pages) {
                /* wrap around */
                search->search_page = 0;
        }

        if (search->search_page != search->start_page) {
	        ev_document_find_changed (EV_DOCUMENT_FIND (pdf_document),
					  changed_page);
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
	for (i = 0; i < n_pages; i++) {
		search->pages[i] = NULL;
	}

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
	
        g_free (search->text);
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

int
pdf_document_find_get_n_results (EvDocumentFind *document_find, int page)
{
	PdfDocumentSearch *search = PDF_DOCUMENT (document_find)->search;

	if (search) {
		return g_list_length (search->pages[page]);
	} else {
		return 0;
	}
}

gboolean
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
	
	return TRUE;
}

int
pdf_document_find_page_has_results (EvDocumentFind *document_find,
				    int             page)
{
	PdfDocumentSearch *search = PDF_DOCUMENT (document_find)->search;

	g_return_val_if_fail (search != NULL, FALSE);

	return search->pages[page] != NULL;
}

double
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
pdf_document_ps_exporter_begin (EvPSExporter *exporter, const char *filename)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);
	int n_pages;
	
	n_pages = pdf_document_get_n_pages (EV_DOCUMENT (exporter));
	pdf_document->ps_file = poppler_ps_file_new (pdf_document->document,
						     filename, n_pages);
}

static void
pdf_document_ps_exporter_do_page (EvPSExporter *exporter, int page)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);
	PopplerPage *poppler_page;

	g_return_if_fail (pdf_document->ps_file != NULL);

	poppler_page = poppler_document_get_page (pdf_document->document, page);
	poppler_page_render_to_ps (poppler_page, pdf_document->ps_file);
}

static void
pdf_document_ps_exporter_end (EvPSExporter *exporter)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (exporter);

	poppler_ps_file_free (pdf_document->ps_file);
	pdf_document->ps_file = NULL;
}

static void
pdf_document_ps_exporter_iface_init (EvPSExporterIface *iface)
{
        iface->begin = pdf_document_ps_exporter_begin;
        iface->do_page = pdf_document_ps_exporter_do_page;
        iface->end = pdf_document_ps_exporter_end;
}

PdfDocument *
pdf_document_new (void)
{
	return PDF_DOCUMENT (g_object_new (PDF_TYPE_DOCUMENT, NULL));
}
