/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *
 *  Author:
 *    Martin Kretzschmar <martink@gnome.org>
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

#ifndef EV_WINDOW_H
#define EV_WINDOW_H

#include <glib-object.h>
#include <gtk/gtkwindow.h>

#include "ev-bookmark.h"

G_BEGIN_DECLS

typedef struct _EvWindow EvWindow;
typedef struct _EvWindowClass EvWindowClass;
typedef struct _EvWindowPrivate EvWindowPrivate;

#define EV_TYPE_WINDOW			(ev_window_get_type())
#define EV_WINDOW(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_WINDOW, EvWindow))
#define EV_WINDOW_CLASS(klass)		(G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_WINDOW, EvWindowClass))
#define EV_IS_WINDOW(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_WINDOW))
#define EV_IS_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_WINDOW))
#define EV_WINDOW_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_WINDOW, EvWindowClass))

struct _EvWindow {
	GtkWindow		base_instance;
	EvWindowPrivate		*priv;
};

struct _EvWindowClass {
	GtkWindowClass		base_class;

	/* signals */
	void (*signal)		(EvWindow	*self,
				 const char	*string);
};

GType		ev_window_get_type		(void);
void		ev_window_open			(EvWindow *ev_window, const char *uri);
void		ev_window_open_bookmark		(EvWindow   *ev_window,
						 EvBookmark *bookmark);
gboolean	ev_window_is_empty		(const EvWindow *ev_window);

G_END_DECLS

#endif /* !EV_WINDOW_H */

