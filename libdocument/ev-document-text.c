/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2010 Yaco Sistemas, Daniel Garcia <danigm@yaco.es>
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
 *  $Id$
 */

#include "config.h"

#include "ev-document-text.h"

G_DEFINE_INTERFACE (EvDocumentText, ev_document_text, 0)

static void
ev_document_text_default_init (EvDocumentTextInterface *klass)
{
}

gchar *
ev_document_text_get_text (EvDocumentText   *document_text,
			   EvPage           *page)
{
	EvDocumentTextInterface *iface = EV_DOCUMENT_TEXT_GET_IFACE (document_text);

	if (!iface->get_text)
		return NULL;

	return iface->get_text (document_text, page);
}


gboolean
ev_document_text_get_text_layout (EvDocumentText   *document_text,
				  EvPage           *page,
				  EvRectangle     **areas,
				  guint            *n_areas)
{
	EvDocumentTextInterface *iface = EV_DOCUMENT_TEXT_GET_IFACE (document_text);

	if (!iface->get_text_layout)
		return FALSE;

	return iface->get_text_layout (document_text, page, areas, n_areas);
}

cairo_region_t *
ev_document_text_get_text_mapping (EvDocumentText *document_text,
				   EvPage         *page)
{
	EvDocumentTextInterface *iface = EV_DOCUMENT_TEXT_GET_IFACE (document_text);

	if (!iface->get_text_mapping)
		return NULL;

	return iface->get_text_mapping (document_text, page);
}

/**
 * ev_document_text_get_text_attrs:
 * @document_text: a #EvDocumentText
 * @page: a #EvPage
 *
 * FIXME
 *
 * Returns: (transfer full): a newly created #PangoAttrList
 *
 * Since: 3.10
 */
PangoAttrList *
ev_document_text_get_text_attrs (EvDocumentText *document_text,
				 EvPage         *page)
{
	EvDocumentTextInterface *iface = EV_DOCUMENT_TEXT_GET_IFACE (document_text);

	if (!iface->get_text_attrs)
		return NULL;

	return iface->get_text_attrs (document_text, page);
}
