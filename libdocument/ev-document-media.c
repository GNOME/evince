/* ev-document-media.c
 *  this file is part of evince, a gnome document_links viewer
 *
 * Copyright (C) 2015 Igalia S.L.
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include "ev-document-media.h"

G_DEFINE_INTERFACE (EvDocumentMedia, ev_document_media, 0)

static void
ev_document_media_default_init (EvDocumentMediaInterface *klass)
{
}

EvMappingList *
ev_document_media_get_media_mapping (EvDocumentMedia *document_media,
                                     EvPage           *page)
{
	EvDocumentMediaInterface *iface = EV_DOCUMENT_MEDIA_GET_IFACE (document_media);

	return iface->get_media_mapping (document_media, page);
}
