/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Red Hat, Inc.
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

#include "ev-document-find.h"
#include "ev-backend-marshalers.h"

static void ev_document_find_base_init (gpointer g_class);

GType
ev_document_find_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EvDocumentFindIface),
			ev_document_find_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvDocumentFind",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

static void
ev_document_find_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_signal_new ("find_changed",
			      EV_TYPE_DOCUMENT_FIND,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvDocumentFindIface, find_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

		initialized = TRUE;
	}
}

void
ev_document_find_begin (EvDocumentFind   *document_find,
                        const char       *search_string,
                        gboolean          case_sensitive)
{
	EvDocumentFindIface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);

        g_return_if_fail (search_string != NULL);
        
	iface->begin (document_find, search_string, case_sensitive);
}

void
ev_document_find_cancel (EvDocumentFind   *document_find)
{
	EvDocumentFindIface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);
	iface->cancel (document_find);
}

int
ev_document_find_page_has_results (EvDocumentFind *document_find,
				   int             page)
{
	EvDocumentFindIface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);
	return iface->page_has_results (document_find, page);
}

int
ev_document_find_get_n_results (EvDocumentFind *document_find)
{
	EvDocumentFindIface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);
	return iface->get_n_results (document_find);
}

gboolean
ev_document_find_get_result (EvDocumentFind *document_find,
			     int             n_result,
			     GdkRectangle   *rectangle)
{
	EvDocumentFindIface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);
	return iface->get_result (document_find, n_result, rectangle);
}

void
ev_document_find_get_progress (EvDocumentFind *document_find,
			       double          percent_complete)
{
	EvDocumentFindIface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);
	iface->get_progress (document_find, percent_complete);
}

void
ev_document_find_changed (EvDocumentFind  *document_find)
{
	g_signal_emit_by_name (document_find, "find_changed");
}
				    
