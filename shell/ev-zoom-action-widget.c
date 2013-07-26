/* ev-zoom-action-widget.c
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

#include "ev-zoom-action-widget.h"
#include "ev-zoom-action.h"

#include <glib/gi18n.h>

enum {
        PROP_0,
        PROP_POPUP_SHOWN
};

struct _EvZoomActionWidgetPrivate {
        GtkWidget       *entry;

        EvDocumentModel *model;
        EvWindow        *window;
        GtkWidget       *popup;
        gboolean         popup_shown;
};

G_DEFINE_TYPE (EvZoomActionWidget, ev_zoom_action_widget, GTK_TYPE_TOOL_ITEM)

#define EPSILON 0.000001

static void
ev_zoom_action_widget_set_zoom_level (EvZoomActionWidget *control,
                                      float               zoom)
{
        gchar *zoom_str;
        float  zoom_perc;
        guint  i;

        for (i = 3; i < G_N_ELEMENTS (zoom_levels); i++) {
                if (ABS (zoom - zoom_levels[i].level) < EPSILON) {
                        gtk_entry_set_text (GTK_ENTRY (control->priv->entry),
                                            zoom_levels[i].name);
                        return;
                }
        }

        zoom_perc = zoom * 100.;
        if (ABS ((gint)zoom_perc - zoom_perc) < 0.001)
                zoom_str = g_strdup_printf ("%d%%", (gint)zoom_perc);
        else
                zoom_str = g_strdup_printf ("%.2f%%", zoom_perc);
        gtk_entry_set_text (GTK_ENTRY (control->priv->entry), zoom_str);
        g_free (zoom_str);
}

static void
ev_zoom_action_widget_update_zoom_level (EvZoomActionWidget *control)
{
        float      zoom = ev_document_model_get_scale (control->priv->model);
        GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (control));

        zoom *= 72.0 / ev_document_misc_get_screen_dpi (screen);
        ev_zoom_action_widget_set_zoom_level (control, zoom);
}

static void
zoom_changed_cb (EvDocumentModel    *model,
                 GParamSpec         *pspec,
                 EvZoomActionWidget *control)
{
        ev_zoom_action_widget_update_zoom_level (control);
}

static void
document_changed_cb (EvDocumentModel    *model,
                     GParamSpec         *pspec,
                     EvZoomActionWidget *control)
{
        if (!ev_document_model_get_document (model))
                return;

        ev_zoom_action_widget_update_zoom_level (control);
}

static void
entry_activated_cb (GtkEntry           *entry,
                    EvZoomActionWidget *control)
{
        GdkScreen   *screen;
        double       zoom_perc;
        float        zoom;
        const gchar *text = gtk_entry_get_text (entry);
        gchar       *end_ptr = NULL;

        if (!text || text[0] == '\0') {
                ev_zoom_action_widget_update_zoom_level (control);
                return;
        }

        zoom_perc = g_strtod (text, &end_ptr);
        if (end_ptr && end_ptr[0] != '\0' && end_ptr[0] != '%') {
                ev_zoom_action_widget_update_zoom_level (control);
                return;
        }

        screen = gtk_widget_get_screen (GTK_WIDGET (control));
        zoom = zoom_perc / 100.;
        ev_document_model_set_sizing_mode (control->priv->model, EV_SIZING_FREE);
        ev_document_model_set_scale (control->priv->model,
                                     zoom * ev_document_misc_get_screen_dpi (screen) / 72.0);
}

static gboolean
focus_out_cb (EvZoomActionWidget *control)
{
        ev_zoom_action_widget_update_zoom_level (control);

        return FALSE;
}

static gint
get_max_zoom_level_label (EvZoomActionWidget *control)
{
        GList *actions, *l;
        gint   width = 0;

        actions = gtk_action_group_list_actions (ev_window_get_zoom_selector_action_group (control->priv->window));

        for (l = actions; l; l = g_list_next (l)) {
                GtkAction *action = (GtkAction *)l->data;
                int        length;

                length = g_utf8_strlen (gtk_action_get_label (action), -1);
                if (length > width)
                        width = length;
        }

        g_list_free (actions);

        /* Count the toggle size as one character more */
        return width + 1;
}

