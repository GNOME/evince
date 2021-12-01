/* ev-bookmarks.h
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

#include <glib-object.h>

#include "ev-metadata.h"

G_BEGIN_DECLS

#define EV_TYPE_BOOKMARKS         (ev_bookmarks_get_type())
#define EV_BOOKMARKS(object)      (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_BOOKMARKS, EvBookmarks))
#define EV_BOOKMARKS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_BOOKMARKS, EvBookmarksClass))
#define EV_IS_BOOKMARKS(object)   (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_BOOKMARKS))

typedef struct _EvBookmarks      EvBookmarks;
typedef struct _EvBookmarksClass EvBookmarksClass;

typedef struct _EvBookmark {
        guint  page;
        gchar *title;
} EvBookmark;

GType        ev_bookmarks_get_type      (void) G_GNUC_CONST;
EvBookmarks *ev_bookmarks_new           (EvMetadata *metadata);
GList       *ev_bookmarks_get_bookmarks (EvBookmarks *bookmarks);
gboolean     ev_bookmarks_has_bookmarks (EvBookmarks *bookmarks);
void         ev_bookmarks_add           (EvBookmarks *bookmarks,
                                         EvBookmark  *bookmark);
void         ev_bookmarks_delete        (EvBookmarks *bookmarks,
                                         EvBookmark  *bookmark);
void         ev_bookmarks_update        (EvBookmarks *bookmarks,
                                         EvBookmark  *bookmark);

G_END_DECLS
