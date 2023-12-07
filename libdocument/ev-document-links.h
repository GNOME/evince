/* ev-document-links.h
 *  this file is part of evince, a gnome document viewer
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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ev-macros.h"
#include "ev-document.h"
#include "ev-link.h"
#include "ev-mapping-list.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_LINKS		  (ev_document_links_get_type ())

EV_PUBLIC
G_DECLARE_INTERFACE (EvDocumentLinks, ev_document_links, EV, DOCUMENT_LINKS, GObject)

enum {
	EV_DOCUMENT_LINKS_COLUMN_MARKUP,
	EV_DOCUMENT_LINKS_COLUMN_LINK,
	EV_DOCUMENT_LINKS_COLUMN_EXPAND,
	EV_DOCUMENT_LINKS_COLUMN_PAGE_LABEL,
	EV_DOCUMENT_LINKS_COLUMN_NUM_COLUMNS
};

struct _EvDocumentLinksInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	gboolean       (* has_document_links) (EvDocumentLinks *document_links);
	GtkTreeModel  *(* get_links_model)    (EvDocumentLinks *document_links);
	EvMappingList *(* get_links)          (EvDocumentLinks *document_links,
					       EvPage          *page);
	EvLinkDest    *(* find_link_dest)     (EvDocumentLinks *document_links,
					       const gchar     *link_name);
	gint           (* find_link_page)     (EvDocumentLinks *document_links,
					       const gchar     *link_name);
};

EV_PUBLIC
gboolean       ev_document_links_has_document_links  (EvDocumentLinks *document_links);
EV_PUBLIC
GtkTreeModel  *ev_document_links_get_links_model     (EvDocumentLinks *document_links);

EV_PUBLIC
EvMappingList *ev_document_links_get_links           (EvDocumentLinks *document_links,
						      EvPage          *page);
EV_PUBLIC
EvLinkDest    *ev_document_links_find_link_dest      (EvDocumentLinks *document_links,
						      const gchar     *link_name);
EV_PUBLIC
gint           ev_document_links_find_link_page      (EvDocumentLinks *document_links,
						      const gchar     *link_name);
EV_PUBLIC
gint           ev_document_links_get_dest_page       (EvDocumentLinks *document_links,
						      EvLinkDest      *dest);
EV_PUBLIC
gchar         *ev_document_links_get_dest_page_label (EvDocumentLinks *document_links,
						      EvLinkDest      *dest);
EV_PUBLIC
gint           ev_document_links_get_link_page       (EvDocumentLinks *document_links,
						      EvLink          *link);
EV_PUBLIC
gchar         *ev_document_links_get_link_page_label (EvDocumentLinks *document_links,
						      EvLink          *link);

G_END_DECLS
