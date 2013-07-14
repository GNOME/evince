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

#include <glib/gi18n.h>

struct _EvZoomActionWidgetPrivate {
        GtkWidget       *combo;

        EvDocumentModel *model;
        gulong           combo_changed_handler;
};

#define EV_ZOOM_FIT_PAGE  (-3.0)
#define EV_ZOOM_FIT_WIDTH (-4.0)
#define EV_ZOOM_SEPARATOR (-5.0)

static const struct {
        const gchar *name;
        float        level;
} zoom_levels[] = {
        { N_("Fit Page"),  EV_ZOOM_FIT_PAGE  },
        { N_("Fit Width"), EV_ZOOM_FIT_WIDTH },
        { NULL,            EV_ZOOM_SEPARATOR },
        { N_("50%"), 0.5 },
        { N_("70%"), 0.7071067811 },
        { N_("85%"), 0.8408964152 },
        { N_("100%"), 1.0 },
        { N_("125%"), 1.1892071149 },
        { N_("150%"), 1.4142135623 },
        { N_("175%"), 1.6817928304 },
        { N_("200%"), 2.0 },
        { N_("300%"), 2.8284271247 },
        { N_("400%"), 4.0 },
        { N_("800%"), 8.0 },
        { N_("1600%"), 16.0 },
        { N_("3200%"), 32.0 },
        { N_("6400%"), 64.0 }
};

enum {
        TEXT_COLUMN,
        IS_SEPARATOR_COLUMN,

        N_COLUMNS
};

G_DEFINE_TYPE (EvZoomActionWidget, ev_zoom_action_widget, GTK_TYPE_TOOL_ITEM)

#define EPSILON 0.000001

static void
ev_zoom_action_widget_set_zoom_level (EvZoomActionWidget *control,
                                      float               zoom)
{
        GtkWidget *entry = gtk_bin_get_child (GTK_BIN (control->priv->combo));
        gchar     *zoom_str;
        float      zoom_perc;
        guint      i;

        for (i = 3; i < G_N_ELEMENTS (zoom_levels); i++) {
                if (ABS (zoom - zoom_levels[i].level) < EPSILON) {
                        gtk_entry_set_text (GTK_ENTRY (entry), zoom_levels[i].name);
                        return;
                }
        }

        zoom_perc = zoom * 100.;
        if (ABS ((gint)zoom_perc - zoom_perc) < 0.001)
                zoom_str = g_strdup_printf ("%d%%", (gint)zoom_perc);
        else
                zoom_str = g_strdup_printf ("%.2f%%", zoom_perc);
        gtk_entry_set_text (GTK_ENTRY (entry), zoom_str);
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
combo_changed_cb (GtkComboBox        *combo,
                  EvZoomActionWidget *control)
{
        gint         index;
        float        zoom;
        EvSizingMode mode;

        index = gtk_combo_box_get_active (combo);
        if (index == -1)
                return;

        zoom = zoom_levels[index].level;
        if (zoom == EV_ZOOM_FIT_PAGE)
                mode = EV_SIZING_FIT_PAGE;
        else if (zoom == EV_ZOOM_FIT_WIDTH)
                mode = EV_SIZING_FIT_WIDTH;
        else
                mode = EV_SIZING_FREE;

        ev_document_model_set_sizing_mode (control->priv->model, mode);
        if (mode == EV_SIZING_FREE) {
                GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (control));

                ev_document_model_set_scale (control->priv->model,
                                             zoom * ev_document_misc_get_screen_dpi (screen) / 72.0);
        }
}

