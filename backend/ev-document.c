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
}

void
ev_document_load (EvDocument  *document,
		  const char  *uri,
		  GError      *error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->load (document, uri);
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
ev_document_set_page_rect (EvDocument  *document,
			   int          x,
			   int          y,
			   int          width,
			   int          height)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	iface->set_page_rect (document, x, y, width, height);
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
