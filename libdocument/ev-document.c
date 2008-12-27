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

static void ev_document_class_init (gpointer g_class);


GMutex *ev_doc_mutex = NULL;
GMutex *ev_fc_mutex = NULL;

GType
ev_document_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EvDocumentIface),
			NULL,
			NULL,
			(GClassInitFunc)ev_document_class_init
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvDocument",
					       &our_info, (GTypeFlags)0);
		
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
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
}

GMutex *
ev_document_get_doc_mutex (void)
{
	if (ev_doc_mutex == NULL) {
		ev_doc_mutex = g_mutex_new ();
	}
	return ev_doc_mutex;
}

void
ev_document_doc_mutex_lock (void)
{
	g_mutex_lock (ev_document_get_doc_mutex ());
}

void
ev_document_doc_mutex_unlock (void)
{
	g_mutex_unlock (ev_document_get_doc_mutex ());
}

gboolean
ev_document_doc_mutex_trylock (void)
{
	return g_mutex_trylock (ev_document_get_doc_mutex ());
}

GMutex *
ev_document_get_fc_mutex (void)
{
	if (ev_fc_mutex == NULL) {
		ev_fc_mutex = g_mutex_new ();
	}
	return ev_fc_mutex;
}

void
ev_document_fc_mutex_lock (void)
{
	g_mutex_lock (ev_document_get_fc_mutex ());
}

void
ev_document_fc_mutex_unlock (void)
{
	g_mutex_unlock (ev_document_get_fc_mutex ());
}

gboolean
ev_document_fc_mutex_trylock (void)
{
	return g_mutex_trylock (ev_document_get_fc_mutex ());
}

gboolean
ev_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gboolean retval;

	retval = iface->load (document, uri, error);

	return retval;
}

gboolean
ev_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gboolean retval;

	retval = iface->save (document, uri, error);

	return retval;
}

int
ev_document_get_n_pages (EvDocument  *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gint retval;

	retval = iface->get_n_pages (document);

	return retval;
}

EvPage *
ev_document_get_page (EvDocument *document,
		      gint        index)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	EvPage *retval;

	if (iface->get_page)
		retval = iface->get_page (document, index);
	else
		retval = ev_page_new (index);

	return retval;
}

void
ev_document_get_page_size (EvDocument *document,
			   EvPage     *page,
			   double     *width,
			   double     *height)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	iface->get_page_size (document, page, width, height);
}

char *
ev_document_get_page_label (EvDocument *document,
			    EvPage     *page)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	if (iface->get_page_label == NULL)
		return NULL;

	return iface->get_page_label (document, page);
}

EvDocumentInfo *
ev_document_get_info (EvDocument *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	return iface->get_info (document);
}

gboolean
ev_document_has_attachments (EvDocument *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	if (iface->has_attachments == NULL)
		return FALSE;
	
	return iface->has_attachments (document);
}

GList *
ev_document_get_attachments (EvDocument *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	GList *retval;

	if (iface->get_attachments == NULL)
		return NULL;
	retval = iface->get_attachments (document);

	return retval;
}

cairo_surface_t *
ev_document_render (EvDocument      *document,
		    EvRenderContext *rc)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	cairo_surface_t *retval;

	g_assert (iface->render);

	retval = iface->render (document, rc);

	return retval;
}

void
ev_document_info_free (EvDocumentInfo *info)
{
	if (info == NULL)
		return;

	g_free (info->title);
	g_free (info->format);
	g_free (info->author);
	g_free (info->subject);
	g_free (info->keywords);
	g_free (info->creator);
	g_free (info->producer);
	g_free (info->linearized);
	g_free (info->security);
	
	g_free (info);
}


/* Compares two rects.  returns 0 if they're equal */
#define EPSILON 0.0000001

gint
ev_rect_cmp (EvRectangle *a,
	     EvRectangle *b)
{
	if (a == b)
		return 0;
	if (a == NULL || b == NULL)
		return 1;

	return ! ((ABS (a->x1 - b->x1) < EPSILON) &&
		  (ABS (a->y1 - b->y1) < EPSILON) &&
		  (ABS (a->x2 - b->x2) < EPSILON) &&
		  (ABS (a->y2 - b->y2) < EPSILON));
}



