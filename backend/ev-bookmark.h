/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc.
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

#ifndef EV_BOOKMARK_H
#define EV_BOOKMARK_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvBookmark EvBookmark;
typedef struct _EvBookmarkClass EvBookmarkClass;
typedef struct _EvBookmarkPrivate EvBookmarkPrivate;

#define EV_TYPE_BOOKMARK		(ev_bookmark_get_type())
#define EV_BOOKMARK(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_BOOKMARK, EvBookmark))
#define EV_BOOKMARK_CLASS(klass)	(G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_BOOKMARK, EvBookmarkClass))
#define EV_IS_BOOKMARK(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_BOOKMARK))
#define EV_IS_BOOKMARK_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_BOOKMARK))
#define EV_BOOKMARK_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_BOOKMARK, EvBookmarkClass))

#define EV_TYPE_BOOKMARK_TYPE (ev_bookmark_type_get_type ())

typedef enum
{
	EV_BOOKMARK_TYPE_TITLE,
	EV_BOOKMARK_TYPE_LINK,
	EV_BOOKMARK_TYPE_EXTERNAL_URI
} EvBookmarkType;

struct _EvBookmark {
	GObject base_instance;
	EvBookmarkPrivate *priv;
};

struct _EvBookmarkClass {
	GObjectClass base_class;
};

GType           ev_bookmark_type_get_type       (void);
GType		ev_bookmark_get_type		(void);

EvBookmark     *ev_bookmark_new_title		(const char     *title);
EvBookmark     *ev_bookmark_new_link		(const char     *title,
						 int             page);
EvBookmark     *ev_bookmark_new_external	(const char     *title,
						 const char     *uri);
const char     *ev_bookmark_get_title		(EvBookmark     *bookmark);
void		ev_bookmark_set_title		(EvBookmark     *bookmark,
					 	 const char     *title);
const char     *ev_bookmark_get_uri		(EvBookmark     *bookmark);
void		ev_bookmark_set_uri		(EvBookmark     *bookmark,
					 	 const char     *uri);
EvBookmarkType  ev_bookmark_get_bookmark_type	(EvBookmark     *bookmark);
void		ev_bookmark_set_bookmark_type	(EvBookmark     *bookmark,
						 EvBookmarkType  type);
int		ev_bookmark_get_page		(EvBookmark     *bookmark);
void		ev_bookmark_set_page		(EvBookmark     *bookmark,
					 	 int             page);

G_END_DECLS

#endif /* !EV_BOOKMARK_H */
