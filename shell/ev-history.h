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

#pragma once

#include <glib-object.h>
#include <evince-document.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_HISTORY            (ev_history_get_type ())
#define EV_HISTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_HISTORY, EvHistory))
#define EV_HISTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_HISTORY, EvHistoryClass))
#define EV_IS_HISTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_HISTORY))
#define EV_IS_HISTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_HISTORY))
#define EV_HISTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_HISTORY, EvHistoryClass))

typedef struct _EvHistory		EvHistory;
typedef struct _EvHistoryClass		EvHistoryClass;

struct _EvHistory
{
	GObject parent;
};

struct _EvHistoryClass
{
	GObjectClass parent_class;

	void (* changed)       (EvHistory *history);
        void (* activate_link) (EvHistory *history,
                                EvLink    *link);
};

GType           ev_history_get_type         (void);
EvHistory      *ev_history_new              (EvDocumentModel *model);
void            ev_history_add_link         (EvHistory       *history,
                                             EvLink          *link);
void            ev_history_add_page         (EvHistory       *history,
                                             gint            page);
gboolean        ev_history_can_go_back      (EvHistory       *history);
void            ev_history_go_back          (EvHistory       *history);
gboolean        ev_history_can_go_forward   (EvHistory       *history);
void            ev_history_go_forward       (EvHistory       *history);
gboolean        ev_history_go_to_link       (EvHistory       *history,
                                             EvLink          *link);
GList          *ev_history_get_back_list    (EvHistory       *history);
GList          *ev_history_get_forward_list (EvHistory       *history);

void            ev_history_freeze           (EvHistory       *history);
void            ev_history_thaw             (EvHistory       *history);
gboolean        ev_history_is_frozen        (EvHistory       *history);

G_END_DECLS
