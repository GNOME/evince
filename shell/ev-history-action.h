/* ev-history-action.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2013 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#ifndef EV_HISTORY_ACTION_H
#define EV_HISTORY_ACTION_H

#include <gtk/gtk.h>

#include "ev-history.h"

G_BEGIN_DECLS

#define EV_TYPE_HISTORY_ACTION            (ev_history_action_get_type ())
#define EV_HISTORY_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_HISTORY_ACTION, EvHistoryAction))
#define EV_IS_HISTORY_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_HISTORY_ACTION))
#define EV_HISTORY_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_HISTORY_ACTION, EvHistoryActionClass))
#define EV_IS_HISTORY_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_HISTORY_ACTION))
#define EV_HISTORY_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_HISTORY_ACTION, EvHistoryActionClass))

typedef struct _EvHistoryAction        EvHistoryAction;
typedef struct _EvHistoryActionClass   EvHistoryActionClass;
typedef struct _EvHistoryActionPrivate EvHistoryActionPrivate;

struct _EvHistoryAction {
        GtkBox parent;

        EvHistoryActionPrivate *priv;
};

struct _EvHistoryActionClass {
        GtkBoxClass parent_class;
};

GType      ev_history_action_get_type        (void);

GtkWidget *ev_history_action_new             (EvHistory       *history);
gboolean   ev_history_action_get_popup_shown (EvHistoryAction *action);

G_END_DECLS

#endif
