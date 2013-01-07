/* ev-document-attachments.c
 *  this file is part of evince, a gnome document_links viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include "config.h"

#include "ev-document-attachments.h"
#include "ev-document.h"

G_DEFINE_INTERFACE (EvDocumentAttachments, ev_document_attachments, 0)

static void
ev_document_attachments_default_init (EvDocumentAttachmentsInterface *klass)
{
}

gboolean
ev_document_attachments_has_attachments (EvDocumentAttachments *document_attachments)
{
	EvDocumentAttachmentsInterface *iface = EV_DOCUMENT_ATTACHMENTS_GET_IFACE (document_attachments);

	return iface->has_attachments (document_attachments);
}

/**
 * ev_document_attachments_get_attachments:
 * @document_attachments: an #EvDocumentAttachments
 *
 * Returns: (transfer full) (element-type EvAttachment): a list of #EvAttachment objects
 */
GList *
ev_document_attachments_get_attachments (EvDocumentAttachments *document_attachments)
{
	EvDocumentAttachmentsInterface *iface = EV_DOCUMENT_ATTACHMENTS_GET_IFACE (document_attachments);

	return iface->get_attachments (document_attachments);
}


