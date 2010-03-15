/*
 *  Copyright (C) 2005 Marco Pesenti Gritti
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

#ifndef EV_HISTORY_H
#define EV_HISTORY_H

#include <glib-object.h>

#include "ev-link.h"

G_BEGIN_DECLS

#define EV_TYPE_HISTORY            (ev_history_get_type ())
#define EV_HISTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_HISTORY, EvHistory))
#define EV_HISTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_HISTORY, EvHistoryClass))
#define EV_IS_HISTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_HISTORY))
#define EV_IS_HISTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_HISTORY))
#define EV_HISTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_HISTORY, EvHistoryClass))

typedef struct _EvHistory		EvHistory;
typedef struct _EvHistoryPrivate	EvHistoryPrivate;
typedef struct _EvHistoryClass		EvHistoryClass;

struct _EvHistory
{
	GObject parent;
	
	/*< private >*/
	EvHistoryPrivate *priv;
};

struct _EvHistoryClass
{
	GObjectClass parent_class;
	
	void (*changed) (EvHistory *history);
};

GType		ev_history_get_type		(void);
EvHistory      *ev_history_new			(void);
void		ev_history_add_link		(EvHistory  *history,
						 EvLink     *linkk);
EvLink	       *ev_history_get_link_nth		(EvHistory  *history,
					 	 int         index);
int		ev_history_get_n_links		(EvHistory  *history);

G_END_DECLS

#endif