static void
popup_menu_show_cb (GtkWidget          *widget,
                    EvZoomActionWidget *control)
{
        control->priv->popup_shown = TRUE;
        g_object_notify (G_OBJECT (control), "popup-shown");
}

static void
popup_menu_hide_cb (GtkWidget          *widget,
                    EvZoomActionWidget *control)
{
        control->priv->popup_shown = FALSE;
        g_object_notify (G_OBJECT (control), "popup-shown");
}

static void
popup_menu_detached (EvZoomActionWidget *control,
                     GtkWidget          *popup)
{
        GtkWidget *toplevel;

        if (control->priv->popup != popup)
                return;

        toplevel = gtk_widget_get_toplevel (control->priv->popup);
        g_signal_handlers_disconnect_by_func (toplevel,
                                              popup_menu_show_cb,
                                              control);
        g_signal_handlers_disconnect_by_func (toplevel,
                                              popup_menu_hide_cb,
                                              control);

        control->priv->popup = NULL;
}

static GtkWidget *
get_popup (EvZoomActionWidget *control)
{
        GtkUIManager *ui_manager;
        GtkWidget    *toplevel;

        if (control->priv->popup)
                return control->priv->popup;

        ui_manager = ev_window_get_ui_manager (control->priv->window);
        control->priv->popup = gtk_ui_manager_get_widget (ui_manager, "/ZoomSelectorPopup");
        gtk_menu_attach_to_widget (GTK_MENU (control->priv->popup), GTK_WIDGET (control),
                                   (GtkMenuDetachFunc)popup_menu_detached);
        toplevel = gtk_widget_get_toplevel (control->priv->popup);
        g_signal_connect (toplevel, "show",
                          G_CALLBACK (popup_menu_show_cb),
                          control);
        g_signal_connect (toplevel, "hide",
                          G_CALLBACK (popup_menu_hide_cb),
                          control);

        return control->priv->popup;
}

static void
menu_position_below (GtkMenu  *menu,
                     gint     *x,
                     gint     *y,
                     gint     *push_in,
                     gpointer  user_data)
{
        EvZoomActionWidget *control;
        GtkWidget          *widget;
        GtkAllocation       child_allocation;
        GtkRequisition      req;
        GdkScreen          *screen;
        gint                monitor_num;
        GdkRectangle        monitor;
        gint                sx = 0, sy = 0;

        control = EV_ZOOM_ACTION_WIDGET (user_data);
        widget = GTK_WIDGET (control);

        gtk_widget_get_allocation (control->priv->entry, &child_allocation);

        if (!gtk_widget_get_has_window (control->priv->entry)) {
                sx += child_allocation.x;
                sy += child_allocation.y;
        }

        gdk_window_get_root_coords (gtk_widget_get_window (control->priv->entry),
                                    sx, sy, &sx, &sy);

        gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);

        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
                *x = sx;
        else
                *x = sx + child_allocation.width - req.width;
        *y = sy;

        screen = gtk_widget_get_screen (widget);
        monitor_num = gdk_screen_get_monitor_at_window (screen,
                                                        gtk_widget_get_window (widget));
        gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

        if (*x < monitor.x)
                *x = monitor.x;
        else if (*x + req.width > monitor.x + monitor.width)
                *x = monitor.x + monitor.width - req.width;

        if (monitor.y + monitor.height - *y - child_allocation.height >= req.height)
                *y += child_allocation.height;
        else if (*y - monitor.y >= req.height)
                *y -= req.height;
        else if (monitor.y + monitor.height - *y - child_allocation.height > *y - monitor.y)
                *y += child_allocation.height;
        else
                *y -= req.height;

        *push_in = FALSE;
}

