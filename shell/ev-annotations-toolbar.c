/* ev-annotations-toolbar.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2015 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include "ev-annotations-toolbar.h"
#include <evince-document.h>
#include <glib/gi18n.h>

enum {
        BEGIN_ADD_ANNOT,
        CANCEL_ADD_ANNOT,
        ANNOT_RGBA_SET,
        N_SIGNALS
};

struct _EvAnnotationsToolbar {
	GtkToolbar base_instance;

        GtkWidget *text_button;
        GtkWidget *highlight_button;
        GtkWidget *annot_rgba_button;
        GdkRGBA annot_rgba;

};

struct _EvAnnotationsToolbarClass {
	GtkToolbarClass base_class;

};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (EvAnnotationsToolbar, ev_annotations_toolbar, GTK_TYPE_TOOLBAR)

static void
ev_annotations_toolbar_annot_button_toggled (GtkWidget            *button,
                                             EvAnnotationsToolbar *toolbar)
{
        EvAnnotationType annot_type;

        if (!gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button))) {
                g_signal_emit (toolbar, signals[CANCEL_ADD_ANNOT], 0, NULL);
                return;
        }

        if (button == toolbar->text_button) {
                annot_type = EV_ANNOTATION_TYPE_TEXT;
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (toolbar->highlight_button), FALSE);
        } else if (button == toolbar->highlight_button) {
                annot_type = EV_ANNOTATION_TYPE_TEXT_MARKUP;
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (toolbar->text_button), FALSE);
        } else {
                g_assert_not_reached ();
        }

        g_signal_emit (toolbar, signals[BEGIN_ADD_ANNOT], 0, annot_type);
}

static void
annot_rgba_button_clicked (GtkWidget *button)
{
        GtkColorButton *color_button = GTK_COLOR_BUTTON (gtk_tool_button_get_icon_widget (GTK_TOOL_BUTTON(button)));
        GtkColorButtonClass *color_button_class = GTK_COLOR_BUTTON_GET_CLASS (color_button);
        /* Forward signal to the underlying GtkColorbutton */
        GTK_BUTTON_CLASS (color_button_class)->clicked (GTK_BUTTON(color_button));
}


static void
annot_rgba_set (GtkColorButton *button,
                EvAnnotationsToolbar *toolbar)
{
        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER(button), &(toolbar->annot_rgba));
        g_signal_emit (toolbar, signals[ANNOT_RGBA_SET], 0, &(toolbar->annot_rgba));
}



static GtkColorButton *
ev_annotations_toolbar_get_annot_rgba_button (EvAnnotationsToolbar *toolbar)
{
        return GTK_COLOR_BUTTON(gtk_tool_button_get_icon_widget(GTK_TOOL_BUTTON(toolbar->annot_rgba_button)));
}

void
ev_annotations_toolbar_set_annot_rgba (EvAnnotationsToolbar *toolbar,
                                       const GdkRGBA        *rgba)
{
        GtkColorButton *button = ev_annotations_toolbar_get_annot_rgba_button (toolbar);
        toolbar->annot_rgba.red = rgba->red;
        toolbar->annot_rgba.green = rgba->green;
        toolbar->annot_rgba.blue = rgba->blue;
        toolbar->annot_rgba.alpha = rgba->alpha;
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER(button), &(toolbar->annot_rgba));
}

static gboolean
ev_annotations_toolbar_toggle_button_if_active (EvAnnotationsToolbar *toolbar,
                                                GtkToggleToolButton  *button)
{
        if (!gtk_toggle_tool_button_get_active (button))
                return FALSE;

        g_signal_handlers_block_by_func (button,
                                         ev_annotations_toolbar_annot_button_toggled,
                                         toolbar);
        gtk_toggle_tool_button_set_active (button, FALSE);
        g_signal_handlers_unblock_by_func (button,
                                           ev_annotations_toolbar_annot_button_toggled,
                                           toolbar);

        return TRUE;
}

