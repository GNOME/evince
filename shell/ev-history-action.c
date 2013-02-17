/* ev-history-action.c
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "ev-history-action.h"
#include "ev-history-action-widget.h"

enum {
        ACTIVATED,
        LAST_SIGNAL
};


struct _EvHistoryActionPrivate {
        EvHistory *history;
        gboolean   popup_shown;
};

G_DEFINE_TYPE (EvHistoryAction, ev_history_action, GTK_TYPE_ACTION)

static guint signals[LAST_SIGNAL] = { 0 };

static void
popup_shown_cb (GObject         *history_widget,
                GParamSpec      *pspec,
                EvHistoryAction *history_action)
{
        g_object_get (history_widget, "popup-shown", &history_action->priv->popup_shown, NULL);
}

static void
connect_proxy (GtkAction *action,
               GtkWidget *proxy)
{
        if (EV_IS_HISTORY_ACTION_WIDGET (proxy))   {
                EvHistoryAction       *history_action = EV_HISTORY_ACTION (action);
                EvHistoryActionWidget *history_widget = EV_HISTORY_ACTION_WIDGET (proxy);

                ev_history_action_widget_set_history (history_widget, history_action->priv->history);
                g_signal_connect (history_widget, "notify::popup-shown",
                                  G_CALLBACK (popup_shown_cb),
                                  action);
        }

        GTK_ACTION_CLASS (ev_history_action_parent_class)->connect_proxy (action, proxy);
}

static void
ev_history_action_class_init (EvHistoryActionClass *class)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (class);
        GtkActionClass *action_class = GTK_ACTION_CLASS (class);

        action_class->toolbar_item_type = EV_TYPE_HISTORY_ACTION_WIDGET;
        action_class->connect_proxy = connect_proxy;

        signals[ACTIVATED] =
                g_signal_new ("activated",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_type_class_add_private (object_class, sizeof (EvHistoryActionPrivate));
}

static void
ev_history_action_init (EvHistoryAction *action)
{
        action->priv = G_TYPE_INSTANCE_GET_PRIVATE (action, EV_TYPE_HISTORY_ACTION, EvHistoryActionPrivate);
}

void
ev_history_action_set_history (EvHistoryAction *action,
                               EvHistory       *history)
{
        GSList *proxies, *l;

        g_return_if_fail (EV_IS_HISTORY_ACTION (action));
        g_return_if_fail (EV_IS_HISTORY (history));

        if (action->priv->history == history)
                return;

        action->priv->history = history;
        proxies = gtk_action_get_proxies (GTK_ACTION (action));
        for (l = proxies; l && l->data; l = g_slist_next (l)) {
                if (EV_IS_HISTORY_ACTION_WIDGET (l->data))
                        ev_history_action_widget_set_history (EV_HISTORY_ACTION_WIDGET (l->data), history);
        }
}

gboolean
ev_history_action_get_popup_shown (EvHistoryAction *action)
{
        g_return_val_if_fail (EV_IS_HISTORY_ACTION (action), FALSE);

        return action->priv->popup_shown;
}

