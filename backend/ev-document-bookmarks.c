/* ev-document-bookmarks.h
 *  this file is part of evince, a gnome document_bookmarks viewer
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

#include "ev-document-bookmarks.h"

GType
ev_document_bookmarks_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EvDocumentBookmarksIface),
			NULL,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvDocumentBookmarks",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

gboolean
ev_document_bookmarks_has_document_bookmarks (EvDocumentBookmarks *document_bookmarks)
{
	EvDocumentBookmarksIface *iface = EV_DOCUMENT_BOOKMARKS_GET_IFACE (document_bookmarks);
	return iface->has_document_bookmarks (document_bookmarks);
}

EvDocumentBookmarksIter *
ev_document_bookmarks_begin_read (EvDocumentBookmarks *document_bookmarks)
{
	EvDocumentBookmarksIface *iface = EV_DOCUMENT_BOOKMARKS_GET_IFACE (document_bookmarks);

	return iface->begin_read (document_bookmarks);
}

 /*
  * This function gets the values at a node.  You need to g_free the title.
  * Additionally, if page is -1, the link doesn't go anywhere.
  */
gboolean 
ev_document_bookmarks_get_values (EvDocumentBookmarks      *document_bookmarks,
				  EvDocumentBookmarksIter  *iter,
				  char                    **title,
				  EvDocumentBookmarksType  *type,
				  gint                     *page)
{
	EvDocumentBookmarksIface *iface = EV_DOCUMENT_BOOKMARKS_GET_IFACE (document_bookmarks);

	return iface->get_values (document_bookmarks, iter, title, type, page);
}

EvDocumentBookmarksIter *
ev_document_bookmarks_get_child (EvDocumentBookmarks     *document_bookmarks,
				 EvDocumentBookmarksIter *iter)
{
	EvDocumentBookmarksIface *iface = EV_DOCUMENT_BOOKMARKS_GET_IFACE (document_bookmarks);

	return iface->get_child (document_bookmarks, iter);
}


gboolean 
ev_document_bookmarks_next (EvDocumentBookmarks     *document_bookmarks,
			    EvDocumentBookmarksIter *iter)
{
	EvDocumentBookmarksIface *iface = EV_DOCUMENT_BOOKMARKS_GET_IFACE (document_bookmarks);

	return iface->next (document_bookmarks, iter);
}


void
ev_document_bookmarks_free_iter (EvDocumentBookmarks     *document_bookmarks,
				 EvDocumentBookmarksIter *iter)
{
	EvDocumentBookmarksIface *iface = EV_DOCUMENT_BOOKMARKS_GET_IFACE (document_bookmarks);

	iface->free_iter (document_bookmarks, iter);
}
