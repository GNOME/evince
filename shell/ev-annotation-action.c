/* ev-annotation-action.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2020 Germán Poo-Caamaño <gpoo@gnome.org>
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
#include "ev-annotation-action.h"

#include <glib/gi18n.h>
#include <evince-document.h>

enum {
        ACTIVATED,
        BEGIN_ADD_ANNOT,
        CANCEL_ADD_ANNOT,
        LAST_SIGNAL
};

enum
{
        PROP_0,

        PROP_MENU
};

typedef struct {
        GtkWidget       *annot_button;
        GtkWidget       *annot_menu;

        EvAnnotationActionType active_annot_type;
} EvAnnotationActionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EvAnnotationAction, ev_annotation_action, GTK_TYPE_BOX)

#define GET_PRIVATE(o) ev_annotation_action_get_instance_private (o)

static guint signals[LAST_SIGNAL] = { 0 };

static void
ev_annotation_action_annot_button_toggled (GtkWidget          *button,
                                           EvAnnotationAction *annotation_action)
{
        EvAnnotationActionPrivate *priv = GET_PRIVATE (annotation_action);
        EvAnnotationType annot_type;

        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
                g_signal_emit (annotation_action, signals[CANCEL_ADD_ANNOT], 0, NULL);
                return;
        }

	switch (priv->active_annot_type) {
        case EV_ANNOTATION_ACTION_TYPE_NOTE:
                annot_type = EV_ANNOTATION_TYPE_TEXT;
                break;
        case EV_ANNOTATION_ACTION_TYPE_HIGHLIGHT:
                annot_type = EV_ANNOTATION_TYPE_TEXT_MARKUP;
                break;
        default:
                g_assert_not_reached ();
        }

        g_signal_emit (annotation_action, signals[BEGIN_ADD_ANNOT], 0, annot_type);
}

void
ev_annotation_action_select_annotation (EvAnnotationAction     *annotation_action,
                                        EvAnnotationActionType  annot_type)
{
        EvAnnotationActionPrivate *priv;
        const gchar *icon_name = NULL;
        const gchar *tooltip = NULL;
        GtkWidget *image;

        g_return_if_fail (EV_IS_ANNOTATION_ACTION (annotation_action));

        priv = GET_PRIVATE (annotation_action);

        priv->active_annot_type = annot_type;
	switch (annot_type) {
        case EV_ANNOTATION_ACTION_TYPE_NOTE:
		icon_name = "user-invisible-symbolic";
                tooltip = _("Add text annotation");
		break;
        case EV_ANNOTATION_ACTION_TYPE_HIGHLIGHT:
		icon_name = "document-edit-symbolic";
                tooltip = _("Add highlight annotation");
		break;
        }
        image = gtk_image_new_from_icon_name (icon_name,
                                              GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (priv->annot_button), image);
        gtk_widget_set_tooltip_text (priv->annot_button, tooltip);

        g_signal_emit (annotation_action, signals[ACTIVATED], 0, NULL);
}

static void
ev_annotation_action_finalize (GObject *object)
{
        G_OBJECT_CLASS (ev_annotation_action_parent_class)->finalize (object);
}

static void
ev_annotation_action_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum_width,
                                    gint      *natural_width)
{
        *minimum_width = *natural_width = 0;

        GTK_WIDGET_CLASS (ev_annotation_action_parent_class)->get_preferred_width (widget, minimum_width, natural_width);
        *natural_width = *minimum_width;
}

static void
ev_annotation_action_class_init (EvAnnotationActionClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

        object_class->finalize = ev_annotation_action_finalize;

        widget_class->get_preferred_width = ev_annotation_action_get_preferred_width;

        signals[ACTIVATED] =
                g_signal_new ("activated",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[BEGIN_ADD_ANNOT] =
                g_signal_new ("begin-add-annot",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__ENUM,
                              G_TYPE_NONE, 1,
                              EV_TYPE_ANNOTATION_TYPE);
        signals[CANCEL_ADD_ANNOT] =
                g_signal_new ("cancel-add-annot",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0,
                              G_TYPE_NONE);
}

static void
ev_annotation_action_init (EvAnnotationAction *annotation_action)
{
        EvAnnotationActionPrivate *priv = GET_PRIVATE (annotation_action);

        GtkWidget  *button;
        GtkWidget  *image;
        GtkBuilder *builder;
        GMenuModel *menu;
        GtkPopover *popup;
        GtkStyleContext *style_context;

        gtk_orientable_set_orientation (GTK_ORIENTABLE (annotation_action),
                                        GTK_ORIENTATION_HORIZONTAL);

        builder = gtk_builder_new_from_resource ("/org/gnome/evince/gtk/menus.ui");

        style_context = gtk_widget_get_style_context (GTK_WIDGET (annotation_action));
        gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_RAISED);
        gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_LINKED);

        button = gtk_toggle_button_new ();
        image = gtk_image_new_from_icon_name ("user-invisible-symbolic",
                                              GTK_ICON_SIZE_MENU);
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        gtk_button_set_image (GTK_BUTTON (button), image);
        g_signal_connect (button, "toggled",
                          G_CALLBACK (ev_annotation_action_annot_button_toggled),
                          annotation_action);
        priv->annot_button = button;
        priv->active_annot_type = EV_ANNOTATION_ACTION_TYPE_NOTE;
        gtk_box_pack_start (GTK_BOX (annotation_action), priv->annot_button,
                            FALSE, FALSE, 0);
        gtk_widget_show (priv->annot_button);

        button = gtk_menu_button_new ();
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        gtk_widget_set_focus_on_click (button, FALSE);
        gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
        gtk_image_set_from_icon_name (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (button))),
                                      "pan-down-symbolic", GTK_ICON_SIZE_MENU);
        style_context = gtk_widget_get_style_context (button);
        gtk_style_context_add_class (style_context, "image-button");
        gtk_style_context_add_class (style_context, "disclosure-button");
        gtk_box_pack_start (GTK_BOX (annotation_action), button,
                            FALSE, FALSE, 0);
        priv->annot_menu = button;
        menu = G_MENU_MODEL (gtk_builder_get_object (builder, "annotation-menu"));
        gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), menu);
        popup = gtk_menu_button_get_popover (GTK_MENU_BUTTON (button));
        gtk_popover_set_position (popup, GTK_POS_BOTTOM);
        gtk_widget_set_halign (GTK_WIDGET (popup), GTK_ALIGN_END);
        gtk_widget_show (priv->annot_menu);

 }

GtkWidget *
ev_annotation_action_new (void)
{
        return GTK_WIDGET (g_object_new (EV_TYPE_ANNOTATION_ACTION,
                                         NULL));
}

