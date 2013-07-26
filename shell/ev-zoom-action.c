/* ev-zoom-action.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include "ev-zoom-action.h"
#include "ev-zoom-action-widget.h"

enum {
        ACTIVATED,
        LAST_SIGNAL
};


struct _EvZoomActionPrivate {
        EvDocumentModel *model;
        EvWindow        *window;
        gboolean         popup_shown;
};

G_DEFINE_TYPE (EvZoomAction, ev_zoom_action, GTK_TYPE_ACTION)

static guint signals[LAST_SIGNAL] = { 0 };

static void
popup_shown_cb (GObject      *zoom_widget,
                GParamSpec   *pspec,
                EvZoomAction *zoom_action)
{
        g_object_get (zoom_widget, "popup-shown", &zoom_action->priv->popup_shown, NULL);
}

static void
zoom_widget_activated_cb (GtkEntry     *entry,
                          EvZoomAction *zoom_action)
{
        g_signal_emit (zoom_action, signals[ACTIVATED], 0, NULL);
}

static void
connect_proxy (GtkAction *action,
               GtkWidget *proxy)
{
        if (EV_IS_ZOOM_ACTION_WIDGET (proxy))   {
                EvZoomAction *zoom_action = EV_ZOOM_ACTION (action);
                EvZoomActionWidget* zoom_widget = EV_ZOOM_ACTION_WIDGET (proxy);

                ev_zoom_action_widget_set_model (zoom_widget, zoom_action->priv->model);
                ev_zoom_action_widget_set_window (zoom_widget, zoom_action->priv->window);
                g_signal_connect (zoom_widget, "notify::popup-shown",
                                  G_CALLBACK (popup_shown_cb),
                                  action);
                g_signal_connect (ev_zoom_action_widget_get_entry (zoom_widget), "activate",
                                  G_CALLBACK (zoom_widget_activated_cb),
                                  action);
        }

        GTK_ACTION_CLASS (ev_zoom_action_parent_class)->connect_proxy (action, proxy);
}

static void
ev_zoom_action_class_init (EvZoomActionClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GtkActionClass *action_class = GTK_ACTION_CLASS (class);

        action_class->toolbar_item_type = EV_TYPE_ZOOM_ACTION_WIDGET;
        action_class->connect_proxy = connect_proxy;

        signals[ACTIVATED] =
                g_signal_new ("activated",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_type_class_add_private (object_class, sizeof (EvZoomActionPrivate));
}

static void
ev_zoom_action_init (EvZoomAction *action)
{
        action->priv = G_TYPE_INSTANCE_GET_PRIVATE (action, EV_TYPE_ZOOM_ACTION, EvZoomActionPrivate);
}

void
ev_zoom_action_set_model (EvZoomAction    *action,
                          EvDocumentModel *model)
{
        GSList *proxies, *l;

        g_return_if_fail (EV_IS_ZOOM_ACTION (action));
        g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

        if (action->priv->model == model)
                return;

        action->priv->model = model;
        proxies = gtk_action_get_proxies (GTK_ACTION (action));
        for (l = proxies; l && l->data; l = g_slist_next (l)) {
                if (EV_IS_ZOOM_ACTION_WIDGET (l->data))
                        ev_zoom_action_widget_set_model (EV_ZOOM_ACTION_WIDGET (l->data), model);
        }
}

void
ev_zoom_action_set_window (EvZoomAction *action,
                           EvWindow     *window)
{

        GSList *proxies, *l;

        g_return_if_fail (EV_IS_ZOOM_ACTION (action));
        g_return_if_fail (EV_IS_WINDOW (window));

        if (action->priv->window == window)
                return;

        action->priv->window = window;
        proxies = gtk_action_get_proxies (GTK_ACTION (action));
        for (l = proxies; l && l->data; l = g_slist_next (l)) {
                if (EV_IS_ZOOM_ACTION_WIDGET (l->data))
                        ev_zoom_action_widget_set_window (EV_ZOOM_ACTION_WIDGET (l->data), window);
        }
}

gboolean
ev_zoom_action_get_popup_shown (EvZoomAction *action)
{
        g_return_val_if_fail (EV_IS_ZOOM_ACTION (action), FALSE);

        return action->priv->popup_shown;
}
