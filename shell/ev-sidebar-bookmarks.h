/* ev-sidebar-bookmarks.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include <gtk/gtk.h>
#include <glib-object.h>

#include "ev-bookmarks.h"

G_BEGIN_DECLS

typedef struct _EvSidebarBookmarks        EvSidebarBookmarks;
typedef struct _EvSidebarBookmarksClass   EvSidebarBookmarksClass;
typedef struct _EvSidebarBookmarksPrivate EvSidebarBookmarksPrivate;

#define EV_TYPE_SIDEBAR_BOOKMARKS              (ev_sidebar_bookmarks_get_type())
#define EV_SIDEBAR_BOOKMARKS(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarks))
#define EV_SIDEBAR_BOOKMARKS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarksClass))
#define EV_IS_SIDEBAR_BOOKMARKS(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_SIDEBAR_BOOKMARKS))
#define EV_IS_SIDEBAR_BOOKMARKS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_SIDEBAR_BOOKMARKS))
#define EV_SIDEBAR_BOOKMARKS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_SIDEBAR_BOOKMARKS, EvSidebarBookmarksClass))

struct _EvSidebarBookmarks {
	GtkBox base_instance;
};

struct _EvSidebarBookmarksClass {
	GtkBoxClass base_class;

        void (*add_bookmark) (EvSidebarBookmarks *sidebar_bookmarks);
        void (*activated)    (EvSidebarBookmarks *sidebar_bookmarks,
                              gint                old_page,
                              gint                page);
};

GType      ev_sidebar_bookmarks_get_type      (void) G_GNUC_CONST;
GtkWidget *ev_sidebar_bookmarks_new           (void);
void       ev_sidebar_bookmarks_set_bookmarks (EvSidebarBookmarks *sidebar_bookmarks,
					       EvBookmarks        *bookmarks);
G_END_DECLS