static GtkWidget *
ev_annotations_toolbar_create_toggle_button (EvAnnotationsToolbar *toolbar,
                                             const gchar          *label,
                                             const gchar          *icon_name,
                                             const gchar          *tooltip)
{
        GtkWidget *button = GTK_WIDGET (gtk_toggle_tool_button_new ());

        gtk_widget_set_tooltip_text (button, tooltip);

        if (label)
                gtk_tool_button_set_label (GTK_TOOL_BUTTON (button), label);

        if (icon_name)
                gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (button), icon_name);

        /* For some reason adding text-button class to the GtkToogleButton makes the button smaller */
        gtk_style_context_add_class (gtk_widget_get_style_context (gtk_bin_get_child (GTK_BIN (button))), "text-button");
        g_signal_connect (button, "toggled",
                          G_CALLBACK (ev_annotations_toolbar_annot_button_toggled),
                          toolbar);

        return button;
}

static void
ev_annotations_toolbar_init (EvAnnotationsToolbar *toolbar)
{
        toolbar->annot_rgba.red = 1.0;
        toolbar->annot_rgba.green = 1.0;
        toolbar->annot_rgba.blue = 0.0;
        toolbar->annot_rgba.alpha = 1.0;
        GtkColorButton *color_button = GTK_COLOR_BUTTON (gtk_color_button_new_with_rgba (&(toolbar->annot_rgba)));
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);

        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_MENU);
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (toolbar)),
                                     GTK_STYLE_CLASS_INLINE_TOOLBAR);

        /* Use Text label until we have:
         *   1. More buttons in the toolbar (lack of space)
         *   2. Clear icons for an action.
         */
        toolbar->text_button = ev_annotations_toolbar_create_toggle_button (toolbar,
                               /* Translators: an annotation that looks like a "sticky note" */
                                                                            _("Note text"),
                                                                            NULL,
                                                                            _("Add text annotation"));
        gtk_container_add (GTK_CONTAINER(toolbar), toolbar->text_button);

        toolbar->highlight_button = ev_annotations_toolbar_create_toggle_button (toolbar,
                                                                                 _("Highlight text"),
                                                                                 NULL,
                                                                                 _("Add highlight annotation"));
        gtk_container_add (GTK_CONTAINER (toolbar), toolbar->highlight_button);

        toolbar->annot_rgba_button = GTK_WIDGET (gtk_tool_button_new (GTK_WIDGET (color_button), NULL));
        g_signal_connect (toolbar->annot_rgba_button, "clicked", G_CALLBACK (annot_rgba_button_clicked), NULL);
        g_signal_connect (gtk_tool_button_get_icon_widget (GTK_TOOL_BUTTON(toolbar->annot_rgba_button)),
                          "color-set",
                          G_CALLBACK (annot_rgba_set),
                          toolbar);
        gtk_container_add (GTK_CONTAINER(toolbar), toolbar->annot_rgba_button);
        gtk_widget_show_all (GTK_WIDGET (toolbar));
}

static void
ev_annotations_toolbar_class_init (EvAnnotationsToolbarClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        signals[BEGIN_ADD_ANNOT] =
                g_signal_new ("begin-add-annot",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__ENUM,
                              G_TYPE_NONE, 1,
                              EV_TYPE_ANNOTATION_TYPE);

        signals[CANCEL_ADD_ANNOT] =
                g_signal_new ("cancel-add-annot",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0,
                              G_TYPE_NONE);
        signals[ANNOT_RGBA_SET] =
                g_signal_new ("annot-rgba-set",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1,
                              G_TYPE_POINTER);
}

GtkWidget *
ev_annotations_toolbar_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_ANNOTATIONS_TOOLBAR, NULL));
}

void
ev_annotations_toolbar_add_annot_finished (EvAnnotationsToolbar *toolbar)
{
        g_return_if_fail (EV_IS_ANNOTATIONS_TOOLBAR (toolbar));

        if (ev_annotations_toolbar_toggle_button_if_active (toolbar, GTK_TOGGLE_TOOL_BUTTON (toolbar->text_button)))
                return;

        ev_annotations_toolbar_toggle_button_if_active (toolbar, GTK_TOGGLE_TOOL_BUTTON (toolbar->highlight_button));
}
