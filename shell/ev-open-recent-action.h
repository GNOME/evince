/*
 *  Copyright (C) 2007, Carlos Garcia Campos  <carlosgc@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA..
 *
 */

#ifndef EV_OPEN_RECENT_ACTION_H
#define EV_OPEN_RECENT_ACTION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EV_TYPE_OPEN_RECENT_ACTION            (ev_open_recent_action_get_type ())
#define EV_OPEN_RECENT_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_OPEN_RECENT_ACTION, EvOpenRecentAction))
#define EV_OPEN_RECENT_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_OPEN_RECENT_ACTION, EvOpenRecentActionClass))
#define EV_IS_OPEN_RECENT_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_OPEN_RECENT_ACTION))
#define EV_IS_OPEN_RECENT_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_OPEN_RECENT_ACTION))
#define EV_OPEN_RECENT_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_OPEN_RECENT_ACTION, EvOpenRecentActionClass))

typedef struct _EvOpenRecentAction      EvOpenRecentAction;
typedef struct _EvOpenRecentActionClass EvOpenRecentActionClass;

struct _EvOpenRecentAction {
	GtkAction parent;
};

struct _EvOpenRecentActionClass {
	GtkActionClass parent_class;

	void (* item_activated) (EvOpenRecentAction *action,
				 const gchar        *uri);
};

GType ev_open_recent_action_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* EV_OPEN_RECENT_ACTION_H */
