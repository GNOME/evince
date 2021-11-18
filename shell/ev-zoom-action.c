/* ev-zoom-action.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos  <carlosgc@gnome.org>
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
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
#include "ev-zoom-action.h"

#include <glib/gi18n.h>

enum {
        ACTIVATED,
        LAST_SIGNAL
};

enum {
        ZOOM_MODES_SECTION,
        ZOOM_FREE_SECTION
};

static const struct {
        const gchar *name;
        float        level;
} zoom_levels[] = {
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

typedef struct {
        GtkWidget       *entry;

        EvDocumentModel *model;
        GMenu           *menu;

        GMenuModel      *zoom_free_section;
        GtkPopover      *popup;
} EvZoomActionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EvZoomAction, ev_zoom_action, GTK_TYPE_BOX)

#define GET_PRIVATE(o) ev_zoom_action_get_instance_private (o)

static guint signals[LAST_SIGNAL] = { 0 };

#define EPSILON 0.000001

static void
ev_zoom_action_set_zoom_level (EvZoomAction *zoom_action,
                               float         zoom)
{
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);
        gchar *zoom_str;
        float  zoom_perc;
        guint  i;

        for (i = 0; i < G_N_ELEMENTS (zoom_levels); i++) {
                if (ABS (zoom - zoom_levels[i].level) < EPSILON) {
                        gtk_editable_set_text (GTK_EDITABLE (priv->entry),
                                            zoom_levels[i].name);
                        return;
                }
        }

        zoom_perc = zoom * 100.;
        if (ABS ((gint)zoom_perc - zoom_perc) < 0.01)
                zoom_str = g_strdup_printf ("%d%%", (gint)zoom_perc);
        else
                zoom_str = g_strdup_printf ("%.1f%%", zoom_perc);
        gtk_editable_set_text (GTK_EDITABLE (priv->entry), zoom_str);
        g_free (zoom_str);
}

static void
ev_zoom_action_update_zoom_level (EvZoomAction *zoom_action)
{
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);

        float       zoom = ev_document_model_get_scale (priv->model);

        zoom *= 72.0 / ev_document_misc_get_widget_dpi  (GTK_WIDGET (zoom_action));

        ev_zoom_action_set_zoom_level (zoom_action, zoom);
}

static void
zoom_changed_cb (EvDocumentModel *model,
                 GParamSpec      *pspec,
                 EvZoomAction    *zoom_action)
{
        ev_zoom_action_update_zoom_level (zoom_action);
}

static void
document_changed_cb (EvDocumentModel *model,
                     GParamSpec      *pspec,
                     EvZoomAction    *zoom_action)
{
        EvDocument *document = ev_document_model_get_document (model);

        if (!document) {
                gtk_widget_set_sensitive (GTK_WIDGET (zoom_action), FALSE);
                return;
        }
        gtk_widget_set_sensitive (GTK_WIDGET (zoom_action), ev_document_get_n_pages (document) > 0);

        ev_zoom_action_update_zoom_level (zoom_action);
}

static void
ev_zoom_action_set_width_chars (EvZoomAction *zoom_action,
                                gint          width)
{
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);

        /* width + 2 (one decimals and the comma) + 3 (for the icon) */
        gtk_editable_set_width_chars (GTK_EDITABLE (priv->entry), width + 2 + 3);
}

static void
ev_zoom_action_populate_free_zoom_section (EvZoomAction *zoom_action)
{
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);
        gdouble max_scale;
        guint   i;
        gint    width = 0;

        max_scale = ev_document_model_get_max_scale (priv->model);

        for (i = 0; i < G_N_ELEMENTS (zoom_levels); i++) {
                GMenuItem *item;
                gint       length;

                if (zoom_levels[i].level > max_scale)
                        break;

                length = g_utf8_strlen (zoom_levels[i].name, -1);
                if (length > width)
                        width = length;

                item = g_menu_item_new (zoom_levels[i].name, NULL);
                g_menu_item_set_action_and_target (item, "win.zoom",
                                                   "d", zoom_levels[i].level);
                g_menu_append_item (G_MENU (priv->zoom_free_section), item);
                g_object_unref (item);
        }

        ev_zoom_action_set_width_chars (zoom_action, width);
}

static void
max_zoom_changed_cb (EvDocumentModel *model,
                     GParamSpec      *pspec,
                     EvZoomAction    *zoom_action)
{
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);

        g_menu_remove_all (G_MENU (priv->zoom_free_section));
        ev_zoom_action_populate_free_zoom_section (zoom_action);
}


