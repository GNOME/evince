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

/* Acrobat insists on calling the TOC a bookmark, b/c they allow people to add
 * their own..  We will continue this convention despite it being an obviously
 * bad name.
 */

#ifndef EV_DOCUMENT_BOOKMARKS_H
#define EV_DOCUMENT_BOOKMARKS_H

#include <glib-object.h>
#include <glib.h>
#include <gdk/gdk.h>
#include "ev-document.h"

G_BEGIN_DECLS


#define EV_TYPE_DOCUMENT_BOOKMARKS	      (ev_document_bookmarks_get_type ())
#define EV_DOCUMENT_BOOKMARKS(o)	      (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_BOOKMARKS, EvDocumentBookmarks))
#define EV_DOCUMENT_BOOKMARKS_IFACE(k)	      (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_BOOKMARKS, EvDocumentBookmarksIface))
#define EV_IS_DOCUMENT_BOOKMARKS(o)	      (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_BOOKMARKS))
#define EV_IS_DOCUMENT_BOOKMARKS_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_BOOKMARKS))
#define EV_DOCUMENT_BOOKMARKS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_BOOKMARKS, EvDocumentBookmarksIface))

typedef struct _EvDocumentBookmarks		EvDocumentBookmarks;
typedef struct _EvDocumentBookmarksIface	EvDocumentBookmarksIface;
typedef struct _EvDocumentBookmarksIter 	EvDocumentBookmarksIter;

typedef enum
{
	EV_DOCUMENT_BOOKMARKS_TYPE_TITLE,
	EV_DOCUMENT_BOOKMARKS_TYPE_LINK,
	EV_DOCUMENT_BOOKMARKS_TYPE_EXTERNAL_URI,
} EvDocumentBookmarksType;

struct _EvDocumentBookmarksIface
{
	GTypeInterface base_iface;

	/* Methods  */
	gboolean                 (* has_document_bookmarks) (EvDocumentBookmarks      *document_bookmarks);
	EvDocumentBookmarksIter *(* begin_read)             (EvDocumentBookmarks      *document_bookmarks);
	gboolean                 (* get_values)             (EvDocumentBookmarks      *document_bookmarks,
							     EvDocumentBookmarksIter  *iter,
							     gchar                   **title,
							     EvDocumentBookmarksType  *type,
							     gint                     *page);
	EvDocumentBookmarksIter *(* get_child)              (EvDocumentBookmarks      *document_bookmarks,
							     EvDocumentBookmarksIter  *iter);
	gboolean                 (* next)                   (EvDocumentBookmarks      *document_bookmarks,
							     EvDocumentBookmarksIter  *iter);
	void                     (* free_iter)              (EvDocumentBookmarks      *document_bookmarks,
							     EvDocumentBookmarksIter  *iter);
};

GType                    ev_document_bookmarks_get_type               (void);
gboolean                 ev_document_bookmarks_has_document_bookmarks (EvDocumentBookmarks      *document_bookmarks);
EvDocumentBookmarksIter *ev_document_bookmarks_begin_read             (EvDocumentBookmarks      *document_bookmarks);
gboolean                 ev_document_bookmarks_get_values             (EvDocumentBookmarks      *document_bookmarks,
								       EvDocumentBookmarksIter  *iter,
								       char                    **title,
								       EvDocumentBookmarksType  *type,
								       gint                     *page);
EvDocumentBookmarksIter *ev_document_bookmarks_get_child              (EvDocumentBookmarks      *document_bookmarks,
								       EvDocumentBookmarksIter  *iter);
gboolean                 ev_document_bookmarks_next                   (EvDocumentBookmarks      *document_bookmarks,
								       EvDocumentBookmarksIter  *iter);
void                     ev_document_bookmarks_free_iter              (EvDocumentBookmarks      *document_bookmarks,
								       EvDocumentBookmarksIter  *iter);


G_END_DECLS

#endif
