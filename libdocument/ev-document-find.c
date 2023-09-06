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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ev-document-find.h"

G_DEFINE_INTERFACE (EvDocumentFind, ev_document_find, 0)

static void
ev_document_find_default_init (EvDocumentFindInterface *klass)
{
}

/**
 * ev_document_find_find_text:
 * @document_find: an #EvDocumentFind
 * @page: an #EvPage
 * @text: text to find
 * @options: a set of #EvFindOptions
 *
 * Returns: (transfer full) (element-type EvFindRectangle): a list of results
 */
GList *
ev_document_find_find_text (EvDocumentFind *document_find,
			    EvPage         *page,
			    const gchar    *text,
			    EvFindOptions   options)
{
	EvDocumentFindInterface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);

	return iface->find_text (document_find, page, text, options);
}

/* EvFindRectangle */
G_DEFINE_BOXED_TYPE (EvFindRectangle, ev_find_rectangle, ev_find_rectangle_copy, ev_find_rectangle_free)

EvFindRectangle *
ev_find_rectangle_new (void)
{
	return g_slice_new0 (EvFindRectangle);
}

EvFindRectangle *
ev_find_rectangle_copy (EvFindRectangle *rectangle)
{
	g_return_val_if_fail (rectangle != NULL, NULL);
	return g_slice_dup (EvFindRectangle, rectangle);
}

void
ev_find_rectangle_free (EvFindRectangle *rectangle)
{
	g_slice_free (EvFindRectangle, rectangle);
}

EvFindOptions
ev_document_find_get_supported_options (EvDocumentFind *document_find)
{
	EvDocumentFindInterface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);

	if (iface->get_supported_options)
		return iface->get_supported_options (document_find);
	return 0;
}
