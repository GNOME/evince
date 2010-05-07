/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Anders Carlsson <andersca@gnome.org>
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

#include <config.h>
#include "ev-document-thumbnails.h"
#include "ev-document.h"

G_DEFINE_INTERFACE (EvDocumentThumbnails, ev_document_thumbnails, 0)

static void
ev_document_thumbnails_default_init (EvDocumentThumbnailsInterface *klass)
{
}

GdkPixbuf *
ev_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document,
				      EvRenderContext      *rc,
				      gboolean              border)
{
	EvDocumentThumbnailsInterface *iface;

	g_return_val_if_fail (EV_IS_DOCUMENT_THUMBNAILS (document), NULL);
	g_return_val_if_fail (EV_IS_RENDER_CONTEXT (rc), NULL);

	iface = EV_DOCUMENT_THUMBNAILS_GET_IFACE (document);

	return iface->get_thumbnail (document, rc, border);
}

void
ev_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
				       EvRenderContext      *rc,
				       gint                 *width,
				       gint                 *height)
{
	EvDocumentThumbnailsInterface *iface;

	g_return_if_fail (EV_IS_DOCUMENT_THUMBNAILS (document));
	g_return_if_fail (EV_IS_RENDER_CONTEXT (rc));
	g_return_if_fail (width != NULL);
	g_return_if_fail (height != NULL);

	iface = EV_DOCUMENT_THUMBNAILS_GET_IFACE (document);
	iface->get_dimensions (document, rc, width, height);
}

