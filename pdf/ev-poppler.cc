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


enum {
	PROP_0,
	PROP_TITLE
};


struct _PdfDocumentClass
{
	GObjectClass parent_class;
};

struct _PdfDocument
{
	GObject parent_instance;

	PopplerDocument *document;
	PopplerPage *page;
	double scale;
	gchar *password;
};

static void pdf_document_document_iface_init            (EvDocumentIface           *iface);
static void pdf_document_security_iface_init            (EvDocumentSecurityIface   *iface);
static void pdf_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);
static void pdf_document_document_links_iface_init      (EvDocumentLinksIface      *iface);
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
#if 0
				 G_IMPLEMENT_INTERFACE (EV_TYPE_PS_EXPORTER,
							pdf_document_ps_exporter_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_FIND,
							pdf_document_find_iface_init);
#endif
			 });






static void
pdf_document_get_property (GObject *object,
		           guint prop_id,
		           GValue *value,
		           GParamSpec *pspec)
{
	PdfDocument *pdf_document = PDF_DOCUMENT (object);

	switch (prop_id)
	{
		case PROP_TITLE:
			if (pdf_document->document == NULL)
				g_value_set_string (value, NULL);
			else
				g_object_get_property (G_OBJECT (pdf_document->document), "title", value);
			break;
	}
}

static void
pdf_document_class_init (PdfDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = pdf_document_get_property;

	g_object_class_override_property (gobject_class, PROP_TITLE, "title");
}

static void
pdf_document_init (PdfDocument *pdf_document)
{
	pdf_document->page = NULL;
	pdf_document->scale = 1.0;
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
pdf_document_set_page (EvDocument   *document,
		       int           page)
{
	page = CLAMP (page, 0, poppler_document_get_n_pages (PDF_DOCUMENT (document)->document) - 1);

	PDF_DOCUMENT (document)->page = poppler_document_get_page (PDF_DOCUMENT (document)->document, page);
}

static int
pdf_document_get_page (EvDocument   *document)
{
	PdfDocument *pdf_document;

	pdf_document = PDF_DOCUMENT (document);

	if (pdf_document->page)
		return poppler_page_get_index (pdf_document->page);

	return 1;
}

static void 
pdf_document_set_scale (EvDocument   *document,
			double        scale)
{
	PDF_DOCUMENT (document)->scale = scale;
}


static void
get_size_from_page (PopplerPage *poppler_page,
		    double       scale,
		    int         *width,
		    int         *height)
{
	gdouble width_d, height_d;
	poppler_page_get_size (poppler_page, &width_d, &height_d);
	if (width)
		*width = (int) (width_d * scale);
	if (height)
		*height = (int) (height_d * scale);

}

static void
pdf_document_get_page_size (EvDocument   *document,
			    int           page,
			    int          *width,
			    int          *height)
{
	PopplerPage *poppler_page = NULL;

	if (page == -1)
		poppler_page = PDF_DOCUMENT (document)->page;
	else
		poppler_page = poppler_document_get_page (PDF_DOCUMENT (document)->document,
							  page);

	if (poppler_page == NULL)
		poppler_document_get_page (PDF_DOCUMENT (document)->document, 0);

	get_size_from_page (poppler_page,
			    PDF_DOCUMENT (document)->scale,
			    width, height);
}

static char *
pdf_document_get_page_label (EvDocument *document,
			     int         page)
{
	PopplerPage *poppler_page = NULL;
	char *label = NULL;

	if (page == -1)
		poppler_page = PDF_DOCUMENT (document)->page;
	else
		poppler_page = poppler_document_get_page (PDF_DOCUMENT (document)->document,
							  page);

	g_object_get (poppler_page,
		      "label", &label,
		      NULL);

	return label;
}

static GList *
pdf_document_get_links (EvDocument *document)
{
	PdfDocument *pdf_document;
	GList *retval = NULL;
	GList *mapping_list;
	GList *list;
	gint height;

	pdf_document = PDF_DOCUMENT (document);
	g_return_val_if_fail (pdf_document->page != NULL, NULL);

	mapping_list = poppler_page_get_link_mapping (pdf_document->page);
	get_size_from_page (pdf_document->page, 1.0, NULL, &height);

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
pdf_document_render_pixbuf (EvDocument   *document)
{
	PdfDocument *pdf_document;
	GdkPixbuf *pixbuf;
	gint width, height;

	pdf_document = PDF_DOCUMENT (document);
	g_return_val_if_fail (pdf_document->page != NULL, NULL);

	get_size_from_page (pdf_document->page,
			    pdf_document->scale,
			    &width, &height);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 FALSE, 8,
				 width, height);

	poppler_page_render_to_pixbuf (pdf_document->page,
				       0, 0,
				       width, height,
				       pdf_document->scale,
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



static void
pdf_document_document_iface_init (EvDocumentIface *iface)
{
	iface->save = pdf_document_save;
	iface->load = pdf_document_load;
	iface->get_n_pages = pdf_document_get_n_pages;
	iface->set_page = pdf_document_set_page;
	iface->get_page = pdf_document_get_page;
	iface->set_scale = pdf_document_set_scale;
	iface->get_page_size = pdf_document_get_page_size;
	iface->get_page_label = pdf_document_get_page_label;
	iface->get_links = pdf_document_get_links;
	iface->render_pixbuf = pdf_document_render_pixbuf;
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
		int page_width, page_height;

		get_size_from_page (poppler_page, 1.0, &page_width, &page_height);

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

PdfDocument *
pdf_document_new (void)
{
	return PDF_DOCUMENT (g_object_new (PDF_TYPE_DOCUMENT, NULL));
}
