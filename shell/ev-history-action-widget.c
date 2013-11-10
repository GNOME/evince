/* ev-history-action-widget.c
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

#include "ev-history-action-widget.h"

#include <glib/gi18n.h>
#include <math.h>


enum {
        PROP_0,

        PROP_POPUP_SHOWN
};

struct _EvHistoryActionWidgetPrivate {
        GtkWidget *back_button;
        GtkWidget *forward_button;

        EvHistory *history;
        gboolean   popup_shown;
};

typedef enum {
        EV_HISTORY_ACTION_BUTTON_BACK,
        EV_HISTORY_ACTION_BUTTON_FORWARD
} EvHistoryActionButton;

G_DEFINE_TYPE (EvHistoryActionWidget, ev_history_action_widget, GTK_TYPE_TOOL_ITEM)

static void
ev_history_action_widget_finalize (GObject *object)
{
        EvHistoryActionWidget *control = EV_HISTORY_ACTION_WIDGET (object);

        ev_history_action_widget_set_history (control, NULL);

        G_OBJECT_CLASS (ev_history_action_widget_parent_class)->finalize (object);
}

static void
ev_history_action_widget_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
        EvHistoryActionWidget *history_widget = EV_HISTORY_ACTION_WIDGET (object);

        switch (prop_id) {
        case PROP_POPUP_SHOWN:
                g_value_set_boolean (value, history_widget->priv->popup_shown);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
ev_history_action_widget_set_popup_shown (EvHistoryActionWidget *history_widget,
                                          gboolean               popup_shown)
{
        if (history_widget->priv->popup_shown == popup_shown)
                return;

        history_widget->priv->popup_shown = popup_shown;
        g_object_notify (G_OBJECT (history_widget), "popup-shown");
}

static void
history_menu_link_activated (GtkMenuItem           *item,
                             EvHistoryActionWidget *history_widget)
{
        EvLink *link;

        link = EV_LINK (g_object_get_data (G_OBJECT (item), "ev-history-menu-item-link"));
        if (!link)
                return;

        ev_history_go_to_link (history_widget->priv->history, link);
}

static void
popup_menu_hide_cb (GtkMenu               *menu,
                    EvHistoryActionWidget *history_widget)
{
        ev_history_action_widget_set_popup_shown (history_widget, FALSE);
}

static void
ev_history_action_widget_show_popup (EvHistoryActionWidget *history_widget,
                                     EvHistoryActionButton  action_button,
                                     guint                  button,
                                     guint32                event_time)
{
        GtkWidget *menu;
        GList     *list = NULL;
        GList     *l;

        switch (action_button) {
        case EV_HISTORY_ACTION_BUTTON_BACK:
                list = ev_history_get_back_list (history_widget->priv->history);
                break;
        case EV_HISTORY_ACTION_BUTTON_FORWARD:
                list = ev_history_get_forward_list (history_widget->priv->history);
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
                                         history_widget, 0);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
                gtk_widget_show (item);
        }
        g_list_free (list);

        ev_history_action_widget_set_popup_shown (history_widget, TRUE);
        g_signal_connect (menu, "hide",
                          G_CALLBACK (popup_menu_hide_cb),
                          history_widget);
        gtk_menu_popup (GTK_MENU (menu),
                        NULL, NULL, NULL, NULL,
                        button, event_time);
}

static void
button_clicked (GtkWidget             *button,
                EvHistoryActionWidget *history_widget)
{
        EvHistoryActionWidgetPrivate *priv = history_widget->priv;

        if (button == priv->back_button)
                ev_history_go_back (priv->history);
        else if (button == priv->forward_button)
                ev_history_go_forward (priv->history);
}

static gboolean
button_pressed (GtkWidget             *button,
                GdkEventButton        *event,
                EvHistoryActionWidget *history_widget)
{
        EvHistoryActionWidgetPrivate *priv = history_widget->priv;

        /* TODO: Show the popup menu after a long press too */
        switch (event->button) {
        case GDK_BUTTON_SECONDARY:
                ev_history_action_widget_show_popup (history_widget,
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
ev_history_action_widget_create_button (EvHistoryActionWidget *history_widget,
                                        EvHistoryActionButton  action_button)
{
        GtkWidget   *button;
        GtkWidget   *image;
        const gchar *icon_name = NULL;
        const gchar *tooltip_text = NULL;
        gboolean rtl;

        rtl = (gtk_widget_get_direction (GTK_WIDGET (history_widget)) == GTK_TEXT_DIR_RTL);

        button = gtk_button_new ();
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (button_clicked),
                          history_widget);
        g_signal_connect (button, "button-press-event",
                          G_CALLBACK (button_pressed),
                          history_widget);

        switch (action_button) {
        case EV_HISTORY_ACTION_BUTTON_BACK:
                icon_name = rtl ? "go-previous-rtl-symbolic" : "go-previous-symbolic";
                tooltip_text = _("Go to previous history item");
                break;
        case EV_HISTORY_ACTION_BUTTON_FORWARD:
                icon_name = rtl ? "go-next-rtl-symbolic" : "go-next-symbolic";
                tooltip_text = _("Go to next history item");
                break;
        }

        image = gtk_image_new ();
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_MENU);
        gtk_widget_set_tooltip_text (button, tooltip_text);
        gtk_widget_set_can_focus (button, FALSE);

        return button;
}