static void
combo_activated_cb (GtkEntry           *entry,
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
combo_focus_out_cb (EvZoomActionWidget *control)
{
        ev_zoom_action_widget_update_zoom_level (control);

        return FALSE;
}

static gchar *
combo_format_entry_text (GtkComboBox        *combo,
                         const gchar        *path,
                         EvZoomActionWidget *control)
{
        GtkWidget *entry = gtk_bin_get_child (GTK_BIN (combo));

        return g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

static gboolean
row_is_separator (GtkTreeModel *model,
                  GtkTreeIter  *iter,
                  gpointer      data)
{
        gboolean is_sep;

        gtk_tree_model_get (model, iter, IS_SEPARATOR_COLUMN, &is_sep, -1);

        return is_sep;
}

static void
ev_zoom_action_widget_finalize (GObject *object)
{
        EvZoomActionWidget *control = EV_ZOOM_ACTION_WIDGET (object);

        ev_zoom_action_widget_set_model (control, NULL);

        G_OBJECT_CLASS (ev_zoom_action_widget_parent_class)->finalize (object);
}

static void
fill_combo_model (GtkListStore *model,
                  guint         max_level)
{
        guint i;

        for (i = 0; i < max_level; i++) {
                GtkTreeIter iter;

                gtk_list_store_append (model, &iter);
                gtk_list_store_set (model, &iter,
                                    TEXT_COLUMN, _(zoom_levels[i].name),
                                    IS_SEPARATOR_COLUMN, zoom_levels[i].name == NULL,
                                    -1);
        }
}

static gint
get_max_zoom_level_label (void)
{
        gint i;
        gint width = 0;

        for (i = 0; i < G_N_ELEMENTS (zoom_levels); i++) {
                int length;

                length = zoom_levels[i].name ? strlen (_(zoom_levels[i].name)) : 0;
                if (length > width)
                        width = length;
        }

        return width;
}

static void
ev_zoom_action_widget_init (EvZoomActionWidget *control)
{
        EvZoomActionWidgetPrivate *priv;
        GtkWidget                 *entry;
        GtkWidget                 *vbox;
        GtkListStore              *store;

        control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control, EV_TYPE_ZOOM_ACTION_WIDGET, EvZoomActionWidgetPrivate);
        priv = control->priv;

        store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN);
        fill_combo_model (store, G_N_ELEMENTS (zoom_levels));

        priv->combo = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (store));
        gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (priv->combo), FALSE);
        gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->combo), TEXT_COLUMN);
        gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (priv->combo),
                                              (GtkTreeViewRowSeparatorFunc)row_is_separator,
                                              NULL, NULL);
        g_object_unref (store);

        entry = gtk_bin_get_child (GTK_BIN (priv->combo));
        gtk_entry_set_width_chars (GTK_ENTRY (entry), get_max_zoom_level_label ());

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start (GTK_BOX (vbox), priv->combo, TRUE, FALSE, 0);
        gtk_widget_show (priv->combo);

        gtk_container_add (GTK_CONTAINER (control), vbox);
        gtk_widget_show (vbox);

        priv->combo_changed_handler =
                g_signal_connect (priv->combo, "changed",
                                  G_CALLBACK (combo_changed_cb),
                                  control);
        g_signal_connect (priv->combo, "format-entry-text",
                          G_CALLBACK (combo_format_entry_text),
                          control);
        g_signal_connect (entry, "activate",
                          G_CALLBACK (combo_activated_cb),
                          control);
        g_signal_connect_swapped (entry, "focus-out-event",
                                  G_CALLBACK (combo_focus_out_cb),
                                  control);
}

static void
ev_zoom_action_widget_class_init (EvZoomActionWidgetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = ev_zoom_action_widget_finalize;

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
ev_zoom_action_widget_set_max_zoom_level (EvZoomActionWidget *control,
                                          float               max_zoom)
{
        EvZoomActionWidgetPrivate *priv;
        GtkListStore              *model;
        guint                      max_level_index = 3;
        guint                      i;

        g_return_if_fail (EV_IS_ZOOM_ACTION_WIDGET (control));

        priv = control->priv;

        for (i = 3; i < G_N_ELEMENTS (zoom_levels); i++, max_level_index++) {
                if (zoom_levels[i].level > max_zoom)
                        break;
        }

        g_signal_handler_block (priv->combo, priv->combo_changed_handler);

        model = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo)));
        gtk_list_store_clear (model);
        fill_combo_model (model, max_level_index);

        g_signal_handler_unblock (priv->combo, priv->combo_changed_handler);
}

GtkWidget *
ev_zoom_action_widget_get_combo_box (EvZoomActionWidget *control)
{
        g_return_val_if_fail (EV_IS_ZOOM_ACTION_WIDGET (control), NULL);

        return control->priv->combo;
}
