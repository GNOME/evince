/* ev-document-attachments.h
 *  this file is part of evince, a gnome document viewer
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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_ATTACHMENTS_H
#define EV_DOCUMENT_ATTACHMENTS_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_ATTACHMENTS		(ev_document_attachments_get_type ())
#define EV_DOCUMENT_ATTACHMENTS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_ATTACHMENTS, EvDocumentAttachments))
#define EV_DOCUMENT_ATTACHMENTS_IFACE(k)	(G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_ATTACHMENTS, EvDocumentAttachmentsInterface))
#define EV_IS_DOCUMENT_ATTACHMENTS(o)	        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_ATTACHMENTS))
#define EV_IS_DOCUMENT_ATTACHMENTS_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_ATTACHMENTS))
#define EV_DOCUMENT_ATTACHMENTS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_ATTACHMENTS, EvDocumentAttachmentsInterface))

typedef struct _EvDocumentAttachments          EvDocumentAttachments;
typedef struct _EvDocumentAttachmentsInterface EvDocumentAttachmentsInterface;

struct _EvDocumentAttachmentsInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	gboolean  (* has_attachments) (EvDocumentAttachments *document_attachments);
	GList    *(* get_attachments) (EvDocumentAttachments *document_attachments);
};

GType     ev_document_attachments_get_type        (void) G_GNUC_CONST;

gboolean  ev_document_attachments_has_attachments (EvDocumentAttachments *document_attachments);
GList    *ev_document_attachments_get_attachments (EvDocumentAttachments *document_attachments);

G_END_DECLS

#endif /* EV_DOCUMENT_ATTACHMENTS_H */
