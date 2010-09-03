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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_LINKS_H
#define EV_DOCUMENT_LINKS_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ev-document.h"
#include "ev-link.h"
#include "ev-mapping-list.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_LINKS		  (ev_document_links_get_type ())
#define EV_DOCUMENT_LINKS(o)		  (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_LINKS, EvDocumentLinks))
#define EV_DOCUMENT_LINKS_IFACE(k)	  (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_LINKS, EvDocumentLinksInterface))
#define EV_IS_DOCUMENT_LINKS(o)		  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_LINKS))
#define EV_IS_DOCUMENT_LINKS_IFACE(k)	  (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_LINKS))
#define EV_DOCUMENT_LINKS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_LINKS, EvDocumentLinksInterface))

typedef struct _EvDocumentLinks	         EvDocumentLinks;
typedef struct _EvDocumentLinksInterface EvDocumentLinksInterface;

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

GType          ev_document_links_get_type            (void) G_GNUC_CONST;
gboolean       ev_document_links_has_document_links  (EvDocumentLinks *document_links);
GtkTreeModel  *ev_document_links_get_links_model     (EvDocumentLinks *document_links);

EvMappingList *ev_document_links_get_links           (EvDocumentLinks *document_links,
						      EvPage          *page);
EvLinkDest    *ev_document_links_find_link_dest      (EvDocumentLinks *document_links,
						      const gchar     *link_name);
gint           ev_document_links_find_link_page      (EvDocumentLinks *document_links,
						      const gchar     *link_name);
gint           ev_document_links_get_dest_page       (EvDocumentLinks *document_links,
						      EvLinkDest      *dest);
gchar         *ev_document_links_get_dest_page_label (EvDocumentLinks *document_links,
						      EvLinkDest      *dest);
gint           ev_document_links_get_link_page       (EvDocumentLinks *document_links,
						      EvLink          *link);
gchar         *ev_document_links_get_link_page_label (EvDocumentLinks *document_links,
						      EvLink          *link);

G_END_DECLS

#endif
