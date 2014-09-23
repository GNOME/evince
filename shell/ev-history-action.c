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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ev-history-action.h"

#include <glib/gi18n.h>

enum {
        PROP_0,

        PROP_HISTORY
};

typedef enum {
        EV_HISTORY_ACTION_BUTTON_BACK,
        EV_HISTORY_ACTION_BUTTON_FORWARD
} EvHistoryActionButton;

struct _EvHistoryActionPrivate {
        GtkWidget *back_button;
        GtkWidget *forward_button;

        EvHistory *history;
        gboolean   popup_shown;
};

G_DEFINE_TYPE (EvHistoryAction, ev_history_action, GTK_TYPE_BOX)

static void
history_menu_link_activated (GtkMenuItem     *item,
                             EvHistoryAction *history_action)
{
        EvLink *link;

        link = EV_LINK (g_object_get_data (G_OBJECT (item), "ev-history-menu-item-link"));
        if (!link)
                return;

        ev_history_go_to_link (history_action->priv->history, link);
}

static void
popup_menu_hide_cb (GtkMenu         *menu,
                    EvHistoryAction *history_action)
{
        history_action->priv->popup_shown = FALSE;
}

static void
ev_history_action_show_popup (EvHistoryAction       *history_action,
                              EvHistoryActionButton  action_button,
                              guint                  button,
                              guint32                event_time)
{
        GtkWidget *menu;
        GList     *list = NULL;
        GList     *l;

        switch (action_button) {
        case EV_HISTORY_ACTION_BUTTON_BACK:
                list = ev_history_get_back_list (history_action->priv->history);
                break;
        case EV_HISTORY_ACTION_BUTTON_FORWARD:
                list = ev_history_get_forward_list (history_action->priv->history);
                break;
        }

        if (!list)
                return;

        menu = gtk_menu_new ();

        for (l = list; l; l = g_list_next (l)) {
                EvLink    *link = EV_LINK (l->data);
                GtkWidget *item;

                item = gtk_menu_item_new_with_label (ev_link_get_title (link));
                g_object_set_data_full (G_OBJECT (item), "ev-history-menu-item-link",
                                        g_object_ref (link), (GDestroyNotify)g_object_unref);
                g_signal_connect_object (item, "activate",
                                         G_CALLBACK (history_menu_link_activated),
                                         history_action, 0);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
                gtk_widget_show (item);
        }
        g_list_free (list);

        history_action->priv->popup_shown = TRUE;
        g_signal_connect (menu, "hide",
                          G_CALLBACK (popup_menu_hide_cb),
                          history_action);
        gtk_menu_popup (GTK_MENU (menu),
                        NULL, NULL, NULL, NULL,
                        button, event_time);
}

static void
ev_history_action_finalize (GObject *object)
{
        EvHistoryAction *history_action = EV_HISTORY_ACTION (object);

        if (history_action->priv->history) {
                g_object_remove_weak_pointer (G_OBJECT (history_action->priv->history),
                                              (gpointer)&history_action->priv->history);
        }

        G_OBJECT_CLASS (ev_history_action_parent_class)->finalize (object);
}

static void
ev_history_action_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        EvHistoryAction *history_action = EV_HISTORY_ACTION (object);

        switch (prop_id) {
        case PROP_HISTORY:
                history_action->priv->history = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
ev_history_action_constructed (GObject *object)
{
        EvHistoryAction *history_action = EV_HISTORY_ACTION (object);

        G_OBJECT_CLASS (ev_history_action_parent_class)->constructed (object);

        g_object_add_weak_pointer (G_OBJECT (history_action->priv->history),
                                   (gpointer)&history_action->priv->history);
}

static void
ev_history_action_class_init (EvHistoryActionClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        object_class->constructed = ev_history_action_constructed;
        object_class->finalize = ev_history_action_finalize;
        object_class->set_property = ev_history_action_set_property;

        g_object_class_install_property (object_class,
                                         PROP_HISTORY,
                                         g_param_spec_object ("history",
                                                              "History",
                                                              "The History",
                                                              EV_TYPE_HISTORY,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));

        g_type_class_add_private (object_class, sizeof (EvHistoryActionPrivate));
}

static gboolean
button_pressed (GtkWidget       *button,
                GdkEventButton  *event,
                EvHistoryAction *history_action)
{
        EvHistoryActionPrivate *priv = history_action->priv;

        /* TODO: Show the popup menu after a long press too */
        switch (event->button) {
        case GDK_BUTTON_SECONDARY:
                ev_history_action_show_popup (history_action,
                                              button == priv->back_button ?
                                              EV_HISTORY_ACTION_BUTTON_BACK :
                                              EV_HISTORY_ACTION_BUTTON_FORWARD,
                                              event->button, event->time);
                return GDK_EVENT_STOP;
        default:
                break;
        }

        return GDK_EVENT_PROPAGATE;
}

static GtkWidget *
ev_history_action_create_button (EvHistoryAction       *history_action,
                                 EvHistoryActionButton  action_button)
{
        GtkWidget   *button;
        GtkWidget   *image;
        const gchar *icon_name = NULL;
        const gchar *tooltip_text = NULL;
        const gchar *action_name = NULL;

        button = gtk_button_new ();
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        g_signal_connect (button, "button-press-event",
                          G_CALLBACK (button_pressed),
                          history_action);

        switch (action_button) {
        case EV_HISTORY_ACTION_BUTTON_BACK:
                icon_name = "go-previous-symbolic";
                tooltip_text = _("Go to previous history item");
                action_name = "win.go-back-history";
                break;
        case EV_HISTORY_ACTION_BUTTON_FORWARD:
                icon_name = "go-next-symbolic";
                tooltip_text = _("Go to next history item");
                action_name = "win.go-forward-history";
                break;
        }

        image = gtk_image_new ();
        gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action_name);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_MENU);
        gtk_widget_set_tooltip_text (button, tooltip_text);
        gtk_widget_set_can_focus (button, FALSE);

        return button;
}

static void
ev_history_action_init (EvHistoryAction *history_action)
{
        GtkWidget              *box = GTK_WIDGET (history_action);
        GtkStyleContext        *style_context;
        EvHistoryActionPrivate *priv;

        history_action->priv = G_TYPE_INSTANCE_GET_PRIVATE (history_action, EV_TYPE_HISTORY_ACTION, EvHistoryActionPrivate);
        priv = history_action->priv;

        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
        style_context = gtk_widget_get_style_context (box);
        gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_RAISED);
        gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_LINKED);

        priv->back_button = ev_history_action_create_button (history_action,
                                                             EV_HISTORY_ACTION_BUTTON_BACK);
        gtk_container_add (GTK_CONTAINER (box), priv->back_button);
        gtk_widget_show (priv->back_button);

        priv->forward_button = ev_history_action_create_button (history_action,
                                                                EV_HISTORY_ACTION_BUTTON_FORWARD);
        gtk_container_add (GTK_CONTAINER (box), priv->forward_button);
        gtk_widget_show (priv->forward_button);
}

GtkWidget *
ev_history_action_new (EvHistory *history)
{
        g_return_val_if_fail (EV_IS_HISTORY (history), NULL);

        return GTK_WIDGET (g_object_new (EV_TYPE_HISTORY_ACTION, "history", history, NULL));
}

gboolean
ev_history_action_get_popup_shown (EvHistoryAction *action)
{
        g_return_val_if_fail (EV_IS_HISTORY_ACTION (action), FALSE);

        return action->priv->popup_shown;
}