static void
entry_icon_press_callback (GtkEntry            *entry,
                           GtkEntryIconPosition icon_pos,
                           GdkEventButton      *event,
                           EvZoomActionWidget  *control)
{
        GtkWidget *menu;

        if (!control->priv->window || event->button != GDK_BUTTON_PRIMARY)
                return;

        menu = get_popup (control);
        gtk_widget_set_size_request (menu,
                                     gtk_widget_get_allocated_width (GTK_WIDGET (control)),
                                     -1);

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                        menu_position_below, control,
                        event->button, event->time);
}

static gboolean
setup_initial_entry_size (EvZoomActionWidget *control)
{
        gtk_entry_set_width_chars (GTK_ENTRY (control->priv->entry),
                                   get_max_zoom_level_label (control));
        return FALSE;
}

static void
ev_zoom_action_widget_init (EvZoomActionWidget *control)
{
        EvZoomActionWidgetPrivate *priv;
        GtkWidget                 *vbox;

        control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control, EV_TYPE_ZOOM_ACTION_WIDGET, EvZoomActionWidgetPrivate);
        priv = control->priv;

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

        priv->entry = gtk_entry_new ();
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "go-down-symbolic");
        gtk_box_pack_start (GTK_BOX (vbox), priv->entry, TRUE, FALSE, 0);
        gtk_widget_show (priv->entry);

        gtk_container_add (GTK_CONTAINER (control), vbox);
        gtk_widget_show (vbox);

        g_signal_connect (priv->entry, "icon-press",
                          G_CALLBACK (entry_icon_press_callback),
                          control);
        g_signal_connect (priv->entry, "activate",
                          G_CALLBACK (entry_activated_cb),
                          control);
        g_signal_connect_swapped (priv->entry, "focus-out-event",
                                  G_CALLBACK (focus_out_cb),
                                  control);

        g_idle_add ((GSourceFunc)setup_initial_entry_size, control);
}

static void
ev_zoom_action_widget_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        EvZoomActionWidget *control = EV_ZOOM_ACTION_WIDGET (object);

        switch (prop_id) {
        case PROP_POPUP_SHOWN:
                g_value_set_boolean (value, control->priv->popup_shown);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
         }
}

static void
ev_zoom_action_widget_class_init (EvZoomActionWidgetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = ev_zoom_action_widget_get_property;

        g_object_class_install_property (object_class,
                                         PROP_POPUP_SHOWN,
                                         g_param_spec_boolean ("popup-shown",
                                                               "Popup shown",
                                                               "Whether the zoom control's dropdown is shown",
                                                               FALSE,
                                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

        g_type_class_add_private (object_class, sizeof (EvZoomActionWidgetPrivate));
}

void
ev_zoom_action_widget_set_model (EvZoomActionWidget *control,
                                 EvDocumentModel    *model)
{
        g_return_if_fail (EV_IS_ZOOM_ACTION_WIDGET (control));

        if (control->priv->model == model)
                return;

        if (control->priv->model) {
                g_object_remove_weak_pointer (G_OBJECT (control->priv->model),
                                              (gpointer)&control->priv->model);
                g_signal_handlers_disconnect_by_func (control->priv->model,
                                                      G_CALLBACK (zoom_changed_cb),
                                                      control);
        }
        control->priv->model = model;
        if (!model)
                return;

        g_object_add_weak_pointer (G_OBJECT (model),
                                   (gpointer)&control->priv->model);

        if (ev_document_model_get_document (model)) {
                ev_zoom_action_widget_update_zoom_level (control);
        } else {
                ev_zoom_action_widget_set_zoom_level (control, 1.);
        }

        g_signal_connect (control->priv->model, "notify::document",
                          G_CALLBACK (document_changed_cb),
                          control);
        g_signal_connect (control->priv->model, "notify::scale",
                          G_CALLBACK (zoom_changed_cb),
                          control);
}

void
ev_zoom_action_widget_set_window (EvZoomActionWidget *control,
                                  EvWindow           *window)
{
        if (control->priv->window == window)
                return;

        control->priv->window = window;
}

GtkWidget *
ev_zoom_action_widget_get_entry (EvZoomActionWidget *control)
{
        g_return_val_if_fail (EV_IS_ZOOM_ACTION_WIDGET (control), NULL);

        return control->priv->entry;
}
