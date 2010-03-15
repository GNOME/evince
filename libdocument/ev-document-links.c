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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
			     EvPage          *page)
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

/* Helper functions */
gint
ev_document_links_get_dest_page (EvDocumentLinks *document_links,
				 EvLinkDest      *dest)
{
	gint page = -1;

	switch (ev_link_dest_get_dest_type (dest)) {
	case EV_LINK_DEST_TYPE_NAMED: {
		EvLinkDest *dest2;

		dest2 = ev_document_links_find_link_dest (document_links,
							  ev_link_dest_get_named_dest (dest));
		if (dest2) {
			page = ev_link_dest_get_page (dest2);
			g_object_unref (dest2);
		}
	}
		break;
	case EV_LINK_DEST_TYPE_PAGE_LABEL:
		ev_document_find_page_by_label (EV_DOCUMENT (document_links),
						ev_link_dest_get_page_label (dest),
						&page);
		break;
	default:
		page = ev_link_dest_get_page (dest);
	}

	return page;
}

gchar *
ev_document_links_get_dest_page_label (EvDocumentLinks *document_links,
				       EvLinkDest      *dest)
{
	gchar *label = NULL;

	if (ev_link_dest_get_dest_type (dest) == EV_LINK_DEST_TYPE_PAGE_LABEL) {
		label = g_strdup (ev_link_dest_get_page_label (dest));
	} else {
		gint page;

		page = ev_document_links_get_dest_page (document_links, dest);
		if (page != -1)
			label = ev_document_get_page_label (EV_DOCUMENT (document_links),
							    page);
	}

	return label;
}
