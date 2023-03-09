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

G_DEFINE_INTERFACE (EvDocumentLinks, ev_document_links, 0)

static void
ev_document_links_default_init (EvDocumentLinksInterface *klass)
{
}

gboolean
ev_document_links_has_document_links (EvDocumentLinks *document_links)
{
	EvDocumentLinksInterface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	gboolean retval;

	retval = iface->has_document_links (document_links);

	return retval;
}

/**
 * ev_document_links_get_links_model:
 * @document_links: an #EvDocumentLinks
 *
 * Returns: (transfer full): a #GtkTreeModel
 */
GtkTreeModel *
ev_document_links_get_links_model (EvDocumentLinks *document_links)
{
	EvDocumentLinksInterface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	GtkTreeModel *retval;

	retval = iface->get_links_model (document_links);

	return retval;
}

EvMappingList *
ev_document_links_get_links (EvDocumentLinks *document_links,
			     EvPage          *page)
{
	EvDocumentLinksInterface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);

	return iface->get_links (document_links, page);
}

/**
 * ev_document_links_find_link_dest:
 * @document_links: an #EvDocumentLinks
 * @link_name: the link name
 *
 * Returns: (transfer full): an #EvLinkDest
 */
EvLinkDest *
ev_document_links_find_link_dest (EvDocumentLinks *document_links,
				  const gchar     *link_name)
{
	EvDocumentLinksInterface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	EvLinkDest *retval;

	ev_document_doc_mutex_lock ();
	retval = iface->find_link_dest (document_links, link_name);
	ev_document_doc_mutex_unlock ();

	return retval;
}

gint
ev_document_links_find_link_page (EvDocumentLinks *document_links,
				  const gchar     *link_name)
{
	EvDocumentLinksInterface *iface = EV_DOCUMENT_LINKS_GET_IFACE (document_links);
	gint retval;

	ev_document_doc_mutex_lock ();
	retval = iface->find_link_page (document_links, link_name);
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
		page = ev_document_links_find_link_page (document_links,
							 ev_link_dest_get_named_dest (dest));
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

static EvLinkDest *
get_link_dest (EvLink *link)
{
	EvLinkAction *action;

	action = ev_link_get_action (link);
	if (!action)
		return NULL;

	if (ev_link_action_get_action_type (action) !=
	    EV_LINK_ACTION_TYPE_GOTO_DEST)
		return NULL;

	return ev_link_action_get_dest (action);
}

gint
ev_document_links_get_link_page (EvDocumentLinks *document_links,
				 EvLink          *link)
{
	EvLinkDest *dest;

	dest = get_link_dest (link);

	return dest ? ev_document_links_get_dest_page (document_links, dest) : -1;
}

gchar *
ev_document_links_get_link_page_label (EvDocumentLinks *document_links,
				       EvLink          *link)
{
	EvLinkDest *dest;

	dest = get_link_dest (link);

	return dest ? ev_document_links_get_dest_page_label (document_links, dest) : NULL;
}
