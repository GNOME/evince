/* ev-sidebar-bookmarks.h
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EV_SIDEBAR_BOOKMARKS_H__
#define __EV_SIDEBAR_BOOKMARKS_H__

#include <gtk/gtkvbox.h>
#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvSidebarBookmarks EvSidebarBookmarks;
typedef struct _EvSidebarBookmarksClass EvSidebarBookmarksClass;
typedef struct _EvSidebarBookmarksPrivate EvSidebarBookmarksPrivate;

#define EV_TYPE_SIDEBAR_BOOKMARKS		(ev_sidebar_bookmarks_get_type())
#define EV_SIDEBAR_BOOKMARKS(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarks))
#define EV_SIDEBAR_BOOKMARKS_CLASS(klass)	(G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarksClass))
#define EV_IS_SIDEBAR_BOOKMARKS(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_SIDEBAR_BOOKMARKS))
#define EV_IS_SIDEBAR_BOOKMARKS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_SIDEBAR_BOOKMARKS))
#define EV_SIDEBAR_BOOKMARKS_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarksClass))

struct _EvSidebarBookmarks {
	GtkVBox base_instance;

	EvSidebarBookmarksPrivate *priv;
};

struct _EvSidebarBookmarksClass {
	GtkVBoxClass base_class;
};

GType      ev_sidebar_bookmarks_get_type (void);
GtkWidget *ev_sidebar_bookmarks_new      (void);
void       ev_sidebar_bookmarks_set_document (EvSidebarBookmarks *ev_sidebar_bookmarks,
					      EvDocument         *document);

G_END_DECLS

#endif /* __EV_SIDEBAR_BOOKMARKS_H__ */


