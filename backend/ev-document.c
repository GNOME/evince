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
#include "ev-backend-marshal.c"

static void ev_document_base_init (gpointer g_class);

GType
ev_document_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EvDocumentIface),
			ev_document_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvDocument",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

static void
ev_document_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_signal_new ("found",
			      EV_TYPE_DOCUMENT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvDocumentIface, found),
			      NULL, NULL,
			      _ev_backend_marshal_VOID__POINTER_INT_DOUBLE,
			      G_TYPE_NONE, 3,
			      G_TYPE_POINTER,
			      G_TYPE_INT,
			      G_TYPE_DOUBLE);

		initialized = TRUE;
	}
}

gboolean
ev_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	return iface->load (document, uri, error);
}

int
ev_document_get_n_pages (EvDocument  *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	return iface->get_n_pages (document);
}

void
ev_document_set_page (EvDocument  *document,
		      int          page)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->set_page (document, page);
}

void
ev_document_set_target (EvDocument  *document,
			GdkDrawable *target)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->set_target (document, target);
}

void
ev_document_set_scale (EvDocument   *document,
		       double        scale)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->set_scale (document, scale);
}

void
ev_document_set_page_offset (EvDocument  *document,
			     int          x,
			     int          y)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->set_page_offset (document, x, y);
}

void
ev_document_get_page_size   (EvDocument   *document,
			     int          *width,
			     int          *height)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->get_page_size (document, width, height);
}

void
ev_document_render (EvDocument  *document,
		    int          clip_x,
		    int          clip_y,
		    int          clip_width,
		    int          clip_height)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->render (document, clip_x, clip_y, clip_width, clip_height);
}

void
ev_document_begin_find (EvDocument   *document,
			const char   *search_string,
			gboolean      case_sensitive)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->begin_find (document, search_string, case_sensitive);
}

void
ev_document_end_find (EvDocument   *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->end_find (document);
}