static void
ev_history_action_widget_init (EvHistoryActionWidget *history_widget)
{
        EvHistoryActionWidgetPrivate *priv;
        GtkWidget                    *box;
        GtkStyleContext              *style_context;

        history_widget->priv = G_TYPE_INSTANCE_GET_PRIVATE (history_widget, EV_TYPE_HISTORY_ACTION_WIDGET, EvHistoryActionWidgetPrivate);
        priv = history_widget->priv;

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        style_context = gtk_widget_get_style_context (box);
        gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_RAISED);
        gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_LINKED);

        priv->back_button = ev_history_action_widget_create_button (history_widget,
                                                                    EV_HISTORY_ACTION_BUTTON_BACK);
        gtk_container_add (GTK_CONTAINER (box), priv->back_button);
        gtk_widget_show (priv->back_button);

        priv->forward_button = ev_history_action_widget_create_button (history_widget,
                                                                       EV_HISTORY_ACTION_BUTTON_FORWARD);
        gtk_container_add (GTK_CONTAINER (box), priv->forward_button);
        gtk_widget_show (priv->forward_button);

        gtk_container_add (GTK_CONTAINER (history_widget), box);
        gtk_widget_show (box);

        gtk_widget_set_sensitive (priv->back_button, FALSE);
        gtk_widget_set_sensitive (priv->forward_button, FALSE);
}

static void
ev_history_action_widget_class_init (EvHistoryActionWidgetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = ev_history_action_widget_get_property;
        object_class->finalize = ev_history_action_widget_finalize;

        g_object_class_install_property (object_class,
                                         PROP_POPUP_SHOWN,
                                         g_param_spec_boolean ("popup-shown",
                                                               "Popup shown",
                                                               "Whether the history's dropdown is shown",
                                                               FALSE,
                                                               G_PARAM_READABLE |
                                                               G_PARAM_STATIC_STRINGS));

        g_type_class_add_private (object_class, sizeof (EvHistoryActionWidgetPrivate));
}

static void
history_changed_cb (EvHistory             *history,
                    EvHistoryActionWidget *history_widget)
{
        EvHistoryActionWidgetPrivate *priv = history_widget->priv;

        gtk_widget_set_sensitive (priv->back_button, ev_history_can_go_back (history));
        gtk_widget_set_sensitive (priv->forward_button, ev_history_can_go_forward (history));
}

void
ev_history_action_widget_set_history (EvHistoryActionWidget *history_widget,
                                      EvHistory             *history)
{
        g_return_if_fail (EV_IS_HISTORY_ACTION_WIDGET (history_widget));

        if (history_widget->priv->history == history)
                return;

        if (history_widget->priv->history) {
                g_object_remove_weak_pointer (G_OBJECT (history_widget->priv->history),
                                              (gpointer)&history_widget->priv->history);
                g_signal_handlers_disconnect_by_func (history_widget->priv->history,
                                                      G_CALLBACK (history_changed_cb),
                                                      history_widget);
        }

        history_widget->priv->history = history;
        if (!history)
                return;

        g_object_add_weak_pointer (G_OBJECT (history),
                                   (gpointer)&history_widget->priv->history);

        g_signal_connect (history, "changed",
                          G_CALLBACK (history_changed_cb),
                          history_widget);
}

