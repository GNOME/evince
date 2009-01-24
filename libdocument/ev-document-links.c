/* ev-document-links.h
 *  this file is part of evince, a gnome document_links viewer
 * 
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * Author:
 *   Jonathan Blandford <jrb@alum.mit.edu>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "ev-document-links.h"

EV_DEFINE_INTERFACE (EvDocumentLinks, ev_document_links, 0)

static void
ev_document_links_class_init (EvDocumentLinksIface *klass)
{
}

gboolean
ev_document_links_has_document_links (EvDocumentLinks *document_links)
{
	EvDocumentLinksIface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	gboolean retval;

	retval = iface->has_document_links (document_links);

	return retval;
}

GtkTreeModel *
ev_document_links_get_links_model (EvDocumentLinks *document_links)
{
	EvDocumentLinksIface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	GtkTreeModel *retval;

	retval = iface->get_links_model (document_links);

	return retval;
}

GList *
ev_document_links_get_links (EvDocumentLinks *document_links,
			     gint             page)
{
	EvDocumentLinksIface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	GList *retval;

	retval = iface->get_links (document_links, page);

	return retval;
}

EvLinkDest *
ev_document_links_find_link_dest (EvDocumentLinks *document_links,
				  const gchar     *link_name)
{
	EvDocumentLinksIface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	EvLinkDest *retval;

	ev_document_doc_mutex_lock ();
	retval = iface->find_link_dest (document_links, link_name);
	ev_document_doc_mutex_unlock ();

	return retval;
}
