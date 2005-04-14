/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include "ev-document.h"

#include "ev-backend-marshalers.h"
#include "ev-job-queue.h"

static void ev_document_class_init (gpointer g_class);


GMutex *ev_doc_mutex = NULL;

#define LOG(x) 
GType
ev_document_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EvDocumentIface),
			NULL,
			NULL,
			(GClassInitFunc)ev_document_class_init
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvDocument",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

GQuark
ev_document_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("ev-document-error-quark");

  return q;
}

static void
ev_document_class_init (gpointer g_class)
{
	g_object_interface_install_property (g_class,
				g_param_spec_string ("title",
						     "Document Title",
						     "The title of the document",
						     NULL,
						     G_PARAM_READABLE));
}

#define PAGE_CACHE_STRING "ev-page-cache"

EvPageCache *
ev_document_get_page_cache (EvDocument *document)
{
	EvPageCache *page_cache;

	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);

	page_cache = g_object_get_data (G_OBJECT (document), PAGE_CACHE_STRING);
	if (page_cache == NULL) {
		page_cache = _ev_page_cache_new (document);
		g_object_set_data_full (G_OBJECT (document), PAGE_CACHE_STRING, page_cache, g_object_unref);
	}

	return page_cache;
}

GMutex *
ev_document_get_doc_mutex (void)
{
	if (ev_doc_mutex == NULL) {
		ev_doc_mutex = g_mutex_new ();
	}
	return ev_doc_mutex;
}


gboolean
ev_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gboolean retval;
	LOG ("ev_document_load");
	retval = iface->load (document, uri, error);

	/* Call this to make the initial cached copy */
	if (retval)
		ev_document_get_page_cache (document);

	return retval;
}

gboolean
ev_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gboolean retval;

	LOG ("ev_document_save");
	retval = iface->save (document, uri, error);

	return retval;
}

char *
ev_document_get_title (EvDocument  *document)
{
	char *title;

	LOG ("ev_document_get_title");
	g_object_get (document, "title", &title, NULL);

	return title;
}

int
ev_document_get_n_pages (EvDocument  *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gint retval;

	LOG ("ev_document_get_n_pages");
	retval = iface->get_n_pages (document);

	return retval;
}

void
ev_document_get_page_size   (EvDocument   *document,
			     int           page,
			     double       *width,
			     double       *height)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	LOG ("ev_document_get_page_size");
	iface->get_page_size (document, page, width, height);
}

char *
ev_document_get_page_label(EvDocument    *document,
			   int             page)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	LOG ("ev_document_get_page_label");
	if (iface->get_page_label == NULL)
		return NULL;

	return iface->get_page_label (document, page);
}

gboolean
ev_document_can_get_text (EvDocument  *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	return iface->can_get_text (document);
}

char *
ev_document_get_text (EvDocument  *document,
		      int          page,
		      EvRectangle *rect)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	char *retval;

	LOG ("ev_document_get_text");
	retval = iface->get_text (document, page, rect);

	return retval;
}

GList *
ev_document_get_links (EvDocument *document,
		       int         page)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	GList *retval;

	LOG ("ev_document_get_link");
	if (iface->get_links == NULL)
		return NULL;
	retval = iface->get_links (document, page);

	return retval;
}



GdkPixbuf *
ev_document_render_pixbuf (EvDocument *document,
			   int         page,
			   double      scale)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	GdkPixbuf *retval;

	LOG ("ev_document_render_pixbuf");
	g_assert (iface->render_pixbuf);

	retval = iface->render_pixbuf (document, page, scale);

	return retval;
}