static void
entry_activated_cb (GtkEntry     *entry,
                    EvZoomAction *zoom_action)
{
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);

        double       zoom_perc;
        float        zoom;
        const gchar *text = gtk_editable_get_text (GTK_EDITABLE (entry));
        gchar       *end_ptr = NULL;

        if (!text || text[0] == '\0') {
                ev_zoom_action_update_zoom_level (zoom_action);
                g_signal_emit (zoom_action, signals[ACTIVATED], 0, NULL);
                return;
        }

        zoom_perc = g_strtod (text, &end_ptr);
        if (end_ptr && end_ptr[0] != '\0' && end_ptr[0] != '%') {
                ev_zoom_action_update_zoom_level (zoom_action);
                g_signal_emit (zoom_action, signals[ACTIVATED], 0, NULL);
                return;
        }

        zoom = zoom_perc / 100.;
        ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
        ev_document_model_set_scale (priv->model,
                                     zoom * ev_document_misc_get_widget_dpi (GTK_WIDGET (zoom_action)) / 72.0);
        g_signal_emit (zoom_action, signals[ACTIVATED], 0, NULL);
}

static void
entry_icon_press_cb (GtkEntry            *entry,
		     GtkEntryIconPosition icon_pos,
		     EvZoomAction        *zoom_action)
{
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);
	GdkRectangle rect;

	g_return_if_fail (priv->popup != NULL);

	/* This cannot be done during init, as window does not yet exist
	 * and therefore the rectangle is not yet available */
	if (gtk_popover_get_pointing_to (priv->popup, &rect) == FALSE) {
		gtk_entry_get_icon_area (GTK_ENTRY (priv->entry),
					 GTK_ENTRY_ICON_SECONDARY, &rect);
		gtk_popover_set_pointing_to (priv->popup, &rect);
	}
	gtk_popover_popup (priv->popup);
}

void
ev_zoom_action_set_model (EvZoomAction *zoom_action,
			  EvDocumentModel *model)
{
	EvZoomActionPrivate *priv;

	g_return_if_fail (EV_IS_ZOOM_ACTION (zoom_action));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	priv = GET_PRIVATE (zoom_action);

	g_return_if_fail (priv->model == NULL);
	priv->model = model;

	ev_zoom_action_populate_free_zoom_section (zoom_action);

	g_object_add_weak_pointer (G_OBJECT (priv->model),
				   (gpointer)&priv->model);

	ev_zoom_action_update_zoom_level (zoom_action);

	g_signal_connect_object (priv->model, "notify::document",
				 G_CALLBACK (document_changed_cb),
				 zoom_action, 0);
	g_signal_connect_object (priv->model, "notify::scale",
				 G_CALLBACK (zoom_changed_cb),
				 zoom_action, 0);
	g_signal_connect_object (priv->model, "notify::max-scale",
				 G_CALLBACK (max_zoom_changed_cb),
				 zoom_action, 0);
}

static void
ev_zoom_action_finalize (GObject *object)
{
        EvZoomAction *zoom_action = EV_ZOOM_ACTION (object);
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);

        if (priv->model) {
                g_object_remove_weak_pointer (G_OBJECT (priv->model),
                                              (gpointer)&priv->model);
        }

        g_clear_object (&priv->zoom_free_section);

        G_OBJECT_CLASS (ev_zoom_action_parent_class)->finalize (object);
}

static void
setup_initial_entry_size (EvZoomAction *zoom_action)
{
        gint width;

        width = g_utf8_strlen (zoom_levels[G_N_ELEMENTS (zoom_levels) - 1].name, -1);
        ev_zoom_action_set_width_chars (zoom_action, width);
}

static void
ev_zoom_action_constructed (GObject *object)
{
        EvZoomAction *zoom_action = EV_ZOOM_ACTION (object);
	EvZoomActionPrivate *priv = GET_PRIVATE (zoom_action);

        G_OBJECT_CLASS (ev_zoom_action_parent_class)->constructed (object);


        priv->zoom_free_section =
                g_menu_model_get_item_link (G_MENU_MODEL (priv->menu),
                                            ZOOM_FREE_SECTION, G_MENU_LINK_SECTION);

        setup_initial_entry_size (zoom_action);
}

static void
ev_zoom_action_class_init (EvZoomActionClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

        object_class->finalize = ev_zoom_action_finalize;
        object_class->constructed = ev_zoom_action_constructed;

        gtk_widget_class_set_template_from_resource (widget_class,
                        "/org/gnome/evince/ui/zoom-action.ui");
        gtk_widget_class_bind_template_child_private (widget_class,
                                                      EvZoomAction,
                                                      entry);
	gtk_widget_class_bind_template_child_private (widget_class, EvZoomAction, popup);
	gtk_widget_class_bind_template_child_private (widget_class, EvZoomAction, menu);
	gtk_widget_class_bind_template_callback (widget_class, entry_icon_press_cb);
	gtk_widget_class_bind_template_callback (widget_class, entry_activated_cb);
	gtk_widget_class_bind_template_callback (widget_class, ev_zoom_action_update_zoom_level);

        signals[ACTIVATED] =
                g_signal_new ("activated",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
ev_zoom_action_init (EvZoomAction *zoom_action)
{
        gtk_widget_init_template (GTK_WIDGET (zoom_action));
}
