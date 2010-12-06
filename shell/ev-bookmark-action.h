/* ev-bookmark-action.h
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

#ifndef EV_BOOKMARK_ACTION_H
#define EV_BOOKMARK_ACTION_H

#include <gtk/gtk.h>
#include <glib-object.h>

#include "ev-bookmarks.h"

G_BEGIN_DECLS

#define EV_TYPE_BOOKMARK_ACTION         (ev_bookmark_action_get_type())
#define EV_BOOKMARK_ACTION(object)      (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_BOOKMARK_ACTION, EvBookmarkAction))
#define EV_BOOKMARK_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_BOOKMARK_ACTION, EvBookmarkActionClass))
#define EV_IS_BOOKMARK_ACTION(object)   (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_BOOKMARK_ACTION))

typedef struct _EvBookmarkAction      EvBookmarkAction;
typedef struct _EvBookmarkActionClass EvBookmarkActionClass;

GType      ev_bookmark_action_get_type (void) G_GNUC_CONST;
GtkAction *ev_bookmark_action_new      (EvBookmark       *bookmark);
guint      ev_bookmark_action_get_page (EvBookmarkAction *action);

G_END_DECLS

#endif /* EV_BOOKMARK_ACTION_H */
